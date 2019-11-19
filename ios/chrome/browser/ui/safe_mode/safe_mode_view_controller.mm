// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/safe_mode/safe_mode_view_controller.h"

#import <QuartzCore/QuartzCore.h>

#include "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/crash_report/breakpad_helper.h"
#import "ios/chrome/browser/safe_mode/safe_mode_crashing_modules_config.h"
#import "ios/chrome/browser/safe_mode/safe_mode_util.h"
#import "ios/chrome/browser/ui/fancy_ui/primary_action_button.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#include "ios/chrome/grit/ios_chromium_strings.h"
#import "ui/gfx/ios/NSString+CrStringDrawing.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const CGFloat kVerticalSpacing = 20;
const CGFloat kUploadProgressSpacing = 5;
const NSTimeInterval kUploadPumpInterval = 0.1;
const NSTimeInterval kUploadTotalTime = 5;
}  // anonymous namespace

@interface SafeModeViewController ()
// Returns |YES| if any third-party modifications are detected.
+ (BOOL)detectedThirdPartyMods;
// Returns |YES| if there are crash reports to upload.
+ (BOOL)hasReportToUpload;
// Returns a message explaining which, if any, 3rd party modules were detected
// that may cause Chrome to crash.
- (NSString*)startupCrashModuleText;
// Starts timer to update progress bar for crash report upload.
- (void)startUploadProgress;
// Updates progress bar for crash report upload.
- (void)pumpUploadProgress;
// Called when user taps on "Resume Chrome" button. Restores the default
// Breakpad configuration and notifies the delegate to attempt to start the
// browser.
- (void)startBrowserFromSafeMode;
@end

@implementation SafeModeViewController {
  __weak id<SafeModeViewControllerDelegate> delegate_;
  UIView* innerView_;
  PrimaryActionButton* startButton_;
  UILabel* uploadDescription_;
  UIProgressView* uploadProgress_;
  NSDate* uploadStartTime_;
  NSTimer* uploadTimer_;
}

- (id)initWithDelegate:(id<SafeModeViewControllerDelegate>)delegate {
  self = [super init];
  if (self) {
    delegate_ = delegate;
  }
  return self;
}

