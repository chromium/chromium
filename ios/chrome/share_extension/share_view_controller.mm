// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/share_extension/share_view_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/apple/bundle_locations.h"
#import "base/apple/foundation_util.h"
#import "base/ios/block_types.h"
#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/common/app_group/app_group_command.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/extension_open_url.h"
#import "ios/chrome/share_extension/share_extension_view.h"
#import "ios/chrome/share_extension/ui_util.h"

// Type for completion handler to fetch the components of the share items.
// `idResponse` type depends on the element beeing fetched.
using ItemBlock = void (^)(id idResponse, NSError* error);

namespace {

// Minimum size around the widget
const CGFloat kShareExtensionMargin = 15;
const CGFloat kShareExtensionMaxWidth = 390;
// Clip the last separator out of the table view.
const CGFloat kScreenShotWidth = 100;
const CGFloat kScreenShotHeight = 100;
const CGFloat kMediumAlpha = 0.5;

}  // namespace

@interface ShareViewController () <ShareExtensionViewActionTarget>

@property(nonatomic, weak) UIView* maskView;
@property(nonatomic, weak) ShareExtensionView* shareView;
@property(nonatomic, assign) app_group::ShareExtensionItemType itemType;
@property(nonatomic, strong) NSExtensionItem* shareItem;
@property(nonatomic, strong) NSURL* shareURL;
@property(nonatomic, copy) NSString* shareTitle;
@property(nonatomic, strong) UIImage* image;
// This constrains the center of the widget to be vertically in the center
// of the the screen. It has to be modified for the appearance and dismissal
// animation.
@property(nonatomic, strong)
    NSLayoutConstraint* widgetVerticalPlacementConstraint;

// Creates a files in `app_group::ShareExtensionItemsFolder()` containing a
// serialized NSDictionary.
// If `cancel` is true, `actionType` is ignored.
- (void)queueActionItemURL:(NSURL*)URL
                     title:(NSString*)title
                    action:(app_group::ShareExtensionItemType)actionType
                    cancel:(BOOL)cancel
                completion:(ProceduralBlock)completion;

// Loads all the shared elements from the extension context and update the UI.
- (void)loadElementsFromContext;

// Sets constraints to the widget so that margin are at least
// kShareExtensionMargin points and widget width is closest up to
// kShareExtensionMaxWidth points.
- (void)constrainWidgetWidth;

// Displays the normal share view.
- (void)displayShareView;

// Displays an alert to report an error to the user.
- (void)displayErrorView;

@end

@implementation ShareViewController

@synthesize maskView = _maskView;
@synthesize shareView = _shareView;
@synthesize itemType = _itemType;

+ (void)initialize {
  if (self == [ShareViewController self]) {
    crash_helper::common::StartCrashpad();
  }
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // This view shadows the screen under the share extension.
  UIView* maskView = [[UIView alloc] initWithFrame:CGRectZero];
  self.maskView = maskView;
  // On iOS 13, the default share extension presentation style already has a
  // mask behind the view.
  self.maskView.hidden = YES;
  [self.maskView
      setBackgroundColor:[UIColor colorWithWhite:0 alpha:kMediumAlpha]];
  [self.view addSubview:self.maskView];
  ui_util::ConstrainAllSidesOfViewToView(self.view, self.maskView);

  // This view is the main view of the share extension.
  ShareExtensionView* shareView =
      [[ShareExtensionView alloc] initWithActionTarget:self];
  [self setShareView:shareView];
  [self.view addSubview:self.shareView];

  [self constrainWidgetWidth];

  // Position the widget below the screen. It will be slided up with an
  // animation.
  _widgetVerticalPlacementConstraint =
      [shareView.topAnchor constraintEqualToAnchor:self.view.bottomAnchor];
  [_widgetVerticalPlacementConstraint setActive:YES];
  [[shareView.centerXAnchor constraintEqualToAnchor:self.view.centerXAnchor]
      setActive:YES];

  [self.maskView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [self.shareView setTranslatesAutoresizingMaskIntoConstraints:NO];

  [self loadElementsFromContext];
}

#pragma mark - Private methods

- (void)displayShareView {
  [self.shareView setTitle:_shareTitle];
  [self.shareView setURL:_shareURL];
  if (_image) {
    [self.shareView setScreenshot:_image];
  }
  __weak ShareViewController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    // Center the widget.
    [weakSelf.widgetVerticalPlacementConstraint setActive:NO];
    weakSelf.widgetVerticalPlacementConstraint =
        [weakSelf.shareView.centerYAnchor
            constraintEqualToAnchor:self.view.centerYAnchor];
    [weakSelf.widgetVerticalPlacementConstraint setActive:YES];
    [weakSelf.maskView setAlpha:0];
    [UIView animateWithDuration:ui_util::kAnimationDuration
                     animations:^{
                       [weakSelf.maskView setAlpha:1];
                       [weakSelf.view layoutIfNeeded];
                     }];
  });
}

