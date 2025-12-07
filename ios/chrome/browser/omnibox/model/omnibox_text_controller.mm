// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "base/ios/ios_util.h"
#import "base/memory/raw_ptr.h"
#import "base/metrics/histogram_macros.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_classifier.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/browser/omnibox_text_util.h"
#import "ios/chrome/browser/omnibox/model/omnibox_autocomplete_controller.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller_delegate.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_model.h"
#import "ios/chrome/browser/omnibox/model/suggestions/autocomplete_suggestion.h"
#import "ios/chrome/browser/omnibox/public/omnibox_metrics_helper.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_input_delegate.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/common/NSString+Chromium.h"
#import "net/base/apple/url_conversions.h"

namespace {

const char kOmniboxFocusResultedInNavigation[] =
    "Omnibox.focus_resulted_in_navigation";

}  // namespace

@implementation OmniboxTextController {
  /// Client of the omnibox.
  raw_ptr<OmniboxClient, DanglingUntriaged> _omniboxClient;
  /// Whether the popup was scrolled during this omnibox interaction.
  BOOL _suggestionsListScrolled;
  /// The omnbibox text model, holding the text state.
  raw_ptr<OmniboxTextModel, DanglingUntriaged> _omniboxTextModel;
  /// The context in which the omnibox is presented.
  OmniboxPresentationContext _presentationContext;
  /// The previous omnibox text state.
  OmniboxTextState _stateBeforeChange;
  /// The marked text before the change.
  NSString* _markedTextBeforeChange;
  /// The current text selection.
  NSRange _currentSelection;
  /// The previous text selection.
  NSRange _oldSelection;
}

- (instancetype)initWithOmniboxClient:(OmniboxClient*)omniboxClient
                     omniboxTextModel:(OmniboxTextModel*)omniboxTextModel
                  presentationContext:
                      (OmniboxPresentationContext)presentationContext {
  self = [super init];
  if (self) {
    _omniboxClient = omniboxClient;
    _omniboxTextModel = omniboxTextModel;
    _presentationContext = presentationContext;
    _currentSelection = NSMakeRange(0, 0);
    _oldSelection = NSMakeRange(0, 0);
  }
  return self;
}

- (void)disconnect {
  _omniboxClient = nullptr;
  _omniboxTextModel = nullptr;
}

- (void)updateAppearance {
  // If Siri is thinking, treat that as user input being in progress.  It is
  // unsafe to modify the text input while voice entry is pending.
  if ([self resetDisplayTexts]) {
    // Revert everything to the baseline look.
    [self revertAll];
  } else if (_omniboxTextModel && !_omniboxTextModel->HasFocus()) {
    // Even if the change wasn't "user visible" to the model, it still may be
    // necessary to re-color to the URL string.  Only do this if the omnibox is
    // not currently focused.
    NSAttributedString* as = [[NSMutableAttributedString alloc]
        initWithString:base::SysUTF16ToNSString(
                           _omniboxTextModel->url_for_editing)];
    [self.textInput setText:as userTextLength:[as length]];
  }
}

- (BOOL)isOmniboxFirstResponder {
  return [self.textInput.view isFirstResponder];
}

- (void)focusOmnibox {
  id<OmniboxTextInput> textInput = self.textInput;
  if ([self isOmniboxFirstResponder]) {
    return;
  }
  base::RecordAction(base::UserMetricsAction("MobileOmniboxFocused"));

  // In multiwindow context, -becomeFirstRepsonder is not enough to get the
  // keyboard input. The window will not automatically become key. Make it key
  // manually. UITextField does this under the hood when tapped from
  // -[UITextInteractionAssistant(UITextInteractionAssistant_Internal)
  // setFirstResponderIfNecessaryActivatingSelection:]
  if (base::ios::IsMultipleScenesSupported()) {
    [textInput.view.window makeKeyAndVisible];
  }

  [textInput.view becomeFirstResponder];
  // Ensures that the accessibility system focuses the text input instead of
  // the popup crbug.com/1469173.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  textInput);
}

