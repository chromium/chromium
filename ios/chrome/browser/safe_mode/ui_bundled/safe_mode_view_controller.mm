// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/safe_mode/ui_bundled/safe_mode_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#import "base/strings/sys_string_conversions.h"
#import "ios/chrome/browser/crash_report/model/crash_helper.h"
#import "ios/chrome/browser/safe_mode/model/safe_mode_crashing_modules_config.h"
#import "ios/chrome/browser/safe_mode/model/safe_mode_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/crash_report/crash_helper.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/util/button_util.h"
#import "ios/chrome/grit/ios_branded_strings.h"
#import "ui/base/device_form_factor.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

namespace {
const CGFloat kVerticalSpacing = 20;
const CGFloat kUploadProgressSpacing = 5;
const NSTimeInterval kUploadPumpInterval = 0.1;
const NSTimeInterval kUploadTotalTime = 5;
}  // anonymous namespace

@interface SafeModeViewController ()
// Returns `YES` if any third-party modifications are detected.
+ (BOOL)detectedThirdPartyMods;
// Returns `YES` if there are crash reports to upload.
+ (BOOL)hasReportToUpload;
// Returns a message explaining which, if any, 3rd party modules were detected
// that may cause Chrome to crash.
- (NSString*)startupCrashModuleText;
// Starts timer to update progress bar for crash report upload.
- (void)startUploadProgress;
// Updates progress bar for crash report upload.
- (void)pumpUploadProgress;
// Called when user taps on "Resume Chrome" button. Notifies the delegate to
// attempt to start the browser.
- (void)startBrowserFromSafeMode;
@end

@implementation SafeModeViewController {
  __weak id<SafeModeViewControllerDelegate> _delegate;
  UIView* _innerView;
  UIButton* _startButton;
  UILabel* _uploadDescription;
  UIProgressView* _uploadProgress;
  NSDate* _uploadStartTime;
  NSTimer* _uploadTimer;
}

- (id)initWithDelegate:(id<SafeModeViewControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    _delegate = delegate;
  }
  return self;
}

+ (BOOL)hasSuggestions {
  if ([SafeModeViewController detectedThirdPartyMods])
    return YES;

  static dispatch_once_t once_token = 0;
  dispatch_once(&once_token, ^{
    crash_helper::ProcessIntermediateReportsForSafeMode();
  });
  return [SafeModeViewController hasReportToUpload];
}

+ (BOOL)detectedThirdPartyMods {
  std::vector<std::string> thirdPartyMods = safe_mode_util::GetLoadedImages(
      "/Library/MobileSubstrate/DynamicLibraries/");
  return (thirdPartyMods.size() > 0);
}

+ (BOOL)hasReportToUpload {
  // If uploading is enabled and more than one report has stacked up, then we
  // assume that the app may be in a state that is preventing crash report
  // uploads before crashing again.
  return crash_helper::common::UserEnabledUploading() &&
         crash_helper::GetPendingCrashReportCount() > 1;
}

// Return any jailbroken library that appears in SafeModeCrashingModulesConfig.
- (NSArray*)startupCrashModules {
  std::vector<std::string> modules = safe_mode_util::GetLoadedImages(
      "/Library/MobileSubstrate/DynamicLibraries/");
  NSMutableArray* array = [NSMutableArray arrayWithCapacity:modules.size()];
  SafeModeCrashingModulesConfig* config =
      [SafeModeCrashingModulesConfig sharedInstance];
  for (size_t i = 0; i < modules.size(); i++) {
    NSString* path = base::SysUTF8ToNSString(modules[i]);
    NSString* friendlyName = [config startupCrashModuleFriendlyName:path];
    if (friendlyName != nil)
      [array addObject:friendlyName];
  }
  return array;
}

// Since we are still supporting iOS5, this is a helper for basic flow layout.
- (void)centerView:(UIView*)view afterView:(UIView*)afterView {
  CGPoint center = [view center];
  center.x = [_innerView frame].size.width / 2;
  [view setCenter:center];

  if (afterView) {
    CGRect frame = view.frame;
    frame.origin.y = CGRectGetMaxY(afterView.frame) + kVerticalSpacing;
    view.frame = frame;
  }
}

