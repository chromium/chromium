// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/keyboard_assist/omnibox_assistive_keyboard_mediator.h"

#import "base/apple/foundation_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_constants.h"
#import "ios/chrome/browser/shared/public/commands/application_commands.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"
#import "ios/chrome/browser/ui/lens/lens_entrypoint.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/public/provider/chrome/browser/voice_search/voice_search_api.h"

@implementation OmniboxAssistiveKeyboardMediator

@synthesize layoutGuideCenter = _layoutGuideCenter;
@synthesize lensButton = _lensButton;

#pragma mark - Public

- (void)keyboardAccessoryVoiceSearchTapped:(id)sender {
  if (ios::provider::IsVoiceSearchEnabled()) {
    [self.browserCoordinatorCommandsHandler preloadVoiceSearch];
    base::RecordAction(base::UserMetricsAction("MobileCustomRowVoiceSearch"));
    // Voice Search will query kVoiceSearchButtonGuide to know from where to
    // start its animation, so reference the sender under that name. The sender
    // can be a regular view or a bar button item. Handle both cases.
    UIView* view;
    if ([sender isKindOfClass:[UIView class]]) {
      view = base::apple::ObjCCastStrict<UIView>(sender);
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
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:LensEntrypoint::Keyboard
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  [self.lensCommandsHandler openLensInputSelection:command];
}

- (void)keyboardAccessoryDebuggerTapped {
  CHECK(experimental_flags::IsOmniboxDebuggingEnabled());
  [self.delegate omniboxAssistiveKeyboardDidTapDebuggerButton];
}

- (void)keyPressed:(NSString*)title {
  // Event can happen when the textfield is not editing (crbug.com/349002705).
  if (!self.omniboxTextField.isEditing) {
    return;
  }

  NSString* text = [self updateTextForDotCom:title];
  [self.omniboxTextField insertTextWhileEditing:text];
}

#pragma mark - Private

/// Inserts 'com' without the period if cursor is directly after a period.
- (NSString*)updateTextForDotCom:(NSString*)text {
  if ([text isEqualToString:kDotComTLD]) {
    UITextRange* textRange = [self.omniboxTextField selectedTextRange];
    NSInteger pos = [self.omniboxTextField
        offsetFromPosition:[self.omniboxTextField beginningOfDocument]
                toPosition:textRange.start];
    if (pos > 0 &&
        [[self.omniboxTextField text] characterAtIndex:pos - 1] == '.') {
      return [kDotComTLD substringFromIndex:1];
    }
  }
  return text;
}

@end
