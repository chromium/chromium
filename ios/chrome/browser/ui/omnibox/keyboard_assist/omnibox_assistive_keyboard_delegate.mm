// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_delegate.h"

#import "base/mac/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/commands/application_commands.h"
#import "ios/chrome/browser/ui/commands/browser_commands.h"
#import "ios/chrome/browser/ui/commands/lens_commands.h"
#import "ios/chrome/browser/ui/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/location_bar/location_bar_constants.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/ui/util/util_swift.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation OmniboxAssistiveKeyboardDelegateImpl

@synthesize applicationCommandsHandler = _applicationCommandsHandler;
@synthesize browserCommandsHandler = _browserCommandsHandler;
@synthesize layoutGuideCenter = _layoutGuideCenter;
@synthesize qrScannerCommandsHandler = _qrScannerCommandsHandler;
@synthesize omniboxTextField = _omniboxTextField;

#pragma mark - Public

- (void)keyboardAccessoryVoiceSearchTapped:(id)sender {
  if (ios::provider::IsVoiceSearchEnabled()) {
    [self.browserCommandsHandler preloadVoiceSearch];
    base::RecordAction(base::UserMetricsAction("MobileCustomRowVoiceSearch"));
    // Voice Search will query kVoiceSearchButtonGuide to know from where to
    // start its animation, so reference the sender under that name. The sender
    // can be a regular view or a bar button item. Handle both cases.
    UIView* view;
    if ([sender isKindOfClass:[UIView class]]) {
      view = base::mac::ObjCCastStrict<UIView>(sender);
    } else if ([sender isKindOfClass:[UIBarButtonItem class]]) {
      view = [sender valueForKey:@"view"];
    }
    DCHECK(view);
    [self.layoutGuideCenter referenceView:view
                                underName:kVoiceSearchButtonGuide];
    [self.applicationCommandsHandler startVoiceSearch];
  }
}

- (void)keyboardAccessoryCameraSearchTapped {
  base::RecordAction(base::UserMetricsAction("MobileCustomRowCameraSearch"));
  [self.qrScannerCommandsHandler showQRScanner];
}

- (void)keyboardAccessoryLensTapped {
  base::RecordAction(base::UserMetricsAction("MobileCustomRowLensSearch"));
  [self.lensCommandsHandler
      openInputSelectionForEntrypoint:LensEntrypoint::Keyboard];
}

- (void)keyPressed:(NSString*)title {
  NSString* text = [self updateTextForDotCom:title];
  [self.omniboxTextField insertTextWhileEditing:text];
}

#pragma mark - Private

// Insert 'com' without the period if cursor is directly after a period.
- (NSString*)updateTextForDotCom:(NSString*)text {
  if ([text isEqualToString:kDotComTLD]) {
    UITextRange* textRange = [self.omniboxTextField selectedTextRange];
    NSInteger pos = [self.omniboxTextField
        offsetFromPosition:[self.omniboxTextField beginningOfDocument]
                toPosition:textRange.start];
    if (pos > 0 &&
        [[self.omniboxTextField text] characterAtIndex:pos - 1] == '.')
      return [kDotComTLD substringFromIndex:1];
  }
  return text;
}

@end