- (NSString*)startupCrashModuleText {
  NSArray* knownModules = [self startupCrashModules];
  if ([knownModules count]) {
    NSString* wrongText =
        NSLocalizedString(@"IDS_IOS_SAFE_MODE_NAMED_TWEAKS_FOUND", @"");
    NSMutableString* text = [NSMutableString stringWithString:wrongText];
    [text appendString:@"\n"];
    for (NSString* module in knownModules) {
      [text appendFormat:@"\n    %@", module];
    }
    return text;
  } else if ([SafeModeViewController detectedThirdPartyMods]) {
    return NSLocalizedString(@"IDS_IOS_SAFE_MODE_TWEAKS_FOUND", @"");
  } else {
    return NSLocalizedString(@"IDS_IOS_SAFE_MODE_UNKNOWN_CAUSE", @"");
  }
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Width of the inner view on iPhone.
  const CGFloat kIPhoneWidth = 250;
  // Width of the inner view on iPad.
  const CGFloat kIPadWidth = 350;
  // Horizontal buffer.
  const CGFloat kHorizontalSpacing = 20;

  self.view.autoresizesSubviews = YES;
  CGRect mainBounds = [[UIScreen mainScreen] bounds];
  // SafeModeViewController only supports portrait orientation (see
  // implementation of supportedInterfaceOrientations: below) but if the app is
  // launched from landscape mode (e.g. iPad or iPhone 6+) then the mainScreen's
  // bounds will still be landscape at this point. Swap the height and width
  // here so that the dimensions will be correct once the app rotates to
  // portrait.
  if (IsLandscape(self.view.window)) {
    mainBounds.size = CGSizeMake(mainBounds.size.height, mainBounds.size.width);
  }
  UIScrollView* scrollView = [[UIScrollView alloc] initWithFrame:mainBounds];
  self.view = scrollView;
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  const CGFloat kIPadInset =
      (mainBounds.size.width - kIPadWidth - kHorizontalSpacing) / 2;
  const CGFloat widthInset =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kIPadInset
          : kHorizontalSpacing;
  _innerView = [[UIView alloc]
      initWithFrame:CGRectInset(mainBounds, widthInset, kVerticalSpacing * 2)];
  [scrollView addSubview:_innerView];

  UIImage* fatalImage = [[UIImage imageNamed:@"fatal_error.png"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:fatalImage];
  imageView.tintColor = [UIColor colorNamed:kPlaceholderImageTintColor];
  // Shift the image down a bit.
  CGRect imageFrame = [imageView frame];
  imageFrame.origin.y = kVerticalSpacing;
  [imageView setFrame:imageFrame];
  [self centerView:imageView afterView:nil];
  [_innerView addSubview:imageView];

  UILabel* awSnap = [[UILabel alloc] init];
  [awSnap setText:NSLocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP", @"")];
  awSnap.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [awSnap setFont:[UIFont boldSystemFontOfSize:21]];
  [awSnap sizeToFit];
  [self centerView:awSnap afterView:imageView];
  [_innerView addSubview:awSnap];

  UILabel* description = [[UILabel alloc] init];
  [description setText:[self startupCrashModuleText]];
  description.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [description setTextAlignment:NSTextAlignmentCenter];
  [description setNumberOfLines:0];
  [description setLineBreakMode:NSLineBreakByWordWrapping];
  CGRect frame = [description frame];
  frame.size.width =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kIPadWidth
          : kIPhoneWidth;
  CGSize maxSize = CGSizeMake(frame.size.width, 999999.0f);
  frame.size.height =
      [[description text] cr_boundingSizeWithSize:maxSize
                                             font:[description font]]
          .height;
  [description setFrame:frame];
  [self centerView:description afterView:awSnap];
  [_innerView addSubview:description];

  _startButton = PrimaryActionButton(YES);
  NSString* startText =
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_RELOAD_CHROME", @"");
  SetConfigurationTitle(_startButton, startText);

  UIButtonConfiguration* buttonConfiguration = _startButton.configuration;
  buttonConfiguration.titleAlignment =
      UIButtonConfigurationTitleAlignmentCenter;
  buttonConfiguration.titleLineBreakMode = NSLineBreakByWordWrapping;
  _startButton.configuration = buttonConfiguration;

  frame = [_startButton frame];
  frame.size.width =
      (ui::GetDeviceFormFactor() == ui::DEVICE_FORM_FACTOR_TABLET)
          ? kIPadWidth
          : kIPhoneWidth;
  frame.size.height = _startButton.intrinsicContentSize.height;
  [_startButton setFrame:frame];
  [_startButton addTarget:self
                   action:@selector(startBrowserFromSafeMode)
         forControlEvents:UIControlEventTouchUpInside];
  [self centerView:_startButton afterView:description];
  [_innerView addSubview:_startButton];

  UIView* lastView = _startButton;
  if ([SafeModeViewController hasReportToUpload]) {
    crash_helper::StartUploadingReportsInRecoveryMode();

    // If there are no jailbreak modifications, then present the "Sending crash
    // report..." UI.
    if (![SafeModeViewController detectedThirdPartyMods]) {
      [_startButton setEnabled:NO];

      _uploadDescription = [[UILabel alloc] init];
      [_uploadDescription
          setText:NSLocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT",
                                    @"")];
      [_uploadDescription setFont:[UIFont systemFontOfSize:13]];
      _uploadDescription.textColor = [UIColor colorNamed:kTextSecondaryColor];
      [_uploadDescription sizeToFit];
      [self centerView:_uploadDescription afterView:_startButton];
      [_innerView addSubview:_uploadDescription];

      _uploadProgress = [[UIProgressView alloc]
          initWithProgressViewStyle:UIProgressViewStyleDefault];
      [self centerView:_uploadProgress afterView:nil];
      frame = [_uploadProgress frame];
      frame.origin.y =
          CGRectGetMaxY([_uploadDescription frame]) + kUploadProgressSpacing;
      [_uploadProgress setFrame:frame];
      [_innerView addSubview:_uploadProgress];

      lastView = _uploadProgress;
      [self startUploadProgress];
    }
  }

  CGSize scrollSize =
      CGSizeMake(mainBounds.size.width,
                 CGRectGetMaxY([lastView frame]) + kVerticalSpacing);
  frame = [_innerView frame];
  frame.size.height = scrollSize.height;
  [_innerView setFrame:frame];
  scrollSize.height += frame.origin.y;
  [scrollView setContentSize:scrollSize];
}

- (NSUInteger)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskPortrait;
}

