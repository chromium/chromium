// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_service_controller.h"

#import <MobileCoreServices/MobileCoreServices.h>

#include "base/logging.h"
#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/send_tab_to_self/target_device_info.h"
#include "ios/chrome/browser/bookmarks/bookmark_model_factory.h"
#import "ios/chrome/browser/passwords/password_form_filler.h"
#import "ios/chrome/browser/send_tab_to_self/send_tab_to_self_util.h"
#include "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/ui/activity_services/activities/bookmark_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/copy_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/find_in_page_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/print_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/reading_list_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/request_desktop_or_mobile_site_activity.h"
#import "ios/chrome/browser/ui/activity_services/activities/send_tab_to_self_activity.h"
#import "ios/chrome/browser/ui/activity_services/activity_type_util.h"
#import "ios/chrome/browser/ui/activity_services/appex_constants.h"
#import "ios/chrome/browser/ui/activity_services/chrome_activity_item_source.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_password.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_positioner.h"
#import "ios/chrome/browser/ui/activity_services/requirements/activity_service_presentation.h"
#import "ios/chrome/browser/ui/activity_services/share_protocol.h"
#import "ios/chrome/browser/ui/activity_services/share_to_data.h"
#import "ios/chrome/browser/ui/commands/snackbar_commands.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/third_party/material_components_ios/src/components/Snackbar/src/MaterialSnackbar.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Snackbar category for activity services.
NSString* const kActivityServicesSnackbarCategory =
    @"ActivityServicesSnackbarCategory";
}  // namespace

@interface ActivityServiceController () {
  BOOL active_;
  __weak id<ActivityServicePassword> passwordProvider_;
  __weak id<ActivityServicePresentation> presentationProvider_;
  UIActivityViewController* activityViewController_;
  __weak id<SnackbarCommands> dispatcher_;
}

// Resets the controller's user interface and delegate.
- (void)resetUserInterface;
// Called when UIActivityViewController user interface is dismissed by user
// signifying the end of the Share/Action activity.
- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError;
// Returns an array of UIActivityItemSource objects to provide the |data| to
// share to the sharing activities.
- (NSArray*)activityItemsForData:(ShareToData*)data;
// Returns an array of UIActivity objects that can handle the given |data|.
- (NSArray*)applicationActivitiesForData:(ShareToData*)data
                              dispatcher:(id<BrowserCommands>)dispatcher
                           bookmarkModel:
                               (bookmarks::BookmarkModel*)bookmarkModel
                        canSendTabToSelf:(BOOL)canSendTabToSelf;
// Processes |extensionItems| returned from App Extension invocation returning
// the |activityType|. Calls shareDelegate_ with the processed returned items
// and |result| of activity. Returns whether caller should reset UI.
- (BOOL)processItemsReturnedFromActivity:(NSString*)activityType
                                  status:(ShareTo::ShareResult)result
                                   items:(NSArray*)extensionItems;
@end

@implementation ActivityServiceController

+ (ActivityServiceController*)sharedInstance {
  static ActivityServiceController* instance =
      [[ActivityServiceController alloc] init];
  return instance;
}

#pragma mark - ShareProtocol

- (BOOL)isActive {
  return active_;
}

- (void)cancelShareAnimated:(BOOL)animated {
  if (!active_) {
    return;
  }
  DCHECK(activityViewController_);
  // There is no guarantee that the completion callback will be called because
  // the |activityViewController_| may have been dismissed already. For example,
  // if the user selects Facebook Share Extension, the UIActivityViewController
  // is first dismissed and then the UI for Facebook Share Extension comes up.
  // At this time, if the user backgrounds Chrome and then relaunch Chrome
  // through an external app (e.g. with googlechrome://url.com), Chrome restart
  // dismisses the modal UI coming through this path. But since the
  // UIActivityViewController has already been dismissed, the following method
  // does nothing and completion callback is not called. The call
  // -shareFinishedWithActivityType:completed:returnedItems:error: must be
  // called explicitly to do the clean up or else future attempts to use
  // Share will fail.
  [activityViewController_ dismissViewControllerAnimated:animated
                                              completion:nil];
  [self shareFinishedWithActivityType:nil
                            completed:NO
                        returnedItems:nil
                                error:nil];
}

