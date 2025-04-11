// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_view.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/public/omnibox_metrics_helper.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_text_field_ios.h"
#import "ios/chrome/browser/omnibox/ui_bundled/omnibox_view_ios.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "net/base/apple/url_conversions.h"

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
  OmniboxTextFieldIOS* textField = self.textField;
  // Ensure omnibox is first responder. This will bring up the keyboard so the
  // user can start typing a new query.
  if (![textField isFirstResponder]) {
    [textField becomeFirstResponder];
  }
  if (textField.text.length != 0) {
    // Remove the text in the omnibox.
    // Calling -[UITextField setText:] does not trigger
    // -[id<UITextFieldDelegate> textDidChange] so it must be called explicitly.
    [textField clearAutocompleteText];
    [textField exitPreEditState];
    [textField setText:@""];
    if (_omniboxViewIOS) {
      _omniboxViewIOS->OnDidChange(/*processing_user_input=*/true);
    }
  }
  // Calling OnDidChange() can trigger a scroll event, which removes focus from
  // the omnibox.
  [textField becomeFirstResponder];
}

- (void)acceptInput {
  RecordAction(base::UserMetricsAction("MobileOmniboxUse"));
  RecordAction(base::UserMetricsAction("IOS.Omnibox.AcceptDefaultSuggestion"));

  if (_omniboxEditModel) {
    // The omnibox edit model doesn't support accepting input with no text.
    // Delegate the call to the client instead.
    if (OmniboxClient* client = [self omniboxClient];
        client && !self.textField.text.length) {
      client->OnThumbnailOnlyAccept();
    } else {
      _omniboxEditModel->OpenSelection();
    }
  }
  if (_omniboxViewIOS) {
    _omniboxViewIOS->RevertAll();
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

- (void)onTextInputModeChange {
  // Update the popup to align suggestions with the text in the textField.
  [self.omniboxAutocompleteController updatePopupSuggestions];
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
  NSString* selectedText = nil;
  NSInteger startLocation = 0;
  OmniboxTextFieldIOS* textField = self.textField;
  if ([textField isPreEditing]) {
    selectedText = textField.text;
    startLocation = 0;
  } else {
    UITextRange* selectedRange = [textField selectedTextRange];
    selectedText = [textField textInRange:selectedRange];
    UITextPosition* start = [textField beginningOfDocument];
    // The following call to `-offsetFromPosition:toPosition:` gives the offset
    // in terms of the number of "visible characters."  The documentation does
    // not specify whether this means glyphs or UTF16 chars.  This does not
    // matter for the current implementation of AdjustTextForCopy(), but it may
    // become an issue at some point.
    startLocation = [textField offsetFromPosition:start
                                       toPosition:[selectedRange start]];
  }
  std::u16string text = selectedText.cr_UTF16String;

  GURL URL;
  bool writeURL = false;
  // Model can be nullptr in tests.
  if (_omniboxEditModel) {
    _omniboxEditModel->AdjustTextForCopy(startLocation, &text, &URL, &writeURL);
  }

  // Create the pasteboard item manually because the pasteboard expects a single
  // item with multiple representations.  This is expressed as a single
  // NSDictionary with multiple keys, one for each representation.
  NSMutableDictionary* item = [NSMutableDictionary dictionaryWithCapacity:2];
  [item setObject:[NSString cr_fromString16:text]
           forKey:UTTypePlainText.identifier];

  using enum OmniboxCopyType;
  if (writeURL && URL.is_valid()) {
    [item setObject:net::NSURLWithGURL(URL) forKey:UTTypeURL.identifier];

    if ([textField isPreEditing]) {
      RecordOmniboxCopy(kPreEditURL);
    } else {
      RecordOmniboxCopy(kEditedURL);
    }
  } else {
    RecordOmniboxCopy(kText);
  }

  StoreItemInPasteboard(item);
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

#pragma mark - Private

- (OmniboxClient*)omniboxClient {
  return _omniboxController ? _omniboxController->client() : nullptr;
}

@end