- (void)displayErrorView {
  __weak ShareViewController* weakSelf = self;
  dispatch_async(dispatch_get_main_queue(), ^{
    [weakSelf displayErrorViewMainThread];
  });
}

- (void)displayErrorViewMainThread {
  NSString* errorMessage =
      NSLocalizedString(@"IDS_IOS_ERROR_MESSAGE_SHARE_EXTENSION",
                        @"The error message to display to the user.");
  NSString* okButton =
      NSLocalizedString(@"IDS_IOS_OK_BUTTON_SHARE_EXTENSION",
                        @"The label of the OK button in share extension.");
  NSString* applicationName = [[base::apple::FrameworkBundle() infoDictionary]
      valueForKey:@"CFBundleDisplayName"];
  errorMessage =
      [errorMessage stringByReplacingOccurrencesOfString:@"APPLICATION_NAME"
                                              withString:applicationName];
  UIAlertController* alert =
      [UIAlertController alertControllerWithTitle:errorMessage
                                          message:[_shareURL absoluteString]
                                   preferredStyle:UIAlertControllerStyleAlert];
  __weak ShareViewController* weakSelf = self;
  UIAlertAction* defaultAction = [UIAlertAction
      actionWithTitle:okButton
                style:UIAlertActionStyleDefault
              handler:^(UIAlertAction* action) {
                NSError* unsupportedURLError =
                    [NSError errorWithDomain:NSURLErrorDomain
                                        code:NSURLErrorUnsupportedURL
                                    userInfo:nil];
                [weakSelf dismissAndReturnItem:nil error:unsupportedURLError];
              }];
  [alert addAction:defaultAction];
  [self presentViewController:alert animated:YES completion:nil];
}

- (void)constrainWidgetWidth {
  // Setting the constraints.
  NSDictionary* views = @{ @"share" : self.shareView };

  NSDictionary* metrics = @{
    @"margin" : @(kShareExtensionMargin),
    @"maxWidth" : @(kShareExtensionMaxWidth),
  };

  NSArray* constraints = @[
    // Sets the margin around the share extension.
    @"H:|-(>=margin)-[share(<=maxWidth)]-(>=margin)-|",
    // If the screen is too small, reduce width of widget.
    @"H:[share(==maxWidth@900)]",
  ];

  for (NSString* constraint : constraints) {
    [NSLayoutConstraint
        activateConstraints:[NSLayoutConstraint
                                constraintsWithVisualFormat:constraint
                                                    options:0
                                                    metrics:metrics
                                                      views:views]];
  }

  // `self.shareView` must be as large as possible and in the center of the
  // screen.
  [self.shareView
      setContentCompressionResistancePriority:UILayoutPriorityDefaultHigh
                                      forAxis:UILayoutConstraintAxisHorizontal];
}