- (void)shareWithData:(ShareToData*)data
            browserState:(ios::ChromeBrowserState*)browserState
              dispatcher:(id<BrowserCommands, SnackbarCommands>)dispatcher
        passwordProvider:(id<ActivityServicePassword>)passwordProvider
        positionProvider:(id<ActivityServicePositioner>)positionProvider
    presentationProvider:(id<ActivityServicePresentation>)presentationProvider {
  DCHECK(data);
  DCHECK(!active_);

  CGRect fromRect = CGRectZero;
  UIView* inView = nil;
  if (IsIPadIdiom() && !IsCompactWidth()) {
    DCHECK(positionProvider);
    inView = [positionProvider shareButtonView];
    fromRect = inView.bounds;
    DCHECK(fromRect.size.height);
    DCHECK(fromRect.size.width);
    DCHECK(inView);
  }

  DCHECK(!passwordProvider_);
  DCHECK(!presentationProvider_);
  passwordProvider_ = passwordProvider;
  presentationProvider_ = presentationProvider;

  dispatcher_ = dispatcher;

  bookmarks::BookmarkModel* bookmarkModel =
      ios::BookmarkModelFactory::GetForBrowserState(browserState);

  BOOL canSendTabToSelf =
      send_tab_to_self::ShouldOfferFeature(browserState, data.shareURL);

  DCHECK(!activityViewController_);
  activityViewController_ = [[UIActivityViewController alloc]
      initWithActivityItems:[self activityItemsForData:data]
      applicationActivities:[self
                                applicationActivitiesForData:data
                                                  dispatcher:dispatcher
                                               bookmarkModel:bookmarkModel
                                            canSendTabToSelf:canSendTabToSelf]];

  // Reading List and Print activities refer to iOS' version of these.
  // Chrome-specific implementations of these two activities are provided
  // below in applicationActivitiesForData:dispatcher:bookmarkModel: The
  // "Copy" action is also provided by chrome in order to change its icon.
  NSArray* excludedActivityTypes = @[
    UIActivityTypeAddToReadingList, UIActivityTypeCopyToPasteboard,
    UIActivityTypePrint, UIActivityTypeSaveToCameraRoll
  ];
  [activityViewController_ setExcludedActivityTypes:excludedActivityTypes];

  __weak ActivityServiceController* weakSelf = self;
  [activityViewController_ setCompletionWithItemsHandler:^(
                               NSString* activityType, BOOL completed,
                               NSArray* returnedItems, NSError* activityError) {
    [weakSelf shareFinishedWithActivityType:activityType
                                  completed:completed
                              returnedItems:returnedItems
                                      error:activityError];
  }];

  active_ = YES;
  activityViewController_.modalPresentationStyle = UIModalPresentationPopover;
  activityViewController_.popoverPresentationController.sourceView = inView;
  activityViewController_.popoverPresentationController.sourceRect = fromRect;
  [presentationProvider_
      presentActivityServiceViewController:activityViewController_];
}

#pragma mark - Private

- (void)resetUserInterface {
  passwordProvider_ = nil;
  presentationProvider_ = nil;
  activityViewController_ = nil;
  active_ = NO;
}

- (void)shareFinishedWithActivityType:(NSString*)activityType
                            completed:(BOOL)completed
                        returnedItems:(NSArray*)returnedItems
                                error:(NSError*)activityError {
  DCHECK(active_);
  DCHECK(passwordProvider_);
  DCHECK(presentationProvider_);

  BOOL shouldResetUI = YES;
  if (activityType) {
    ShareTo::ShareResult shareResult = completed
                                           ? ShareTo::ShareResult::SHARE_SUCCESS
                                           : ShareTo::ShareResult::SHARE_CANCEL;
    if (activity_type_util::TypeFromString(activityType) ==
        activity_type_util::APPEX_PASSWORD_MANAGEMENT) {
      // A compatible Password Management App Extension was invoked.
      shouldResetUI = [self processItemsReturnedFromActivity:activityType
                                                      status:shareResult
                                                       items:returnedItems];
    } else {
      activity_type_util::ActivityType type =
          activity_type_util::TypeFromString(activityType);
      activity_type_util::RecordMetricForActivity(type);
      NSString* completionMessage =
          activity_type_util::CompletionMessageForActivity(type);
      [self shareDidComplete:shareResult completionMessage:completionMessage];
    }
  } else {
    [self shareDidComplete:ShareTo::ShareResult::SHARE_CANCEL
         completionMessage:nil];
  }
  if (shouldResetUI)
    [self resetUserInterface];
}

- (NSArray*)activityItemsForData:(ShareToData*)data {
  NSMutableArray* activityItems = [NSMutableArray array];
  // ShareToData object guarantees that there is a sharedNSURL and
  // passwordManagerNSURL.
  DCHECK(data.shareNSURL);
  DCHECK(data.passwordManagerNSURL);

  // In order to support find-login-action protocol, the provider object
  // UIActivityURLSource supports both Password Management App Extensions
  // (e.g. 1Password) and also provide a public.url UTType for Share Extensions
  // (e.g. Facebook, Twitter).
  UIActivityURLSource* loginActionProvider =
      [[UIActivityURLSource alloc] initWithShareURL:data.shareNSURL
                                 passwordManagerURL:data.passwordManagerNSURL
                                            subject:data.title
                                 thumbnailGenerator:data.thumbnailGenerator];
  [activityItems addObject:loginActionProvider];

  return activityItems;
}

