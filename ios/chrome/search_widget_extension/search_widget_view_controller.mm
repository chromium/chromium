// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/search_widget_extension/search_widget_view_controller.h"

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/open_from_clipboard/clipboard_recent_content_impl_ios.h"
#include "ios/chrome/common/app_group/app_group_command.h"
#include "ios/chrome/common/app_group/app_group_constants.h"
#include "ios/chrome/common/app_group/app_group_field_trial_version.h"
#include "ios/chrome/common/app_group/app_group_metrics.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/chrome/common/ui_util/image_util.h"
#import "ios/chrome/search_widget_extension/copied_content_view.h"
#import "ios/chrome/search_widget_extension/search_widget_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SearchWidgetViewController ()<SearchWidgetViewActionTarget>
@property(nonatomic, weak) SearchWidgetView* widgetView;
@property(nonatomic, strong, nullable) NSString* copiedText;
@property(nonatomic, strong, nullable) UIImage* copiedImage;
@property(nonatomic) CopiedContentType copiedContentType;
@property(nonatomic, strong)
    ClipboardRecentContentImplIOS* clipboardRecentContent;
@property(nonatomic, copy, nullable) NSDictionary* fieldTrialValues;
// Whether the current default search engine supports search by image
@property(nonatomic, assign) BOOL supportsSearchByImage;
@property(nonatomic, strong) AppGroupCommand* command;

@end

@implementation SearchWidgetViewController

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

  // |widgetActiveDisplayMode| does not contain a valid value in viewDidLoad. By
  // the time viewWillAppear is called, it is correct, so set the mode here.
  BOOL initiallyCompact = [self.extensionContext widgetActiveDisplayMode] ==
                          NCWidgetDisplayModeCompact;
  [self.widgetView showMode:initiallyCompact];
}

- (void)widgetPerformUpdateWithCompletionHandler:
    (void (^)(NCUpdateResult))completionHandler {
  completionHandler([self updateWidget] ? NCUpdateResultNewData
                                        : NCUpdateResultNoData);
}

// Updates the widget with latest data from the clipboard. Returns whether any
// visual updates occurred.
- (BOOL)updateWidget {
  NSUserDefaults* sharedDefaults = app_group::GetGroupUserDefaults();
  NSString* fieldTrialKey =
      base::SysUTF8ToNSString(app_group::kChromeExtensionFieldTrialPreference);
  self.fieldTrialValues = [sharedDefaults dictionaryForKey:fieldTrialKey];

  NSString* supportsSearchByImageKey =
      base::SysUTF8ToNSString(app_group::kChromeAppGroupSupportsSearchByImage);
  self.supportsSearchByImage =
      [sharedDefaults boolForKey:supportsSearchByImageKey];

  NSString* copiedText;
  UIImage* copiedImage;
  CopiedContentType type = CopiedContentTypeNone;

  if (UIImage* image = [self getCopiedImageFromClipboard]) {
    copiedImage = image;
    type = CopiedContentTypeImage;
  } else if (NSURL* url =
                 [self.clipboardRecentContent recentURLFromClipboard]) {
    copiedText = url.absoluteString;
    type = CopiedContentTypeURL;
  } else if (NSString* text =
                 [self.clipboardRecentContent recentTextFromClipboard]) {
    copiedText = text;
    type = CopiedContentTypeString;
  }

  return [self setCopiedContentType:type
                         copiedText:copiedText
                        copiedImage:copiedImage];
}

// Helper method to encapsulate checking whether the current search engine
// supports search-by-image and getting the copied image.
- (UIImage*)getCopiedImageFromClipboard {
  if (!self.supportsSearchByImage) {
    return nil;
  }
  return [self.clipboardRecentContent recentImageFromClipboard];
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
                         withMaximumSize:(CGSize)maxSize
    API_AVAILABLE(ios(10.0)) {
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
  DCHECK([self verifyCopiedContentType]);
  switch (self.copiedContentType) {
    case CopiedContentTypeURL:
      [self.command prepareToOpenURL:[NSURL URLWithString:self.copiedText]];
      break;
    case CopiedContentTypeString:
      [self.command prepareToSearchText:self.copiedText];
      break;
    case CopiedContentTypeImage: {
      // Resize image before converting to NSData so we can store less data.
      UIImage* resizedImage = ResizeImageForSearchByImage(self.copiedImage);
      [self.command prepareToSearchImage:resizedImage];
      break;
    }
    case CopiedContentTypeNone:
      NOTREACHED();
      return;
  }
  [self.command executeInApp];
}

#pragma mark - internal

// Opens the main application with the given |command|.
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

// Sets the copied content type. |copiedText| should be provided if the content
// type requires textual data, otherwise it should be nil. Likewise,
// |copiedImage| should be provided if the content type requires image data.
// Also saves the data and returns YES if the screen needs updating and NO
// otherwise.
- (BOOL)setCopiedContentType:(CopiedContentType)type
                  copiedText:(NSString*)copiedText
                 copiedImage:(UIImage*)copiedImage {
  if (self.copiedContentType == type &&
      [self.copiedText isEqualToString:copiedText] &&
      [self.copiedImage isEqual:copiedImage]) {
    return NO;
  }
  self.copiedContentType = type;
  self.copiedText = copiedText;
  self.copiedImage = copiedImage;
  [self.widgetView setCopiedContentType:self.copiedContentType
                             copiedText:self.copiedText];
  return YES;
}

// Verifies that the current copied content type has the required data with it.
- (BOOL)verifyCopiedContentType {
  switch (self.copiedContentType) {
    case CopiedContentTypeString:
    case CopiedContentTypeURL:
      return self.copiedText;
    case CopiedContentTypeImage:
      return self.copiedImage;
    case CopiedContentTypeNone:
      return true;
  }
}

@end