- (void)handleURL:(id)idURL
          forItem:(NSExtensionItem*)item
        withError:(NSError*)error {
  NSURL* URL = base::apple::ObjCCast<NSURL>(idURL);
  if (!URL) {
    [self displayErrorView];
    return;
  }
  self.shareItem = item;
  self.shareURL = URL;
  self.shareTitle = [[item attributedContentText] string];
  if ([self.shareTitle length] == 0) {
    self.shareTitle = [URL host];
  }
  if ([[self.shareURL scheme] isEqualToString:@"http"] ||
      [[self.shareURL scheme] isEqualToString:@"https"]) {
    [self displayShareView];
  } else {
    [self displayErrorView];
  }
}

- (void)handleImage:(id)idImage
            forItem:(NSExtensionItem*)item
          withError:(NSError*)error {
  self.image = base::apple::ObjCCast<UIImage>(idImage);
  if (self.image && self.shareView) {
    [self.shareView setScreenshot:self.image];
  }
}

- (void)loadElementsFromContext {
  NSString* typeURL = UTTypeURL.identifier;
  __weak ShareViewController* weakSelf = self;
  // TODO(crbug.com/40278725): Reorganize sharing extension handler.
  BOOL foundMatch = false;
  for (NSExtensionItem* item in self.extensionContext.inputItems) {
    for (NSItemProvider* itemProvider in item.attachments) {
      if ([itemProvider hasItemConformingToTypeIdentifier:typeURL]) {
        foundMatch = true;
        ItemBlock URLCompletion = ^(id idURL, NSError* error) {
          // Crash reports showed that this block can be called on a background
          // thread. Move back the UI updating code to main thread.
          dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf handleURL:idURL forItem:item withError:error];
          });
        };
        [itemProvider loadItemForTypeIdentifier:typeURL
                                        options:nil
                              completionHandler:URLCompletion];
        NSDictionary* imageOptions = @{
          NSItemProviderPreferredImageSizeKey : [NSValue
              valueWithCGSize:CGSizeMake(kScreenShotWidth, kScreenShotHeight)]
        };
        ItemBlock imageCompletion = ^(id imageData, NSError* error) {
          // Crash reports showed that this block can be called on a background
          // thread. Move back the UI updating code to main thread.
          dispatch_async(dispatch_get_main_queue(), ^{
            [weakSelf handleImage:imageData forItem:item withError:error];
          });
        };
        [itemProvider loadPreviewImageWithOptions:imageOptions
                                completionHandler:imageCompletion];
      }
    }
  }

  // Display the error view when no match have been found.
  if (!foundMatch) {
    [self displayErrorView];
  }
}

- (void)dismissAndReturnItem:(NSExtensionItem*)item error:(NSError*)error {
  // Set the Y placement constraints so the whole extension slides out of the
  // screen.
  // The direction (up or down) is relative to the output (cancel or submit).
  [_widgetVerticalPlacementConstraint setActive:NO];
  if (item) {
    _widgetVerticalPlacementConstraint =
        [_shareView.bottomAnchor constraintEqualToAnchor:self.view.topAnchor];
  } else {
    _widgetVerticalPlacementConstraint =
        [_shareView.topAnchor constraintEqualToAnchor:self.view.bottomAnchor];
  }
  [_widgetVerticalPlacementConstraint setActive:YES];
  __weak ShareViewController* weakSelf = self;
  [UIView animateWithDuration:ui_util::kAnimationDuration
      animations:^{
        [weakSelf.maskView setAlpha:0];
        [weakSelf.view layoutIfNeeded];
      }
      completion:^(BOOL finished) {
        NSArray* returnItem = item ? @[ item ] : @[];
        if (error) {
          [weakSelf.extensionContext cancelRequestWithError:error];
        } else {
          [weakSelf.extensionContext completeRequestReturningItems:returnItem
                                                 completionHandler:nil];
        }
      }];
}