- (NSString*)sendTabToSelfContextMenuTitleForDevice:(NSString*)device_name
                                daysSinceLastUpdate:(int)days {
  NSString* active_time = @"";
  if (days == 0) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_TODAY);
  } else if (days == 1) {
    active_time = l10n_util::GetNSString(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAY);
  } else {
    active_time = l10n_util::GetNSStringF(
        IDS_IOS_SEND_TAB_TO_SELF_TARGET_DEVICE_ITEM_SUBTITLE_DAYS,
        base::NumberToString16(days));
  }
  return [NSString stringWithFormat:@"%@ \u2022 %@", device_name, active_time];
}

- (NSArray*)applicationActivitiesForData:(ShareToData*)data
                              dispatcher:(id<BrowserCommands>)dispatcher
                           bookmarkModel:
                               (bookmarks::BookmarkModel*)bookmarkModel
                        canSendTabToSelf:(BOOL)canSendTabToSelf {
  NSMutableArray* applicationActivities = [NSMutableArray array];

  [applicationActivities
      addObject:[[CopyActivity alloc] initWithURL:data.shareURL]];

  if (data.shareURL.SchemeIsHTTPOrHTTPS()) {
    if (canSendTabToSelf) {
      SendTabToSelfActivity* sendTabToSelfActivity =
          [[SendTabToSelfActivity alloc] initWithDispatcher:dispatcher];
      [applicationActivities addObject:sendTabToSelfActivity];
    }

    ReadingListActivity* readingListActivity =
        [[ReadingListActivity alloc] initWithURL:data.shareURL
                                           title:data.title
                                      dispatcher:dispatcher];
    [applicationActivities addObject:readingListActivity];

    if (bookmarkModel) {
      BOOL bookmarked = bookmarkModel->loaded() &&
                        bookmarkModel->IsBookmarked(data.visibleURL);
      BookmarkActivity* bookmarkActivity =
          [[BookmarkActivity alloc] initWithURL:data.visibleURL
                                     bookmarked:bookmarked
                                     dispatcher:dispatcher];
      [applicationActivities addObject:bookmarkActivity];
    }

    if (data.isPageSearchable) {
      FindInPageActivity* findInPageActivity =
          [[FindInPageActivity alloc] initWithDispatcher:dispatcher];
      [applicationActivities addObject:findInPageActivity];
    }

    if (data.userAgent != web::UserAgentType::NONE) {
      RequestDesktopOrMobileSiteActivity* requestActivity =
          [[RequestDesktopOrMobileSiteActivity alloc]
              initWithDispatcher:dispatcher
                       userAgent:data.userAgent];
      [applicationActivities addObject:requestActivity];
    }
  }
  if (data.isPagePrintable) {
    PrintActivity* printActivity = [[PrintActivity alloc] init];
    printActivity.dispatcher = dispatcher;
    [applicationActivities addObject:printActivity];
  }
  return applicationActivities;
}

- (BOOL)processItemsReturnedFromActivity:(NSString*)activityType
                                  status:(ShareTo::ShareResult)result
                                   items:(NSArray*)extensionItems {
  NSItemProvider* itemProvider = nil;
  if ([extensionItems count] > 0) {
    // Based on calling convention described in
    // https://github.com/AgileBits/onepassword-app-extension/blob/master/OnePasswordExtension.m
    // the username/password is always in the first element of the returned
    // item.
    NSExtensionItem* extensionItem = extensionItems[0];
    // Checks that there is at least one attachment and that the attachment
    // is a property list which can be converted into a NSDictionary object.
    // If not, early return.
    if (extensionItem.attachments.count > 0) {
      itemProvider = [extensionItem.attachments objectAtIndex:0];
      if (![itemProvider
              hasItemConformingToTypeIdentifier:(NSString*)kUTTypePropertyList])
        itemProvider = nil;
    }
  }
  if (!itemProvider) {
    // The didFinish method must still be called on incorrect |extensionItems|.
    [self passwordAppExDidFinish:ShareTo::ShareResult::SHARE_ERROR
                        username:nil
                        password:nil
               completionMessage:nil];
    return YES;
  }

  // |completionHandler| is the block that will be executed once the
  // property list has been loaded from the attachment.
  void (^completionHandler)(id, NSError*) = ^(id item, NSError* error) {
    ShareTo::ShareResult activityResult = result;
    NSString* username = nil;
    NSString* password = nil;
    NSString* message = nil;
    NSDictionary* loginDictionary = base::mac::ObjCCast<NSDictionary>(item);
    if (error || !loginDictionary) {
      activityResult = ShareTo::ShareResult::SHARE_ERROR;
    } else {
      username = loginDictionary[activity_services::kPasswordAppExUsernameKey];
      password = loginDictionary[activity_services::kPasswordAppExPasswordKey];
      activity_type_util::ActivityType type =
          activity_type_util::TypeFromString(activityType);
      activity_type_util::RecordMetricForActivity(type);
      message = activity_type_util::CompletionMessageForActivity(type);
    }
    // Password autofill uses JavaScript injection which must be executed on
    // the main thread, however,
    // loadItemForTypeIdentifier:options:completionHandler: documentation states
    // that completion block "may  be executed on a background thread", so the
    // code to do password filling must be re-dispatched back to main thread.
    // Completion block intentionally retains |self|.
    dispatch_async(dispatch_get_main_queue(), ^{
      [self passwordAppExDidFinish:activityResult
                          username:username
                          password:password
                 completionMessage:message];
      // Controller state can be reset only after delegate has
      // processed the item returned from the App Extension.
      [self resetUserInterface];
    });
  };
  [itemProvider loadItemForTypeIdentifier:(NSString*)kUTTypePropertyList
                                  options:nil
                        completionHandler:completionHandler];
  return NO;
}