- (void)endEditing {
  [self hideKeyboard];

  if (!_omniboxTextModel || !_omniboxTextModel->HasFocus()) {
    return;
  }
  [self.omniboxAutocompleteController closeOmniboxPopup];

  if (_omniboxClient) {
    RecordSuggestionsListScrolled(
        _omniboxClient->GetPageClassification(/*is_prefetch=*/false),
        _suggestionsListScrolled);
  }

  if ((_omniboxTextModel->user_input_in_progress ||
       !_omniboxTextModel->in_revert) &&
      _omniboxClient) {
    _omniboxClient->OnInputStateChanged();
  }

  UMA_HISTOGRAM_BOOLEAN(kOmniboxFocusResultedInNavigation,
                        _omniboxTextModel->focus_resulted_in_navigation);
  if (_omniboxTextModel->HasFocus()) {
    _omniboxTextModel->KillFocus();
  }

  // Skip exit pre edit here to avoid resizing the multiline omnibox on exit.
  // Exiting pre edit also shows the selections handle when animating the
  // defocus (crbug.com/458055336).
  BOOL skipExitPreEdit =
      (IsMultilineBrowserOmniboxEnabled() &&
       _presentationContext == OmniboxPresentationContext::kLocationBar) ||
      (IsComposeboxIOSEnabled() &&
       _presentationContext == OmniboxPresentationContext::kComposebox);
  ;
  if (!skipExitPreEdit) {
    [self.textInput exitPreEditState];
  }

  // The controller looks at the current pre-edit state, so the call to
  // OnKillFocus() must come after exiting pre-edit.
  [self.focusDelegate omniboxDidResignFirstResponder];

  // Composebox is destroyed on endEditing, skip revert to avoid resizing on
  // revert.
  if (_presentationContext != OmniboxPresentationContext::kComposebox) {
    // Blow away any in-progress edits.
    [self revertAll];
    DCHECK(![self.textInput hasAutocompleteText]);
  }
  _suggestionsListScrolled = NO;
}

- (void)insertTextToOmnibox:(NSString*)text {
  [self.textInput insertTextWhileEditing:text];
  // The call to `setText` shouldn't be needed, but without it the "Go" button
  // of the keyboard is disabled.
  [self.textInput setText:text];
  // Notify the accessibility system to start reading the new contents of the
  // Omnibox.
  UIAccessibilityPostNotification(UIAccessibilityScreenChangedNotification,
                                  self.textInput.view);
}

// Notifies the client about input changes.
- (void)notifyClientOnUserInputInProgressChange:(BOOL)changedToUserInProgress {
  if (changedToUserInProgress && _omniboxClient) {
    _omniboxClient->OnInputInProgress(true);

    if (_omniboxTextModel->user_input_in_progress ||
        !_omniboxTextModel->in_revert) {
      _omniboxClient->OnInputStateChanged();
    }
  }
}

- (void)getSelectionBounds:(size_t*)start end:(size_t*)end {
  if ([self isOmniboxFirstResponder]) {
    NSRange selectedRange = [self.textInput selectedNSRange];
    *start = selectedRange.location;
    *end = selectedRange.location + self.textInput.autocompleteText.length;
  } else {
    *start = *end = 0;
  }
}

- (void)revertAll {
  [self revertState];
  // This will stop the `AutocompleteController`. This should happen after
  // `user_input_in_progress_` is cleared above; otherwise, closing the popup
  // will trigger unnecessary `AutocompleteClassifier::Classify()` calls to
  // try to update the views which are unnecessary since they'll be thrown
  // away during the model revert anyways.
  [self.omniboxAutocompleteController stopAutocompleteWithClearSuggestions:YES];

  [self onTextChanged];
}

- (std::u16string)displayedText {
  return base::SysNSStringToUTF16([self.textInput displayedText]);
}

- (void)setInputInProgress:(BOOL)inProgress {
  if (!_omniboxTextModel) {
    return;
  }

  if (_omniboxTextModel->SetInputInProgressNoNotify(inProgress)) {
    if (_omniboxTextModel->user_input_in_progress) {
      [self.omniboxAutocompleteController resetSession];
    }
    [self notifyClientOnUserInputInProgressChange:inProgress];
  }
}

- (void)revertState {
  if (!_omniboxTextModel) {
    return;
  }

  [self setInputInProgress:NO];
  _omniboxTextModel->input.Clear();
  _omniboxTextModel->paste_state = OmniboxPasteState::kNone;
  _omniboxTextModel->UpdateUserText(std::u16string());
  size_t start, end;
  [self getSelectionBounds:&start end:&end];
  _omniboxTextModel->current_match = AutocompleteMatch();
  // First home the cursor, so view of text is scrolled to left, then correct
  // it. `SetCaretPos()` doesn't scroll the text, so doing that first wouldn't
  // accomplish anything.
  std::u16string current_permanent_url = _omniboxTextModel->url_for_editing;

  [self setWindowText:current_permanent_url
               caretPos:0
      startAutocomplete:false
      notifyTextChanged:true];
  [self setCaretPos:std::min(current_permanent_url.length(), start)];

  _omniboxClient->OnRevert();
}

