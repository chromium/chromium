// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "ios/chrome/browser/omnibox/model/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"
#import "ios/chrome/browser/omnibox/public/omnibox_metrics_helper.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "net/base/apple/url_conversions.h"

@interface OmniboxTextController ()

/// The omnibox client.
@property(nonatomic, assign, readonly) OmniboxClient* client;

@end

@implementation OmniboxTextController {
  /// Controller of the omnibox.
  raw_ptr<OmniboxControllerIOS> _omniboxController;
  /// Controller of the omnibox view.
  raw_ptr<OmniboxViewIOS> _omniboxViewIOS;
  /// Omnibox edit model. Should only be used for text interactions.
  raw_ptr<OmniboxEditModelIOS> _omniboxEditModel;
  /// Whether the popup was scrolled during this omnibox interaction.
  BOOL _suggestionsListScrolled;
  /// Whether it's the lens overlay omnibox.
  BOOL _inLensOverlay;
}

- (instancetype)initWithOmniboxController:
                    (OmniboxControllerIOS*)omniboxController
                           omniboxViewIOS:(OmniboxViewIOS*)omniboxViewIOS
                            inLensOverlay:(BOOL)inLensOverlay {
  self = [super init];
  if (self) {
    _omniboxController = omniboxController;
    _omniboxEditModel = omniboxController->edit_model();
    _omniboxViewIOS = omniboxViewIOS;
    _inLensOverlay = inLensOverlay;
  }
  return self;
}

- (void)disconnect {
  _omniboxController = nullptr;
  _omniboxEditModel = nullptr;
  _omniboxViewIOS = nullptr;
}

- (void)updateAppearance {
  if (!_omniboxEditModel) {
    return;
  }
  // If Siri is thinking, treat that as user input being in progress.  It is
  // unsafe to modify the text field while voice entry is pending.
  if (_omniboxEditModel->ResetDisplayTexts()) {
    if (_omniboxViewIOS) {
      // Revert everything to the baseline look.
      _omniboxViewIOS->RevertAll();
    }
  } else if (!_omniboxEditModel->has_focus()) {
    // Even if the change wasn't "user visible" to the model, it still may be
    // necessary to re-color to the URL string.  Only do this if the omnibox is
    // not currently focused.
    NSAttributedString* as = [[NSMutableAttributedString alloc]
        initWithString:base::SysUTF16ToNSString(
                           _omniboxEditModel->GetPermanentDisplayText())];
    [self.textField setText:as userTextLength:[as length]];
  }
}

- (BOOL)isOmniboxFirstResponder {
  return [self.textField isFirstResponder];
}

- (void)focusOmnibox {
  UITextField* textField = self.textField;
  if ([textField isFirstResponder]) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileOmniboxFocused"));

  // In multiwindow context, -becomeFirstRepsonder is not enough to get the
  // keyboard input. The window will not automatically become key. Make it key
  // manually. UITextField does this under the hood when tapped from
  // -[UITextInteractionAssistant(UITextInteractionAssistant_Internal)
  // setFirstResponderIfNecessaryActivatingSelection:]
  if (base::ios::IsMultipleScenesSupported()) {
    [textField.window makeKeyAndVisible];
  }

  [textField becomeFirstResponder];
  // Ensures that the accessibility system focuses the text field instead of
  // the popup crbug.com/1469173.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  textField);
}

- (void)endEditing {
  [self hideKeyboard];

  if (!_omniboxEditModel || !_omniboxEditModel->has_focus()) {
    return;
  }
  [self.omniboxAutocompleteController closeOmniboxPopup];

  if (OmniboxClient* client = self.client) {
    RecordSuggestionsListScrolled(
        client->GetPageClassification(/*is_prefetch=*/false),
        _suggestionsListScrolled);
  }

  _omniboxEditModel->OnWillKillFocus();
  _omniboxEditModel->OnKillFocus();
  [self.textField exitPreEditState];

  // The controller looks at the current pre-edit state, so the call to
  // OnKillFocus() must come after exiting pre-edit.
  [self.focusDelegate omniboxDidResignFirstResponder];

  // Blow away any in-progress edits.
  if (_omniboxViewIOS) {
    _omniboxViewIOS->RevertAll();
  }
  DCHECK(![self.textField hasAutocompleteText]);
  _suggestionsListScrolled = NO;
}

