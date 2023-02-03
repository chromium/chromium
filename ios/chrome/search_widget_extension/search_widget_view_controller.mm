// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view_controller.h"

#import "base/mac/foundation_util.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"
#import "ios/chrome/common/app_group/app_group_command.h"
#import "ios/chrome/common/app_group/app_group_constants.h"
#import "ios/chrome/common/app_group/app_group_field_trial_version.h"
#import "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/ui/util/constraints_ui_util.h"
#import "ios/chrome/common/ui/util/image_util.h"
#import "ios/chrome/search_widget_extension/copied_content_view.h"
#import "ios/chrome/search_widget_extension/search_widget_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SearchWidgetViewController ()<SearchWidgetViewActionTarget>
@property(nonatomic, weak) SearchWidgetView* widgetView;
@property(nonatomic) CopiedContentType copiedContentType;
@property(nonatomic, strong)
    ClipboardRecentContentImplIOS* clipboardRecentContent;
@property(nonatomic, copy, nullable) NSDictionary* fieldTrialValues;
// Whether the current default search engine supports search by image
@property(nonatomic, assign) BOOL supportsSearchByImage;
@property(nonatomic, strong) AppGroupCommand* command;

@end

@implementation SearchWidgetViewController

+ (void)initialize {
  if (self == [SearchWidgetViewController self]) {
    crash_helper::common::StartCrashpad();
  }
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _clipboardRecentContent = [[ClipboardRecentContentImplIOS alloc]
           initWithMaxAge:1 * 60 * 60
        authorizedSchemes:[NSSet setWithObjects:@"http", @"https", nil]
             userDefaults:app_group::GetGroupUserDefaults()
                 delegate:nil];
    _copiedContentType = CopiedContentTypeNone;
    _command = [[AppGroupCommand alloc]
        initWithSourceApp:app_group::kOpenCommandSourceSearchExtension
           URLOpenerBlock:^(NSURL* openURL) {
             [self.extensionContext openURL:openURL completionHandler:nil];
           }];
  }
  return self;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  DCHECK(self.extensionContext);

  CGFloat height =
      [self.extensionContext
          widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeCompact]
          .height;

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  SearchWidgetView* widgetView =
      [[SearchWidgetView alloc] initWithActionTarget:self compactHeight:height];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];
  [self updateWidget];

  self.extensionContext.widgetLargestAvailableDisplayMode =
      NCWidgetDisplayModeExpanded;

  self.widgetView.translatesAutoresizingMaskIntoConstraints = NO;

  AddSameConstraints(self.view, self.widgetView);
}

- (void)viewWillAppear:(BOOL)animated {
  [super viewWillAppear:animated];
  [self registerWidgetDisplay];
  [self updateWidget];

  // `widgetActiveDisplayMode` does not contain a valid value in viewDidLoad. By
  // the time viewWillAppear is called, it is correct, so set the mode here.
  BOOL initiallyCompact = [self.extensionContext widgetActiveDisplayMode] ==
                          NCWidgetDisplayModeCompact;
  [self.widgetView showMode:initiallyCompact];
}

- (void)widgetPerformUpdateWithCompletionHandler:
    (void (^)(NCUpdateResult))completionHandler {
  [self updateWidgetWithCompletionHandler:^(BOOL updates) {
    completionHandler(updates ? NCUpdateResultNewData : NCUpdateResultNoData);
  }];
}

- (void)updateWidget {
  [self updateWidgetWithCompletionHandler:^(BOOL updates) {
    if (updates && self.extensionContext.widgetActiveDisplayMode ==
                       NCWidgetDisplayModeExpanded) {
      CGSize maxSize = [self.extensionContext
          widgetMaximumSizeForDisplayMode:NCWidgetDisplayModeExpanded];
      self.preferredContentSize =
          CGSizeMake(maxSize.width, [self.widgetView widgetHeight]);
    }
  }];
}

// Updates the widget with latest data from the clipboard. Calls completion
// handler with whether any updates occured..
- (void)updateWidgetWithCompletionHandler:(void (^)(BOOL))completionHandler {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* fieldTrialKey = app_group::kChromeExtensionFieldTrialPreference;
  self.fieldTrialValues = [sharedDefaults dictionaryForKey:fieldTrialKey];

  NSString* supportsSearchByImageKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSupportsSearchByImage);
  self.supportsSearchByImage =
      [sharedDefaults boolForKey:supportsSearchByImageKey];

  NSSet* wantedTypes = [NSSet
      setWithArray:@[ ContentTypeURL, ContentTypeText, ContentTypeImage ]];

  [self.clipboardRecentContent
      hasContentMatchingTypes:wantedTypes
            completionHandler:^(NSSet<ContentType>* matchedTypes) {
              CopiedContentType newType = CopiedContentTypeNone;
              if (self.supportsSearchByImage &&
                  [matchedTypes containsObject:ContentTypeImage]) {
                newType = CopiedContentTypeImage;
              } else if ([matchedTypes containsObject:ContentTypeURL]) {
                newType = CopiedContentTypeURL;
              } else if ([matchedTypes containsObject:ContentTypeText]) {
                newType = CopiedContentTypeString;
              }
              dispatch_async(dispatch_get_main_queue(), ^{
                completionHandler([self updateCopiedContentType:newType]);
              });
            }];
}