- (void)queueActionItemURL:(NSURL*)URL
                     title:(NSString*)title
                    action:(app_group::ShareExtensionItemType)actionType
                    cancel:(BOOL)cancel
                completion:(ProceduralBlock)completion {
  NSURL* readingListURL = app_group::ExternalCommandsItemsFolder();
  if (![[NSFileManager defaultManager]
          fileExistsAtPath:[readingListURL path]]) {
    [[NSFileManager defaultManager] createDirectoryAtPath:[readingListURL path]
                              withIntermediateDirectories:YES
                                               attributes:nil
                                                    error:nil];
  }
  NSDate* date = [NSDate date];
  NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
  // This format sorts files by alphabetical order.
  [dateFormatter setDateFormat:@"yyyy-MM-dd-HH-mm-ss.SSSSSS"];
  NSTimeZone* timeZone = [NSTimeZone timeZoneWithName:@"UTC"];
  [dateFormatter setTimeZone:timeZone];
  NSString* dateString = [dateFormatter stringFromDate:date];
  NSURL* fileURL =
      [readingListURL URLByAppendingPathComponent:dateString isDirectory:NO];

  NSMutableDictionary* dict = [[NSMutableDictionary alloc] init];
  if (URL)
    [dict setObject:URL forKey:app_group::kShareItemURL];
  if (title)
    [dict setObject:title forKey:app_group::kShareItemTitle];
  [dict setObject:date forKey:app_group::kShareItemDate];
  [dict setObject:app_group::kShareItemSourceShareExtension
           forKey:app_group::kShareItemSource];

  if (!cancel) {
    NSNumber* entryType = [NSNumber numberWithInteger:actionType];
    [dict setObject:entryType forKey:app_group::kShareItemType];
  }

  [dict setValue:[NSNumber numberWithBool:cancel]
          forKey:app_group::kShareItemCancel];
  NSError* error = nil;
  NSData* data = [NSKeyedArchiver archivedDataWithRootObject:dict
                                       requiringSecureCoding:NO
                                                       error:&error];

  if (!data || error) {
    DLOG(WARNING) << "Error serializing data for title: "
                  << base::SysNSStringToUTF8(title)
                  << base::SysNSStringToUTF8([error description]);
    return;
  }

  [[NSFileManager defaultManager] createFileAtPath:[fileURL path]
                                          contents:data
                                        attributes:nil];
  if (completion) {
    completion();
  }
}

#pragma mark - ShareExtensionViewActionTarget

- (void)shareExtensionViewDidSelectCancel:(id)sender {
  __weak ShareViewController* weakSelf = self;
  [self
      queueActionItemURL:nil
                   title:nil
                  action:app_group::READING_LIST_ITEM  // Ignored
                  cancel:YES
              completion:^{
                [weakSelf
                    dismissAndReturnItem:nil
                                   error:
                                       [NSError
                                           errorWithDomain:NSCocoaErrorDomain
                                                      code:NSUserCancelledError
                                                  userInfo:nil]];
              }];
}

- (void)shareExtensionViewDidSelectAddToReadingList:(id)sender {
  __weak ShareViewController* weakSelf = self;
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::READING_LIST_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndReturnItem:weakSelf.shareItem error:nil];
                }];
}

- (void)shareExtensionViewDidSelectAddToBookmarks:(id)sender {
  __weak ShareViewController* weakSelf = self;
  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::BOOKMARK_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndReturnItem:weakSelf.shareItem error:nil];
                }];
}

- (void)shareExtensionViewDidSelectOpenInChrome:(id)sender {
  __weak ShareViewController* weakSelf = self;
  AppGroupCommand* command = [[AppGroupCommand alloc]
      initWithSourceApp:app_group::kOpenCommandSourceShareExtension
         URLOpenerBlock:^(NSURL* openURL) {
           ExtensionOpenURL(openURL, weakSelf, nil);
         }];
  [command prepareToOpenURL:_shareURL];
  [command executeInApp];

  [self queueActionItemURL:_shareURL
                     title:_shareTitle
                    action:app_group::OPEN_IN_CHROME_ITEM
                    cancel:NO
                completion:^{
                  [weakSelf dismissAndReturnItem:weakSelf.shareItem error:nil];
                }];
}

- (void)shareExtensionView:(id)sender
               typeChanged:(app_group::ShareExtensionItemType)type {
  [self setItemType:type];
}

@end