- (void)insertTextToOmnibox:(NSString*)text {
  [self.textField insertTextWhileEditing:text];
  // The call to `setText` shouldn't be needed, but without it the "Go" button
  // of the keyboard is disabled.
  [self.textField setText:text];
  // Notify the accessibility system to start reading the new contents of the
  // Omnibox.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.textField);
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
    [self.omniboxAutocompleteController closeOmniboxPopup];
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
    if (OmniboxClient* client = self.client;
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
  // If Open from Clipboard offers a suggestion, the popup may be opened when
  // `OnSetFocus` is called on the model. The state of the popup is saved early
  // to ignore that case.
  BOOL popupOpenBeforeEdit = self.omniboxAutocompleteController.hasSuggestions;

  OmniboxTextFieldIOS* textField = self.textField;

  // Make sure the omnibox popup's semantic content attribute is set correctly.
  [self.omniboxAutocompleteController
      setSemanticContentAttribute:[textField bestSemanticContentAttribute]];

  if (_omniboxViewIOS) {
    _omniboxViewIOS->OnBeforePossibleChange();
  }

  if (_omniboxEditModel) {
    _omniboxEditModel->OnSetFocus();

    if (_inLensOverlay) {
      if (textField.userText.length) {
        _omniboxEditModel->SetUserText(textField.userText.cr_UTF16String);
        _omniboxEditModel->StartAutocomplete(
            /*has_selected_text=*/false,
            /*prevent_inline_autocomplete=*/true);
      } else if (OmniboxClient* client = self.client;
                 client &&
                 client->GetPageClassification(/*is_prefetch=*/false) ==
                     metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX) {
        // Zero suggest is only available with LENS_SIDE_PANEL_SEARCHBOX. The
        // lens omnibox should not be in a state where the text is empty and the
        // lens result no thumbnail. (crbug.com/419482108)
        _omniboxEditModel->StartZeroSuggestRequest();
      }
    } else {
      _omniboxEditModel->StartZeroSuggestRequest();
    }
  }

  // If the omnibox is displaying a URL and the popup is not showing, set the
  // field into pre-editing state.  If the omnibox is displaying search terms,
  // leave the default behavior of positioning the cursor at the end of the
  // text.  If the popup is already open, that means that the omnibox is
  // regaining focus after a popup scroll took focus away, so the pre-edit
  // behavior should not be invoked. When `is_lens_overlay_` is true, the
  // omnibox only display search terms.
  if (!popupOpenBeforeEdit && !_inLensOverlay) {
    [textField enterPreEditState];
  }

  // `location_bar_` is only forwarding the call to the BVC. This should only
  // happen when the omnibox is being focused and it starts showing the popup;
  // if the popup was already open, no need to call this.
  if (!popupOpenBeforeEdit) {
    [self.focusDelegate omniboxDidBecomeFirstResponder];
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
  if (_omniboxEditModel) {
    _omniboxEditModel->OnPaste();
  }

  [self.textField exitPreEditState];
}

- (void)onDeleteBackward {
  OmniboxTextFieldIOS* textField = self.textField;
  if (textField.text.length == 0) {
    // If the user taps backspace while the pre-edit text is showing,
    // OnWillChange is invoked before this method and sets the text to an empty
    // string, so use the `clearingPreEditText` to determine if the chip should
    // be cleared or not.
    if ([textField clearingPreEditText]) {
      // In the case where backspace is tapped while in pre-edit mode,
      // OnWillChange is called but OnDidChange is never called so ensure the
      // clearingPreEditText flag is set to false again.
      [textField setClearingPreEditText:NO];
      // Explicitly set the input-in-progress flag. Normally this is set via
      // in model()->OnAfterPossibleChange, but in this case the text has been
      // set to the empty string by OnWillChange so when OnAfterPossibleChange
      // checks if the text has changed it does not see any difference so it
      // never sets the input-in-progress flag.
      if (_omniboxEditModel) {
        _omniboxEditModel->SetInputInProgress(YES);
      }
    }
  }
}

#pragma mark - Omnibox popup event

- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion
            isFirstUpdate:(BOOL)isFirstUpdate {
  // On first update, don't set the preview text, as omnibox will automatically
  // receive the suggestion as inline autocomplete through OmniboxViewIOS.
  if (!isFirstUpdate) {
    [self previewSuggestion:suggestion];
  }

  [self.delegate omniboxTextController:self
                  didPreviewSuggestion:suggestion
                         isFirstUpdate:isFirstUpdate];
}

- (void)onScroll {
  [self hideKeyboard];
  _suggestionsListScrolled = YES;
}

- (void)hideKeyboard {
  // This check is a tentative fix for a crash that happens when calling
  // `resignFirstResponder`. TODO(crbug.com/375429786): Verify the crash rate
  // and remove the comment or check if needed.
  if (self.textField.window) {
    [self.textField resignFirstResponder];
  }
}

- (void)refineWithText:(const std::u16string&)text {
  OmniboxTextFieldIOS* textField = self.textField;
  if (!_omniboxViewIOS) {
    return;
  }
  // Exit preedit state and append the match. Refocus if necessary.
  [textField exitPreEditState];
  _omniboxViewIOS->SetUserText(text);
  _omniboxViewIOS->OnBeforePossibleChange();
  // Calling setText: does not trigger UIControlEventEditingChanged, so
  // trigger that manually.
  [textField sendActionsForControlEvents:UIControlEventEditingChanged];
  [textField becomeFirstResponder];
  if (@available(iOS 17, *)) {
    // Set the caret pos to the end of the text (crbug.com/331622199).
    [self setCaretPos:text.length()];
  }
}

#pragma mark - Private

/// Previews `suggestion` in the Omnibox. Called when a suggestion is
/// highlighted in the popup.
- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion {
  OmniboxTextFieldIOS* textModel = self.textField;
  NSAttributedString* previewText = suggestion.omniboxPreviewText;

  [textModel exitPreEditState];
  [textModel setAdditionalText:nil];
  [textModel setText:previewText userTextLength:previewText.length];
}

/// Updates the appearance of popup to have proper text alignment.
- (void)updatePopupLayoutDirection {
  OmniboxTextFieldIOS* textField = self.textField;
  [self.omniboxAutocompleteController
      setTextAlignment:[textField bestTextAlignment]];
  [self.omniboxAutocompleteController
      setSemanticContentAttribute:[textField bestSemanticContentAttribute]];
}

/// Sets the caret position. Removes any selection. Clamps the requested caret
/// position to the length of the current text.
- (void)setCaretPos:(NSUInteger)caretPos {
  OmniboxTextFieldIOS* textField = self.textField;
  DCHECK(caretPos <= textField.text.length || caretPos == 0);
  UITextPosition* start = textField.beginningOfDocument;
  UITextPosition* newPosition = [textField positionFromPosition:start
                                                         offset:caretPos];
  textField.selectedTextRange = [textField textRangeFromPosition:newPosition
                                                      toPosition:newPosition];
}

/// Updates the autocomplete popup and other state after the text has been
/// changed by the user.
- (void)startAutocompleteAfterEdit {
  if (_omniboxEditModel) {
    _omniboxEditModel->SetInputInProgress(true);
  }

  if (!_omniboxEditModel || !_omniboxEditModel->has_focus() ||
      !_omniboxViewIOS) {
    return;
  }

  OmniboxTextFieldIOS* textField = self.textField;
  // Prevent inline-autocomplete if the IME is currently composing or if the
  // cursor is not at the end of the text.
  const BOOL IMEComposing = [textField markedTextRange] != nil;
  NSRange currentSelection = _omniboxViewIOS->GetCurrentSelection();
  BOOL preventInlineAutocomplete =
      IMEComposing || NSMaxRange(currentSelection) != [textField.text length];
  _omniboxEditModel->StartAutocomplete(currentSelection.length != 0,
                                       preventInlineAutocomplete);

  [self updatePopupLayoutDirection];
}

/// Sets the window text and the caret position. `notifyTextChanged` is true if
/// the model should be notified of the change. Clears the additional text.
- (void)setWindowText:(const std::u16string&)text
             caretPos:(size_t)caretPos
    startAutocomplete:(BOOL)startAutocomplete
    notifyTextChanged:(BOOL)notifyTextChanged {
  OmniboxTextFieldIOS* textField = self.textField;
  // Do not call SetUserText() here, as the user has not triggered this change.
  // Instead, set the field's text directly.
  [textField setText:[NSString cr_fromString16:text]];

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:[NSString cr_fromString16:text]];
  [textField setText:as userTextLength:[as length]];

  if (startAutocomplete) {
    [self startAutocompleteAfterEdit];
  }

  if (notifyTextChanged && _omniboxEditModel) {
    _omniboxEditModel->OnChanged();
  }

  [self setCaretPos:caretPos];
}

/// Updates inline autocomplete if the full text is different.
- (void)updateAutocompleteIfTextChanged:(const std::u16string&)userText
                         autocompletion:
                             (const std::u16string&)inlineAutocomplete {
  std::u16string displayedText = userText + inlineAutocomplete;
  if (displayedText == self.textField.displayedText.cr_UTF16String) {
    return;
  }

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:[NSString cr_fromString16:displayedText]];
  [self.textField setText:as userTextLength:userText.size()];
}

/// Returns the omnibox client.
- (OmniboxClient*)client {
  return _omniboxController ? _omniboxController->client() : nullptr;
}

@end