- (void)getInfoForCurrentText:(AutocompleteMatch*)match
       alternateNavigationURL:(GURL*)alternateNavigationURL {
  if (!_omniboxTextModel || !_omniboxClient) {
    return;
  }
  DCHECK(match);

  BOOL foundMatch = [self.omniboxAutocompleteController
           findMatchForInput:_omniboxTextModel->input
                       match:match
      alternateNavigationURL:alternateNavigationURL];

  if (!foundMatch) {
    // For match generation, we use the unelided `url_for_editing_`, unless the
    // user input is in progress.
    std::u16string textForMatchGeneration =
        _omniboxTextModel->user_input_in_progress
            ? _omniboxTextModel->user_text
            : _omniboxTextModel->url_for_editing;

    _omniboxClient->GetAutocompleteClassifier()->Classify(
        textForMatchGeneration, false, true,
        _omniboxClient->GetPageClassification(
            /*is_prefetch=*/false),
        match, alternateNavigationURL);
  }
}

- (void)setUserText:(const std::u16string&)text {
  if (!_omniboxTextModel || !_omniboxClient) {
    return;
  }
  [self setInputInProgress:YES];
  _omniboxTextModel->UpdateUserText(text);
  [self getInfoForCurrentText:&_omniboxTextModel->current_match
       alternateNavigationURL:nullptr];
  _omniboxTextModel->paste_state = OmniboxPasteState::kNone;
}

- (AutocompleteMatch)currentMatch:(GURL*)alternateNavURL {
  if (!_omniboxTextModel) {
    return AutocompleteMatch();
  }
  // If we have a valid match use it. Otherwise get one for the current text.
  AutocompleteMatch match = _omniboxTextModel->current_match;
  if (!match.destination_url.is_valid()) {
    [self getInfoForCurrentText:&match alternateNavigationURL:alternateNavURL];
  } else if (alternateNavURL) {
    *alternateNavURL = [self.omniboxAutocompleteController
        computeAlternateNavURLForInput:_omniboxTextModel->input
                                 match:match];
  }
  return match;
}

- (void)onTextChanged {
  if (!_omniboxTextModel || !_omniboxClient) {
    return;
  }

  // Don't call CurrentMatch() when there's no editing, as in this case we'll
  // never actually use it.  This avoids running the autocomplete providers (and
  // any systems they then spin up) during startup.
  const AutocompleteMatch& current_match =
      _omniboxTextModel->user_input_in_progress ? [self currentMatch:nullptr]
                                                : AutocompleteMatch();

  if (const AutocompleteResult* result =
          [self.omniboxAutocompleteController autocompleteResult]) {
    _omniboxClient->OnTextChanged(
        current_match, _omniboxTextModel->user_input_in_progress,
        _omniboxTextModel->user_text, *result, _omniboxTextModel->HasFocus());
  }
}

- (void)onPopupDataChanged:(const std::u16string&)inlineAutocompletion
            additionalText:(const std::u16string&)additionalText
                  newMatch:(const AutocompleteMatch&)newMatch {
  if (!_omniboxTextModel) {
    return;
  }
  _omniboxTextModel->current_match = newMatch;
  _omniboxTextModel->inline_autocompletion = inlineAutocompletion;

  const std::u16string& userText = _omniboxTextModel->user_input_in_progress
                                       ? _omniboxTextModel->user_text
                                       : _omniboxTextModel->input.text();

  [self
      updateAutocompleteIfTextChanged:userText
                       autocompletion:_omniboxTextModel->inline_autocompletion];
  [self setAdditionalText:additionalText];

  // We need to invoke this in case the destination url changed (as could
  // happen when control is toggled).
  [self onTextChanged];
}

