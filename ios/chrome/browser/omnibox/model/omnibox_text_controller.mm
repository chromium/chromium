// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"

#import "base/memory/raw_ptr.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_view.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_text_field_ios.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_view_ios.h"
#import "ios/chrome/common/NSString+Chromium.h"

@implementation OmniboxTextController {
  /// Controller of the omnibox.
  raw_ptr<OmniboxController> _omniboxController;
  /// Controller of the omnibox view.
  raw_ptr<OmniboxViewIOS> _omniboxViewIOS;
  /// Omnibox edit model. Should only be used for text interactions.
  raw_ptr<OmniboxEditModel> _omniboxEditModel;
}

- (instancetype)initWithOmniboxController:(OmniboxController*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS {
  self = [super init];
  if (self) {
    _omniboxController = omniboxController;
    _omniboxEditModel = omniboxController->edit_model();
    _omniboxViewIOS = omniboxViewIOS;
  }
  return self;
}

- (void)disconnect {
  _omniboxController = nullptr;
  _omniboxEditModel = nullptr;
  _omniboxViewIOS = nullptr;
}

#pragma mark - Autocomplete events

- (void)setAdditionalText:(const std::u16string&)text {
  if (!text.length()) {
    self.textField.additionalText = nil;
    return;
  }

  [self.textField setAdditionalText:[NSString cr_fromString16:u" - " + text]];
}

#pragma mark - Omnibox text events

- (void)onUserRemoveAdditionalText {
  [self setAdditionalText:u""];
  if (_omniboxEditModel) {
    _omniboxEditModel->UpdateInput(/*has_selected_text=*/false,
                                   /*prevent_inline_autocomplete=*/true);
  }
}

- (void)onThumbnailSet:(BOOL)hasThumbnail {
  [self.omniboxAutocompleteController setHasThumbnail:hasThumbnail];
}

- (void)onUserRemoveThumbnail {
  // Update the client state.
  if (_omniboxController && _omniboxController->client()) {
    _omniboxController->client()->OnThumbnailRemoved();
  }

  // Update the popup for suggestion wrapping.
  [self.omniboxAutocompleteController setHasThumbnail:NO];

  if (self.textField.userText.length) {
    // If the omnibox is not empty, start autocomplete.
    if (_omniboxEditModel) {
      _omniboxEditModel->UpdateInput(/*has_selected_text=*/false,
                                     /*prevent_inline_autocomplete=*/true);
    }
  } else {
    if (_omniboxViewIOS) {
      _omniboxViewIOS->CloseOmniboxPopup();
    }
  }
}

- (void)clearText {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->ClearText();
  }
}

- (void)acceptInput {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnAccept();
  }
}

- (void)prepareForScribble {
  OmniboxTextFieldIOS* textModel = self.textField;

  if (textModel.isPreEditing) {
    [textModel exitPreEditState];
    [textModel setText:@""];
  }
  [textModel clearAutocompleteText];
}

- (void)cleanupAfterScribble {
  [self.textField clearAutocompleteText];
  [self.textField setAdditionalText:nil];
}

- (void)onDidBeginEditing {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnDidBeginEditing();
  }
}

- (BOOL)shouldChangeCharactersInRange:(NSRange)range
                    replacementString:(NSString*)newText {
  if (_omniboxViewIOS) {
    return _omniboxViewIOS->OnWillChange(range, newText);
  }
  return YES;
}

- (void)textDidChangeWithUserEvent:(BOOL)isProcessingUserEvent {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnDidChange(isProcessingUserEvent);
  }
}

- (void)onAcceptAutocomplete {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnAcceptAutocomplete();
  }
}

- (void)onCopy {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnCopy();
  }
}

- (void)willPaste {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->WillPaste();
  }
}

- (void)onDeleteBackward {
  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnDeleteBackward();
  }
}

#pragma mark - Omnibox popup event

- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion {
  OmniboxTextFieldIOS* textModel = self.textField;
  NSAttributedString* previewText = suggestion.omniboxPreviewText;

  [textModel exitPreEditState];
  [textModel setAdditionalText:nil];
  [textModel setText:previewText userTextLength:previewText.length];
}

@end