+ (BOOL)hasSuggestions {
  if ([SafeModeViewController detectedThirdPartyMods])
    return YES;
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
  return breakpad_helper::UserEnabledUploading() &&
         breakpad_helper::GetCrashReportCount() > 1;
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
  center.x = [innerView_ frame].size.width / 2;
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
  if (IsLandscape()) {
    mainBounds.size = CGSizeMake(mainBounds.size.height, mainBounds.size.width);
  }
  UIScrollView* scrollView = [[UIScrollView alloc] initWithFrame:mainBounds];
  self.view = scrollView;
  self.view.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  const CGFloat kIPadInset =
      (mainBounds.size.width - kIPadWidth - kHorizontalSpacing) / 2;
  const CGFloat widthInset = IsIPadIdiom() ? kIPadInset : kHorizontalSpacing;
  innerView_ = [[UIView alloc]
      initWithFrame:CGRectInset(mainBounds, widthInset, kVerticalSpacing * 2)];
  [scrollView addSubview:innerView_];

  UIImage* fatalImage = [[UIImage imageNamed:@"fatal_error.png"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  UIImageView* imageView = [[UIImageView alloc] initWithImage:fatalImage];
  imageView.tintColor = [UIColor colorNamed:kPlaceholderImageTintColor];
  // Shift the image down a bit.
  CGRect imageFrame = [imageView frame];
  imageFrame.origin.y = kVerticalSpacing;
  [imageView setFrame:imageFrame];
  [self centerView:imageView afterView:nil];
  [innerView_ addSubview:imageView];

  UILabel* awSnap = [[UILabel alloc] init];
  [awSnap setText:NSLocalizedString(@"IDS_IOS_SAFE_MODE_AW_SNAP", @"")];
  awSnap.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [awSnap setFont:[UIFont boldSystemFontOfSize:21]];
  [awSnap sizeToFit];
  [self centerView:awSnap afterView:imageView];
  [innerView_ addSubview:awSnap];

  UILabel* description = [[UILabel alloc] init];
  [description setText:[self startupCrashModuleText]];
  description.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [description setTextAlignment:NSTextAlignmentCenter];
  [description setNumberOfLines:0];
  [description setLineBreakMode:NSLineBreakByWordWrapping];
  CGRect frame = [description frame];
  frame.size.width = IsIPadIdiom() ? kIPadWidth : kIPhoneWidth;
  CGSize maxSize = CGSizeMake(frame.size.width, 999999.0f);
  frame.size.height =
      [[description text] cr_boundingSizeWithSize:maxSize
                                             font:[description font]]
          .height;
  [description setFrame:frame];
  [self centerView:description afterView:awSnap];
  [innerView_ addSubview:description];

  startButton_ = [[PrimaryActionButton alloc] init];
  NSString* startText =
      NSLocalizedString(@"IDS_IOS_SAFE_MODE_RELOAD_CHROME", @"");
  [startButton_ setTitle:startText forState:UIControlStateNormal];
  [startButton_ titleLabel].textAlignment = NSTextAlignmentCenter;
  [startButton_ titleLabel].lineBreakMode = NSLineBreakByWordWrapping;
  frame = [startButton_ frame];
  frame.size.width = IsIPadIdiom() ? kIPadWidth : kIPhoneWidth;
  maxSize = CGSizeMake(frame.size.width, 999999.0f);
  const CGFloat kButtonBuffer = kVerticalSpacing / 2;
  CGSize startTextBoundingSize =
      [startText cr_boundingSizeWithSize:maxSize
                                    font:[startButton_ titleLabel].font];
  frame.size.height = startTextBoundingSize.height + kButtonBuffer;
  [startButton_ setFrame:frame];
  [startButton_ addTarget:self
                   action:@selector(startBrowserFromSafeMode)
         forControlEvents:UIControlEventTouchUpInside];
  [self centerView:startButton_ afterView:description];
  [innerView_ addSubview:startButton_];

  UIView* lastView = startButton_;
  if ([SafeModeViewController hasReportToUpload]) {
    breakpad_helper::StartUploadingReportsInRecoveryMode();

    // If there are no jailbreak modifications, then present the "Sending crash
    // report..." UI.
    if (![SafeModeViewController detectedThirdPartyMods]) {
      [startButton_ setEnabled:NO];

      uploadDescription_ = [[UILabel alloc] init];
      [uploadDescription_
          setText:NSLocalizedString(@"IDS_IOS_SAFE_MODE_SENDING_CRASH_REPORT",
                                    @"")];
      [uploadDescription_ setFont:[UIFont systemFontOfSize:13]];
      uploadDescription_.textColor = [UIColor colorNamed:kTextSecondaryColor];
      [uploadDescription_ sizeToFit];
      [self centerView:uploadDescription_ afterView:startButton_];
      [innerView_ addSubview:uploadDescription_];

      uploadProgress_ = [[UIProgressView alloc]
          initWithProgressViewStyle:UIProgressViewStyleDefault];
      [self centerView:uploadProgress_ afterView:nil];
      frame = [uploadProgress_ frame];
      frame.origin.y =
          CGRectGetMaxY([uploadDescription_ frame]) + kUploadProgressSpacing;
      [uploadProgress_ setFrame:frame];
      [innerView_ addSubview:uploadProgress_];

      lastView = uploadProgress_;
      [self startUploadProgress];
    }
  }

  CGSize scrollSize =
      CGSizeMake(mainBounds.size.width,
                 CGRectGetMaxY([lastView frame]) + kVerticalSpacing);
  frame = [innerView_ frame];
  frame.size.height = scrollSize.height;
  [innerView_ setFrame:frame];
  scrollSize.height += frame.origin.y;
  [scrollView setContentSize:scrollSize];
}

- (NSUInteger)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskPortrait;
}

#pragma mark - Private

- (void)startUploadProgress {
  uploadStartTime_ = [NSDate date];
  uploadTimer_ =
      [NSTimer scheduledTimerWithTimeInterval:kUploadPumpInterval
                                       target:self
                                     selector:@selector(pumpUploadProgress)
                                     userInfo:nil
                                      repeats:YES];
}

- (void)pumpUploadProgress {
  NSTimeInterval elapsed =
      [[NSDate date] timeIntervalSinceDate:uploadStartTime_];
  // Theoretically we could stop early when the value returned by
  // breakpad::GetCrashReportCount() changes, but this is simpler. If we decide
  // to look for a change in crash report count, then we also probably want to
  // replace the UIProgressView with a UIActivityIndicatorView.
  if (elapsed <= kUploadTotalTime) {
    [uploadProgress_ setProgress:elapsed / kUploadTotalTime animated:YES];
  } else {
    [uploadTimer_ invalidate];

    [startButton_ setEnabled:YES];
    [uploadDescription_
        setText:NSLocalizedString(@"IDS_IOS_SAFE_MODE_CRASH_REPORT_SENT", @"")];
    [uploadDescription_ sizeToFit];
    [self centerView:uploadDescription_ afterView:startButton_];
    [uploadProgress_ setHidden:YES];
  }
}

- (void)startBrowserFromSafeMode {
  breakpad_helper::RestoreDefaultConfiguration();
  [delegate_ startBrowserFromSafeMode];
}

@end