- (bool)resetDisplayTexts {
  if (!_omniboxTextModel) {
    return false;
  }
  const std::u16string old_display_text = _omniboxTextModel->url_for_editing;
  if (_omniboxClient) {
    _omniboxTextModel->url_for_editing = _omniboxClient->GetFormattedFullURL();
  }
  // When there's new permanent text, and the user isn't interacting with the
  // omnibox, we want to revert the edit to show the new text.  We could simply
  // define "interacting" as "the omnibox has focus", but we still allow updates
  // when the omnibox has focus as long as the user hasn't begun editing, and
  // isn't seeing zerosuggestions (because changing this text would require
  // changing or hiding those suggestions).  When the omnibox doesn't have
  // focus, we assume the user may have abandoned their interaction and it's
  // always safe to change the text; this also prevents someone toggling "Show
  // URL" (which sounds as if it might be persistent) from seeing just that URL
  // forever afterwards.
  return (_omniboxTextModel->url_for_editing != old_display_text) &&
         (!_omniboxTextModel->HasFocus() ||
          (!_omniboxTextModel->user_input_in_progress &&
           !_omniboxAutocompleteController.hasSuggestions));
}

- (void)removePreEditText {
  if (self.textInput.isPreEditing) {
    [self.textInput exitPreEditState];
    [self.textInput setText:@""];
    [self setUserText:u""];
    [self onTextChanged];
  }
}

#pragma mark - Autocomplete events

- (void)setAdditionalText:(const std::u16string&)text {
  if (!text.length()) {
    [self.textInput setAdditionalText:nil];
    return;
  }

  [self.textInput setAdditionalText:[NSString cr_fromString16:u" - " + text]];
}

#pragma mark - Omnibox text events

- (void)onUserRemoveAdditionalText {
  [self setAdditionalText:u""];
  [self updateInput];
}

- (void)onThumbnailSet:(BOOL)hasThumbnail {
  [self.omniboxAutocompleteController setHasThumbnail:hasThumbnail];
}

- (void)onUserRemoveThumbnail {
  // Update the client state.
  if (_omniboxClient) {
    _omniboxClient->OnThumbnailRemoved();
  }

  // Update the popup for suggestion wrapping.
  [self.omniboxAutocompleteController setHasThumbnail:NO];

  if (self.textInput.userText.length) {
    // If the omnibox is not empty, start autocomplete.
    [self updateInput];
  } else {
    [self.omniboxAutocompleteController closeOmniboxPopup];
  }
}

- (void)clearText {
  id<OmniboxTextInput> textInput = self.textInput;
  // Ensure omnibox is first responder. This will bring up the keyboard so the
  // user can start typing a new query.
  if (![self isOmniboxFirstResponder]) {
    [textInput.view becomeFirstResponder];
  }
  if (textInput.text.length != 0) {
    // Remove the text in the omnibox.
    // Calling `setText:` does not trigger `textDidChange:` so it must be called
    // explicitly.
    [textInput clearAutocompleteText];
    textInput.clearingPreEditText = YES;
    [textInput exitPreEditState];
    textInput.clearingPreEditText = NO;
    [textInput setText:@""];
    [self textDidChangeWithUserEvent:YES];
  }
  // Calling textDidChangeWithUserEvent can trigger a scroll event, which
  // removes focus from the omnibox.
  [textInput.view becomeFirstResponder];
}

- (void)acceptInput {
  RecordAction(base::UserMetricsAction("MobileOmniboxUse"));
  RecordAction(base::UserMetricsAction("IOS.Omnibox.AcceptDefaultSuggestion"));

  // The omnibox edit model doesn't support accepting input with no text.
  // Delegate the call to the client instead.
  if (_omniboxClient && !self.textInput.text.length) {
    _omniboxClient->OnThumbnailOnlyAccept();
  } else {
    [self.omniboxAutocompleteController
        openCurrentSelectionWithDisposition:WindowOpenDisposition::CURRENT_TAB
                                  timestamp:base::TimeTicks()];
  }

  // Composebox UI is destroyed on accept, skip revert to avoid UI resizing.
  if (_presentationContext != OmniboxPresentationContext::kComposebox) {
    [self revertAll];
  }
}

- (void)prepareForScribble {
  id<OmniboxTextInput> textInput = self.textInput;

  if (textInput.isPreEditing) {
    [textInput exitPreEditState];
    [textInput setText:@""];
  }
  [textInput clearAutocompleteText];
}

- (void)cleanupAfterScribble {
  [self.textInput clearAutocompleteText];
  [self.textInput setAdditionalText:nil];
}

- (void)onTextInputModeChange {
  [self updatePopupLayoutDirection];
  [self.omniboxAutocompleteController updatePopupSuggestions];
}