- (void)passwordAppExDidFinish:(ShareTo::ShareResult)shareStatus
                      username:(NSString*)username
                      password:(NSString*)password
             completionMessage:(NSString*)message {
  switch (shareStatus) {
    case ShareTo::SHARE_SUCCESS: {
      __weak ActivityServiceController* weakSelf = self;
      // Flag to limit user feedback after form filled to just once.
      __block BOOL shown = NO;
      id<PasswordFormFiller> passwordFormFiller =
          [passwordProvider_ currentPasswordFormFiller];
      [passwordFormFiller findAndFillPasswordForms:username
                                          password:password
                                 completionHandler:^(BOOL completed) {
                                   if (shown || !completed || ![message length])
                                     return;
                                   TriggerHapticFeedbackForNotification(
                                       UINotificationFeedbackTypeSuccess);
                                   [weakSelf showSnackbar:message];
                                   shown = YES;
                                 }];
      break;
    }
    default:
      break;
  }
}

- (void)shareDidComplete:(ShareTo::ShareResult)shareStatus
       completionMessage:(NSString*)message {
  // The shareTo dialog dismisses itself instead of through
  // |-dismissViewControllerAnimated:completion:| so we must notify the
  // presentation provider here so that it can clear its presenting state.
  [presentationProvider_ activityServiceDidEndPresenting];

  switch (shareStatus) {
    case ShareTo::SHARE_SUCCESS:
      if ([message length]) {
        TriggerHapticFeedbackForNotification(UINotificationFeedbackTypeSuccess);
        [self showSnackbar:message];
      }
      break;
    case ShareTo::SHARE_ERROR:
      [self showErrorAlert:IDS_IOS_SHARE_TO_ERROR_ALERT_TITLE
                   message:IDS_IOS_SHARE_TO_ERROR_ALERT];
      break;
    case ShareTo::SHARE_NETWORK_FAILURE:
      [self showErrorAlert:IDS_IOS_SHARE_TO_NETWORK_ERROR_ALERT_TITLE
                   message:IDS_IOS_SHARE_TO_NETWORK_ERROR_ALERT];
      break;
    case ShareTo::SHARE_SIGN_IN_FAILURE:
      [self showErrorAlert:IDS_IOS_SHARE_TO_SIGN_IN_ERROR_ALERT_TITLE
                   message:IDS_IOS_SHARE_TO_SIGN_IN_ERROR_ALERT];
      break;
    case ShareTo::SHARE_CANCEL:
      base::RecordAction(base::UserMetricsAction("MobileShareMenuCancel"));
      break;
    case ShareTo::SHARE_UNKNOWN_RESULT:
      break;
  }
}

- (void)showErrorAlert:(int)titleMessageId message:(int)messageId {
  NSString* title = l10n_util::GetNSString(titleMessageId);
  NSString* message = l10n_util::GetNSString(messageId);
  [presentationProvider_ showActivityServiceErrorAlertWithStringTitle:title
                                                              message:message];
}

- (void)showSnackbar:(NSString*)text {
  MDCSnackbarMessage* message = [MDCSnackbarMessage messageWithText:text];
  message.accessibilityLabel = text;
  message.duration = 2.0;
  message.category = kActivityServicesSnackbarCategory;
  [dispatcher_ showSnackbarMessage:message];
}

#pragma mark - For Testing

- (void)setProvidersForTesting:
            (id<ActivityServicePassword, ActivityServicePresentation>)provider
                    dispatcher:(id<SnackbarCommands>)dispatcher {
  passwordProvider_ = provider;
  presentationProvider_ = provider;
  dispatcher_ = dispatcher;
}

@end