- (void)viewWillTransitionToSize:(CGSize)size
       withTransitionCoordinator:
           (id<UIViewControllerTransitionCoordinator>)coordinator {
  [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];

  BOOL isCompact = [self.extensionContext widgetActiveDisplayMode] ==
                   NCWidgetDisplayModeCompact;

  [coordinator
      animateAlongsideTransition:^(
          id<UIViewControllerTransitionCoordinatorContext> _Nonnull context) {
        [self.widgetView showMode:isCompact];
        [self.widgetView layoutIfNeeded];
      }
                      completion:nil];
}

#pragma mark - NCWidgetProviding

- (void)widgetActiveDisplayModeDidChange:(NCWidgetDisplayMode)activeDisplayMode
                         withMaximumSize:(CGSize)maxSize {
  switch (activeDisplayMode) {
    case NCWidgetDisplayModeCompact:
      self.preferredContentSize = maxSize;
      break;
    case NCWidgetDisplayModeExpanded:
      self.preferredContentSize =
          CGSizeMake(maxSize.width, [self.widgetView widgetHeight]);
      break;
  }
}

#pragma mark - SearchWidgetViewActionTarget

- (void)openSearch:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupFocusOmniboxCommand)];
}

- (void)openIncognito:(id)sender {
  [self
      openAppWithCommand:base::SysUTF8ToNSString(
                             app_group::kChromeAppGroupIncognitoSearchCommand)];
}

- (void)openVoice:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupVoiceSearchCommand)];
}

- (void)openQRCode:(id)sender {
  [self openAppWithCommand:base::SysUTF8ToNSString(
                               app_group::kChromeAppGroupQRScannerCommand)];
}

- (void)openCopiedContent:(id)sender {
  switch (self.copiedContentType) {
    case CopiedContentTypeURL: {
      [self.clipboardRecentContent
          recentURLFromClipboardAsync:^(NSURL* copiedURL) {
            if (!copiedURL) {
              return;
            }
            [self.command prepareToOpenURL:copiedURL];
            [self.command executeInApp];
          }];
      break;
    }
    case CopiedContentTypeString: {
      [self.clipboardRecentContent
          recentTextFromClipboardAsync:^(NSString* copiedText) {
            if (!copiedText) {
              return;
            }
            [self.command prepareToSearchText:copiedText];
            [self.command executeInApp];
          }];
      break;
    }
    case CopiedContentTypeImage: {
      [self.clipboardRecentContent
          recentImageFromClipboardAsync:^(UIImage* copiedImage) {
            if (!copiedImage) {
              return;
            }
            // Resize image before converting to NSData so we can store less
            // data.
            UIImage* resizedImage = ResizeImageForSearchByImage(copiedImage);
            [self.command prepareToSearchImage:resizedImage];
            [self.command executeInApp];
          }];
      break;
    }
    case CopiedContentTypeNone:
      NOTREACHED();
      return;
  }
}

#pragma mark - internal

// Opens the main application with the given `command`.
- (void)openAppWithCommand:(NSString*)command {
  [self.command prepareWithCommandID:command];
  [self.command executeInApp];
}

// Register a display of the widget in the app_group NSUserDefaults.
// Metrics on the widget usage will be sent (if enabled) on the next Chrome
// startup.
- (void)registerWidgetDisplay {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSInteger numberOfDisplay =
      [sharedDefaults integerForKey:app_group::kSearchExtensionDisplayCount];
  [sharedDefaults setInteger:numberOfDisplay + 1
                      forKey:app_group::kSearchExtensionDisplayCount];
}

// Sets the copied content type returns YES if the screen needs updating and NO
// otherwise. This must only be called on the main thread.
- (BOOL)updateCopiedContentType:(CopiedContentType)type {
  if (self.copiedContentType == type) {
    return NO;
  }
  self.copiedContentType = type;
  [self.widgetView setCopiedContentType:self.copiedContentType];
  return YES;
}

@end