- (void)onDidBeginEditing {
  // If Open from Clipboard offers a suggestion, the popup may be opened when
  // `OnSetFocus` is called on the model. The state of the popup is saved early
  // to ignore that case.
  BOOL popupOpenBeforeEdit = self.omniboxAutocompleteController.hasSuggestions;

  id<OmniboxTextInput> textInput = self.textInput;

  // Make sure the omnibox popup's semantic content attribute is set correctly.
  [self.omniboxAutocompleteController
      setSemanticContentAttribute:[textInput bestSemanticContentAttribute]];

  [self onBeforePossibleChange];

  if (_omniboxTextModel) {
    _omniboxTextModel->OnSetFocus();

    if (_presentationContext == OmniboxPresentationContext::kLensOverlay) {
      if (textInput.userText.length) {
        [self setUserText:textInput.userText.cr_UTF16String];
        [self startAutocompletePreventingInline:YES];
      } else if (_omniboxClient &&
                 _omniboxClient->GetPageClassification(/*is_prefetch=*/false) ==
                     metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX) {
        // Zero suggest is only available with LENS_SIDE_PANEL_SEARCHBOX. The
        // lens omnibox should not be in a state where the text is empty and the
        // lens result no thumbnail. (crbug.com/419482108)
        [_omniboxAutocompleteController
            startZeroSuggestRequestWithText:textInput.displayedText
                                                .cr_UTF16String
                              userClobbered:NO];
      }
    } else {
      [_omniboxAutocompleteController
          startZeroSuggestRequestWithText:textInput.displayedText.cr_UTF16String
                            userClobbered:NO];
    }
  }

  // If the omnibox is displaying a URL and the popup is not showing, set the
  // input into pre-editing state.  If the omnibox is displaying search terms,
  // leave the default behavior of positioning the cursor at the end of the
  // text.  If the popup is already open, that means that the omnibox is
  // regaining focus after a popup scroll took focus away, so the pre-edit
  // behavior should not be invoked. When `is_lens_overlay_` is true, the
  // omnibox only display search terms.
  if (!popupOpenBeforeEdit &&
      _presentationContext != OmniboxPresentationContext::kLensOverlay) {
    [textInput enterPreEditState];
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
  BOOL shouldChange = YES;

  id<OmniboxTextInput> textInput = self.textInput;

  if ([textInput isPreEditing]) {
    [textInput setClearingPreEditText:YES];
    [textInput exitPreEditState];
    // Reset `range` to be of zero-length at location zero, as the input will be
    // now cleared.
    range = NSMakeRange(0, 0);
  }

  // Figure out the old and current (new) selections. Assume the new selection
  // will be of zero-length, located at the end of `newText`.
  NSRange oldRange = range;
  NSRange newRange = NSMakeRange(range.location + [newText length], 0);

  // We may need to fix up the old and new ranges in the case where autocomplete
  // text was showing. If there is autocomplete text, assume it was selected.
  // If the change is deleting one character from the end of the actual text,
  // disallow the change, but clear the autocomplete text and call
  // textDidChangeWithUserEvent directly. If there is autocomplete text AND a
  // text input selection, or if the user entered multiple characters, clear the
  // autocomplete text and pretend it never existed.
  if ([textInput hasAutocompleteText]) {
    BOOL addingText = (range.length < [newText length]);
    BOOL deletingText = (range.length > [newText length]);

    if (addingText) {
      // TODO(crbug.com/379695322): What about cases where [newText length] >
      // 1?  This could happen if an IME completion inserts multiple characters
      // at once, or if the user pastes some text in. Let's loosen this test to
      // allow multiple characters, as long as the "old range" ends at the end
      // of the permanent text.
      NSString* userText = textInput.userText;
      if (newText.length == 1 && range.location == userText.length) {
        oldRange =
            NSMakeRange(userText.length, textInput.autocompleteText.length);
      }
    } else if (deletingText) {
      NSString* userText = textInput.userText;
      if ([newText length] == 0 && range.location == [userText length] - 1) {
        shouldChange = NO;
      }
    }
  }

  _oldSelection = oldRange;
  _currentSelection = newRange;

  // Store the displayed text state before the change.
  [self getState:&_stateBeforeChange];
  // Manually update the selection state after calling GetState().
  _stateBeforeChange.sel_start = _oldSelection.location;
  _stateBeforeChange.sel_end = _oldSelection.location + _oldSelection.length;

  if (!shouldChange) {
    // Force a change in the autocomplete system, since we won't get an
    // textDidChangeWithUserEvent message from the text input.
    [self textDidChangeWithUserEvent:YES];
  }

  return shouldChange;
}

- (void)textDidChangeWithUserEvent:(BOOL)isProcessingUserEvent {
  id<OmniboxTextInput> textInput = self.textInput;
  // Sanitize pasted text.
  if (_omniboxTextModel &&
      _omniboxTextModel->paste_state == OmniboxPasteState::kPasting) {
    std::u16string pastedText = base::SysNSStringToUTF16(textInput.text);
    std::u16string newText = omnibox::SanitizeTextForPaste(pastedText);
    if (pastedText != newText) {
      [textInput setText:base::SysUTF16ToNSString(newText)];
    }
  }

  // Clear the autocomplete text.
  [textInput clearAutocompleteText];
  [textInput setClearingPreEditText:NO];

  // Determine if the change should proceed without a direct user event
  // (e.g., IME changes, Korean keyboard).
  BOOL proceedWithoutUserEvent = NO;
  NSString* currentLanguage = [[textInput.view textInputMode] primaryLanguage];
  if ([currentLanguage hasPrefix:@"ko-"]) {
    proceedWithoutUserEvent = YES;
  } else {
    NSString* currentMarkedText = [textInput markedText];
    proceedWithoutUserEvent =
        (_markedTextBeforeChange || currentMarkedText) &&
        ![currentMarkedText isEqualToString:_markedTextBeforeChange];
  }

  if (!isProcessingUserEvent && !proceedWithoutUserEvent) {
    return;
  }

  [self onAfterPossibleChange];
  // Call onBeforePossibleChange again to set up for the next potential
  // change.
  [self onBeforePossibleChange];
}

- (void)onAcceptAutocomplete {
  _currentSelection = [self.textInput selectedNSRange];
  [self textDidChangeWithUserEvent:YES];
}

- (NSRange)currentSelection {
  return _currentSelection;
}

- (void)onCopy {
  if (!_omniboxTextModel || !_omniboxClient) {
    return;
  }
  NSString* selectedText = nil;
  NSInteger startLocation = 0;
  id<OmniboxTextInput> textInput = self.textInput;
  if ([textInput isPreEditing]) {
    selectedText = textInput.text;
    startLocation = 0;
  } else {
    UITextRange* selectedRange = [textInput selectedTextRange];
    selectedText = [textInput textInRange:selectedRange];
    UITextPosition* start = [textInput beginningOfDocument];
    // The following call to `-offsetFromPosition:toPosition:` gives the offset
    // in terms of the number of "visible characters."  The documentation does
    // not specify whether this means glyphs or UTF16 chars.  This does not
    // matter for the current implementation of AdjustTextForCopy(), but it may
    // become an issue at some point.
    startLocation = [textInput offsetFromPosition:start
                                       toPosition:[selectedRange start]];
  }
  std::u16string text = selectedText.cr_UTF16String;

  GURL URL;
  bool writeURL = false;

  omnibox::AdjustTextForCopy(
      startLocation, &text,
      /*has_user_modified_text=*/_omniboxTextModel->user_input_in_progress ||
          text != _omniboxTextModel->url_for_editing,
      /*is_keyword_selected=*/false,
      _omniboxAutocompleteController.hasSuggestions
          ? std::optional<AutocompleteMatch>([self currentMatch:nullptr])
          : std::nullopt,
      _omniboxClient, &URL, &writeURL);

  // Create the pasteboard item manually because the pasteboard expects a single
  // item with multiple representations.  This is expressed as a single
  // NSDictionary with multiple keys, one for each representation.
  NSMutableDictionary* item = [NSMutableDictionary dictionaryWithCapacity:2];
  [item setObject:[NSString cr_fromString16:text]
           forKey:UTTypePlainText.identifier];

  using enum OmniboxCopyType;
  if (writeURL && URL.is_valid()) {
    [item setObject:net::NSURLWithGURL(URL) forKey:UTTypeURL.identifier];

    if ([textInput isPreEditing]) {
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
  if (_omniboxTextModel) {
    UMA_HISTOGRAM_COUNTS_1M("Omnibox.Paste", 1);
    _omniboxTextModel->paste_state = OmniboxPasteState::kPasting;
  }

  [self.textInput exitPreEditState];
}

- (void)onDeleteBackward {
  id<OmniboxTextInput> textInput = self.textInput;
  if (textInput.text.length == 0) {
    // If the user taps backspace while the pre-edit text is showing,
    // shouldChangeCharactersInRange is invoked before this method and sets the
    // text to an empty string, so use the `clearingPreEditText` to determine if
    // the chip should be cleared or not.
    if ([textInput clearingPreEditText]) {
      // In the case where backspace is tapped while in pre-edit mode,
      // shouldChangeCharactersInRange is called but textDidChangeWithUserEvent
      // is never called so ensure the clearingPreEditText flag is set to false
      // again.
      [textInput setClearingPreEditText:NO];
      // Explicitly set the input-in-progress flag. Normally this is set via
      // in model()->OnAfterPossibleChange, but in this case the text has been
      // set to the empty string by `shouldChangeCharactersInRange` so when
      // OnAfterPossibleChange checks if the text has changed it does not see
      // any difference so it never sets the input-in-progress flag.
      [self setInputInProgress:YES];
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
  [self.textInput endEditing:YES];
}

- (void)refineWithText:(const std::u16string&)text {
  id<OmniboxTextInput> textInput = self.textInput;
  // Exit preedit state and append the match. Refocus if necessary.
  [textInput exitPreEditState];
  [self setUserText:text];

  [self setWindowText:text
               caretPos:text.length()
      startAutocomplete:true
      notifyTextChanged:true];

  [self onBeforePossibleChange];
  // Calling setText: does not trigger `textInputDidChange`, so trigger that
  // manually.
  [textInput.omniboxTextInputDelegate textInputDidChange:textInput];
  [textInput.view becomeFirstResponder];
  // Set the caret pos to the end of the text (crbug.com/331622199).
  [self setCaretPos:text.length()];
}

#pragma mark - Private

/// Previews `suggestion` in the Omnibox. Called when a suggestion is
/// highlighted in the popup.
- (void)previewSuggestion:(id<AutocompleteSuggestion>)suggestion {
  id<OmniboxTextInput> textInput = self.textInput;
  NSAttributedString* previewText = suggestion.omniboxPreviewText;

  [textInput exitPreEditState];
  [textInput setAdditionalText:nil];
  [textInput setText:previewText userTextLength:previewText.length];
}

/// Updates the appearance of popup to have proper text alignment.
- (void)updatePopupLayoutDirection {
  id<OmniboxTextInput> textInput = self.textInput;
  [self.omniboxAutocompleteController
      setTextAlignment:[textInput bestTextAlignment]];
  [self.omniboxAutocompleteController
      setSemanticContentAttribute:[textInput bestSemanticContentAttribute]];
}

/// Sets the caret position. Removes any selection. Clamps the requested caret
/// position to the length of the current text.
- (void)setCaretPos:(NSUInteger)caretPos {
  id<OmniboxTextInput> textInput = self.textInput;
  DCHECK(caretPos <= textInput.text.length || caretPos == 0);
  UITextPosition* start = textInput.beginningOfDocument;
  UITextPosition* newPosition = [textInput positionFromPosition:start
                                                         offset:caretPos];
  // Position and range can be nil causing crash. crbug.com/422295565
  if (!newPosition) {
    return;
  }
  UITextRange* textRange = [textInput textRangeFromPosition:newPosition
                                                 toPosition:newPosition];
  if (!textRange) {
    return;
  }
  textInput.selectedTextRange = textRange;
}

/// Updates the autocomplete popup and other state after the text has been
/// changed by the user.
- (void)startAutocompleteAfterEdit {
  [self setInputInProgress:YES];

  if (!_omniboxTextModel || !_omniboxTextModel->HasFocus()) {
    return;
  }

  id<OmniboxTextInput> textInput = self.textInput;
  // Prevent inline-autocomplete if the IME is currently composing or if the
  // cursor is not at the end of the text.
  const BOOL IMEComposing = [textInput markedTextRange] != nil;
  NSRange currentSelection = [self currentSelection];
  BOOL preventInlineAutocomplete =
      IMEComposing || NSMaxRange(currentSelection) != [textInput.text length];
  [self startAutocompletePreventingInline:preventInlineAutocomplete];

  [self updatePopupLayoutDirection];
}

/// Starts autocomplete with the state in `_omniboxTextModel` and the textInput
/// selection bounds.
- (void)startAutocompletePreventingInline:(BOOL)preventInlineAutocomplete {
  const std::u16string inputText = _omniboxTextModel->user_text;

  size_t start, cursorPosition;
  [self getSelectionBounds:&start end:&cursorPosition];
  BOOL hasSelectedText = start != cursorPosition;

  preventInlineAutocomplete =
      preventInlineAutocomplete || _omniboxTextModel->just_deleted_text ||
      (hasSelectedText && _omniboxTextModel->inline_autocompletion.empty()) ||
      _omniboxTextModel->paste_state != OmniboxPasteState::kNone;

  [_omniboxAutocompleteController
      startAutocompleteWithText:inputText
                 cursorPosition:cursorPosition
      preventInlineAutocomplete:preventInlineAutocomplete];
}

/// Sets the window text and the caret position. `notifyTextChanged` is true if
/// the model should be notified of the change. Clears the additional text.
- (void)setWindowText:(const std::u16string&)text
             caretPos:(size_t)caretPos
    startAutocomplete:(BOOL)startAutocomplete
    notifyTextChanged:(BOOL)notifyTextChanged {
  id<OmniboxTextInput> textInput = self.textInput;
  // Do not call SetUserText() here, as the user has not triggered this change.
  // Instead, set the input's text directly.
  [textInput setText:[NSString cr_fromString16:text]];

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:[NSString cr_fromString16:text]];
  [textInput setText:as userTextLength:[as length]];

  if (startAutocomplete) {
    [self startAutocompleteAfterEdit];
  }

  if (notifyTextChanged) {
    [self onTextChanged];
  }

  [self setCaretPos:caretPos];
}

/// Updates inline autocomplete if the full text is different.
- (void)updateAutocompleteIfTextChanged:(const std::u16string&)userText
                         autocompletion:
                             (const std::u16string&)inlineAutocomplete {
  std::u16string displayedText = userText + inlineAutocomplete;
  if (displayedText == self.textInput.displayedText.cr_UTF16String) {
    return;
  }

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:[NSString cr_fromString16:displayedText]];
  [self.textInput setText:as userTextLength:userText.size()];
}

/// Notifes the client and asks the autocomplete controller to start with a new
/// updated input on user input in progress change.
- (void)updateInput {
  if (!_omniboxTextModel) {
    return;
  }
  BOOL changeToUserInputInProgress =
      _omniboxTextModel->SetInputInProgressNoNotify(true);

  if (changeToUserInputInProgress &&
      _omniboxTextModel->user_input_in_progress) {
    [self.omniboxAutocompleteController resetSession];
  }

  if (!(_omniboxTextModel->HasFocus())) {
    [self notifyClientOnUserInputInProgressChange:changeToUserInputInProgress];
    return;
  }

  if (changeToUserInputInProgress && _omniboxTextModel->user_text.empty()) {
    // In the case the user enters user-input-in-progress mode by clearing
    // everything (i.e. via Backspace), ask for ZeroSuggestions instead of the
    // normal prefix (as-you-type) autocomplete.
    //
    // The difference between a ZeroSuggest request and a normal
    // prefix autocomplete request is getting fuzzier, and should be fully
    // encapsulated by the AutocompleteInput::focus_type() member. We should
    // merge these two calls soon, lest we confuse future developers.
    [_omniboxAutocompleteController
        startZeroSuggestRequestWithText:self.textInput.displayedText
                                            .cr_UTF16String
                          userClobbered:YES];
  } else {
    // Otherwise run the normal prefix (as-you-type) autocomplete.
    [self startAutocompletePreventingInline:YES];
  }

  [self notifyClientOnUserInputInProgressChange:changeToUserInputInProgress];
}

/// Gets the current text input state.
- (void)getState:(OmniboxTextState*)state {
  state->text = base::SysNSStringToUTF16([self.textInput displayedText]);
  [self getSelectionBounds:&state->sel_start end:&state->sel_end];
}

/// Marks the text state before future changes.
- (void)onBeforePossibleChange {
  [self getState:&_stateBeforeChange];
  _markedTextBeforeChange = [[self.textInput markedText] copy];
}

/// Computes state changes and inform the edit model.
- (BOOL)onAfterPossibleChange {
  OmniboxTextState newState;
  [self getState:&newState];
  newState.sel_start = _currentSelection.location;
  newState.sel_end = _currentSelection.location + _currentSelection.length;

  OmniboxStateChanges state_changes =
      _omniboxTextModel->GetStateChanges(_stateBeforeChange, newState);

  const BOOL somethingChanged =
      _omniboxTextModel->UpdateStateAfterPossibleChange(state_changes);

  if (somethingChanged) {
    [self startAutocompleteAfterEdit];
  }

  [self onTextChanged];

  // TODO(crbug.com/379695536): Find a different place to call this. Give the
  // omnibox a chance to update the alignment for a text direction change.
  [self.textInput updateTextDirection];

  return somethingChanged;
}

@end