#pragma mark - Private

- (void)startUploadProgress {
  _uploadStartTime = [NSDate date];
  _uploadTimer =
      [NSTimer scheduledTimerWithTimeInterval:kUploadPumpInterval
                                       target:self
                                     selector:@selector(pumpUploadProgress)
                                     userInfo:nil
                                      repeats:YES];
}

- (void)pumpUploadProgress {
  NSTimeInterval elapsed =
      [[NSDate date] timeIntervalSinceDate:_uploadStartTime];
  // Theoretically we could stop early when the value returned by
  // crash_helper::GetCrashReportCount() changes, but this is simpler. If we
  // decide to look for a change in crash report count, then we also probably
  // want to replace the UIProgressView with a UIActivityIndicatorView.
  if (elapsed <= kUploadTotalTime) {
    [_uploadProgress setProgress:elapsed / kUploadTotalTime animated:YES];
  } else {
    [_uploadTimer invalidate];

    [_startButton setEnabled:YES];
    [_uploadDescription
        setText:NSLocalizedString(@"IDS_IOS_SAFE_MODE_CRASH_REPORT_SENT", @"")];
    [_uploadDescription sizeToFit];
    [self centerView:_uploadDescription afterView:_startButton];
    [_uploadProgress setHidden:YES];
  }
}

- (void)startBrowserFromSafeMode {
  [_delegate startBrowserFromSafeMode];
}

@end
