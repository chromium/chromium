// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#import <CoreText/CoreText.h>
#import <MobileCoreServices/MobileCoreServices.h>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/ios/device_util.h"
#include "base/ios/ios_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/strings/string16.h"
#include "base/strings/sys_string_conversions.h"
#include "components/omnibox/browser/autocomplete_input.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "components/omnibox/browser/location_bar_model.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_popup_model.h"
#include "components/omnibox/common/omnibox_focus_state.h"
#include "ios/chrome/browser/autocomplete/autocomplete_scheme_classifier_impl.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/ui/omnibox/chrome_omnibox_client_ios.h"
#include "ios/chrome/browser/ui/omnibox/omnibox_text_field_paste_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#include "ios/chrome/browser/ui/omnibox/web_omnibox_edit_controller.h"
#import "ios/chrome/browser/ui/toolbar/public/omnibox_focuser.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/grit/ios_theme_resources.h"
#include "ios/web/public/navigation/referrer.h"
#import "net/base/mac/url_conversions.h"
#include "skia/ext/skia_utils_ios.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/image/image.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using base::UserMetricsAction;

namespace {

// The color of the https when there is an error.
UIColor* ErrorTextColor() {
  return skia::UIColorFromSkColor(gfx::kGoogleRed700);
}

// The color of the https when there is not an error.
UIColor* SecureTextColor() {
  return skia::UIColorFromSkColor(gfx::kGoogleGreen700);
}

// The color of the https when highlighted in incognito.
UIColor* IncognitoSecureTextColor() {
  return [UIColor colorWithWhite:(255 / 255.0) alpha:1.0];
}

}  // namespace

#pragma mark - OminboxViewIOS

OmniboxViewIOS::OmniboxViewIOS(OmniboxTextFieldIOS* field,
                               WebOmniboxEditController* controller,
                               id<OmniboxLeftImageConsumer> left_image_consumer,
                               ios::ChromeBrowserState* browser_state,
                               id<OmniboxFocuser> omnibox_focuser)
    : OmniboxView(controller,
                  controller
                      ? std::make_unique<ChromeOmniboxClientIOS>(controller,
                                                                 browser_state)
                      : nullptr),
      browser_state_(browser_state),
      field_(field),
      controller_(controller),
      left_image_consumer_(left_image_consumer),
      omnibox_focuser_(omnibox_focuser),
      ignore_popup_updates_(false),
      attributing_display_string_(nil),
      popup_provider_(nullptr) {
  DCHECK(field_);

  paste_delegate_ = [[OmniboxTextFieldPasteDelegate alloc] init];
  [field_ setPasteDelegate:paste_delegate_];

  use_strikethrough_workaround_ = base::ios::IsRunningOnOrLater(10, 3, 0) &&
                                  !base::ios::IsRunningOnOrLater(11, 2, 0);
}

void OmniboxViewIOS::OpenMatch(const AutocompleteMatch& match,
                               WindowOpenDisposition disposition,
                               const GURL& alternate_nav_url,
                               const base::string16& pasted_text,
                               size_t selected_line,
                               base::TimeTicks match_selection_timestamp) {
  // It may be unsafe to modify the contents of the field.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return;
  }

  OmniboxView::OpenMatch(match, disposition, alternate_nav_url, pasted_text,
                         selected_line, match_selection_timestamp);
}

base::string16 OmniboxViewIOS::GetText() const {
  return [field_ displayedText];
}

void OmniboxViewIOS::SetWindowTextAndCaretPos(const base::string16& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  // Do not call SetUserText() here, as the user has not triggered this change.
  // Instead, set the field's text directly.
  // Set the field_ value before calling ApplyTextAttributes(), because that
  // internally calls model()->CurrentTextIsUrl(), which uses the text in the
  // field_.
  [field_ setText:base::SysUTF16ToNSString(text)];

  NSAttributedString* as = ApplyTextAttributes(text);
  [field_ setText:as userTextLength:[as length]];

  if (update_popup)
    UpdatePopup();

  if (notify_text_changed && model())
    model()->OnChanged();

  SetCaretPos(caret_pos);
}

void OmniboxViewIOS::SetCaretPos(size_t caret_pos) {
  DCHECK(caret_pos <= field_.text.length || caret_pos == 0);
  UITextPosition* start = field_.beginningOfDocument;
  UITextPosition* newPosition =
      [field_ positionFromPosition:start offset:caret_pos];
  field_.selectedTextRange =
      [field_ textRangeFromPosition:newPosition toPosition:newPosition];
}

void OmniboxViewIOS::RevertAll() {
  ignore_popup_updates_ = true;
  OmniboxView::RevertAll();
  ignore_popup_updates_ = false;
}

void OmniboxViewIOS::UpdatePopup() {
  if (model())
    model()->SetInputInProgress(true);

  if (model() && !model()->has_focus())
    return;

  // Prevent inline-autocomplete if the IME is currently composing or if the
  // cursor is not at the end of the text.
  bool prevent_inline_autocomplete =
      IsImeComposing() ||
      NSMaxRange(current_selection_) != [[field_ text] length];
  if (model())
    model()->StartAutocomplete(current_selection_.length != 0,
                               prevent_inline_autocomplete);

  UpdatePopupAppearance();
}

void OmniboxViewIOS::UpdatePopupAppearance() {
  DCHECK(popup_provider_);
  popup_provider_->SetTextAlignment([field_ bestTextAlignment]);
  popup_provider_->SetSemanticContentAttribute(
      [field_ bestSemanticContentAttribute]);
}

void OmniboxViewIOS::OnTemporaryTextMaybeChanged(
    const base::string16& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  SetWindowTextAndCaretPos(display_text, display_text.size(), false, false);
  if (model())
    model()->OnChanged();
}

bool OmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const base::string16& display_text,
    size_t user_text_length) {
  if (display_text == GetText())
    return false;

  NSAttributedString* as = ApplyTextAttributes(display_text);
  [field_ setText:as userTextLength:user_text_length];
  if (model())
    model()->OnChanged();
  return true;
}

void OmniboxViewIOS::OnBeforePossibleChange() {
  GetState(&state_before_change_);
  marked_text_before_change_ = [[field_ markedText] copy];
}

bool OmniboxViewIOS::OnAfterPossibleChange(bool allow_keyword_ui_change) {
  State new_state;
  GetState(&new_state);
  // Manually update the selection state after calling GetState().
  new_state.sel_start = current_selection_.location;
  new_state.sel_end = current_selection_.location + current_selection_.length;

  OmniboxView::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  // iOS does not supports KeywordProvider, so never allow keyword UI changes.
  const bool something_changed =
      model() &&
      model()->OnAfterPossibleChange(state_changes, allow_keyword_ui_change);

  if (model())
    model()->OnChanged();

  // TODO(justincohen): Find a different place to call this. Give the omnibox
  // a chance to update the alignment for a text direction change.
  [field_ updateTextDirection];
  return something_changed;
}

bool OmniboxViewIOS::IsImeComposing() const {
  return [field_ markedTextRange] != nil;
}

bool OmniboxViewIOS::IsIndicatingQueryRefinement() const {
  return false;
}

bool OmniboxViewIOS::IsSelectAll() const {
  return false;
}

void OmniboxViewIOS::GetSelectionBounds(base::string16::size_type* start,
                                        base::string16::size_type* end) const {
  if ([field_ isFirstResponder]) {
    NSRange selected_range = [field_ selectedNSRange];
    *start = selected_range.location;
    *end = selected_range.location + selected_range.length;
  } else {
    *start = *end = 0;
  }
}

gfx::NativeView OmniboxViewIOS::GetNativeView() const {
  return nullptr;
}

gfx::NativeView OmniboxViewIOS::GetRelativeWindowForPopup() const {
  return nullptr;
}

void OmniboxViewIOS::OnDidBeginEditing() {

  // If Open from Clipboard offers a suggestion, the popup may be opened when
  // |OnSetFocus| is called on the model. The state of the popup is saved early
  // to ignore that case.
  DCHECK(popup_provider_);
  bool popup_was_open_before_editing_began = popup_provider_->IsPopupOpen();

  // Text attributes (e.g. text color) should not be shown while editing, so
  // strip them out by calling setText (as opposed to setAttributedText).
  [field_ setText:[field_ text]];
  OnBeforePossibleChange();

  // Make sure the omnibox popup's semantic content attribute is set correctly.
  popup_provider_->SetSemanticContentAttribute(
      [field_ bestSemanticContentAttribute]);

  if (model()) {
    // In the case where the user taps the fakebox on the Google landing page,
    // or from the secondary toolbar search button, the focus source is already
    // set to FAKEBOX or SEARCH_BUTTON respectively. Otherwise, set it to
    // OMNIBOX.
    if (model()->focus_source() != OmniboxFocusSource::FAKEBOX &&
        model()->focus_source() != OmniboxFocusSource::SEARCH_BUTTON) {
      model()->set_focus_source(OmniboxFocusSource::OMNIBOX);
    }

    model()->OnSetFocus(/*control_down=*/false,
                        /*suppress_on_focus_suggestions=*/false);
  }

  // If the omnibox is displaying a URL and the popup is not showing, set the
  // field into pre-editing state.  If the omnibox is displaying search terms,
  // leave the default behavior of positioning the cursor at the end of the
  // text.  If the popup is already open, that means that the omnibox is
  // regaining focus after a popup scroll took focus away, so the pre-edit
  // behavior should not be invoked.
  if (!popup_was_open_before_editing_began)
    [field_ enterPreEditState];

  // |controller_| is only forwarding the call to the BVC. This should only
  // happen when the omnibox is being focused and it starts showing the popup;
  // if the popup was already open, no need to call this.
    if (!popup_was_open_before_editing_began)
      controller_->OnSetFocus();
}

void OmniboxViewIOS::OnWillEndEditing() {
  // On iPad, this will be called when the "hide keyboard" button is pressed
  // on the software keyboard. This should be equivalent to tapping the typing
  // shield and should defocus the omnibox, transition the location bar to
  // steady view, and close the popup.
  // This will also be called if -resignFirstResponder is called
  // programmatically. On phone, the omnibox may still be editing when
  // the popup is open, so the Cancel button calls OnWillEndEditing.
  if (IsIPadIdiom()) {
    [omnibox_focuser_ cancelOmniboxEdit];
  }
}

bool OmniboxViewIOS::OnWillChange(NSRange range, NSString* new_text) {
  bool ok_to_change = true;

  // It may be unsafe to modify the contents of the field.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return false;
  }

  if ([field_ isPreEditing]) {
    [field_ setClearingPreEditText:YES];

    // Exit the pre-editing state in OnWillChange() instead of OnDidChange(), as
    // that allows IME to continue working.  The following code selects the text
    // as if the pre-edit fake selection was real.
    [field_ exitPreEditState];

    field_.selectedTextRange =
        [field_ textRangeFromPosition:field_.beginningOfDocument
                           toPosition:field_.endOfDocument];

    // Reset |range| to be of zero-length at location zero, as the field will be
    // now cleared.
    range = NSMakeRange(0, 0);
  }

  // Figure out the old and current (new) selections.  Assume the new selection
  // will be of zero-length, located at the end of |new_text|.
  NSRange old_range = range;
  NSRange new_range = NSMakeRange(range.location + [new_text length], 0);

  // We may need to fix up the old and new ranges in the case where autocomplete
  // text was showing.  If there is autocomplete text, assume it was selected.
  // If the change is deleting one character from the end of the actual text,
  // disallow the change, but clear the autocomplete text and call OnDidChange
  // directly.  If there is autocomplete text AND a text field selection, or if
  // the user entered multiple characters, clear the autocomplete text and
  // pretend it never existed.
  if ([field_ hasAutocompleteText]) {
    bool adding_text = (range.length < [new_text length]);
    bool deleting_text = (range.length > [new_text length]);

    if (adding_text) {
      // TODO(rohitrao): What about cases where [new_text length] > 1?  This
      // could happen if an IME completion inserts multiple characters at once,
      // or if the user pastes some text in.  Let's loosen this test to allow
      // multiple characters, as long as the "old range" ends at the end of the
      // permanent text.
      if ([new_text length] == 1 && range.location == [[field_ text] length]) {
        old_range = NSMakeRange([[field_ text] length],
                                [field_ autocompleteText].length());
      }
    } else if (deleting_text) {
      if ([new_text length] == 0 &&
          range.location == [[field_ text] length] - 1) {
        ok_to_change = false;
      }
    }
  }

  // Update variables needed by OnDidChange() and GetState().
  old_selection_ = old_range;
  current_selection_ = new_range;

  // Store the displayed text.  Older version of Chrome used to clear the
  // autocomplete text here as well, but on iOS7 doing this causes the inline
  // autocomplete text to flicker, so the call was moved to the start on
  // OnDidChange().
  GetState(&state_before_change_);
  // Manually update the selection state after calling GetState().
  state_before_change_.sel_start = old_selection_.location;
  state_before_change_.sel_end =
      old_selection_.location + old_selection_.length;

  if (!ok_to_change) {
    // Force a change in the autocomplete system, since we won't get an
    // OnDidChange() message.
    OnDidChange(true);
  }

  return ok_to_change;
}

void OmniboxViewIOS::OnDidChange(bool processing_user_event) {
  // Sanitize pasted text.
  if (model() && model()->is_pasting()) {
    base::string16 pastedText = base::SysNSStringToUTF16([field_ text]);
    base::string16 newText = OmniboxView::SanitizeTextForPaste(pastedText);
    if (pastedText != newText) {
      [field_ setText:base::SysUTF16ToNSString(newText)];
    }
  }

  // Clear the autocomplete text, since the omnibox model does not expect to see
  // it in OnAfterPossibleChange().  Clearing the text here should not cause
  // flicker as the UI will not get a chance to redraw before the new
  // autocomplete text is set by the model.
  [field_ clearAutocompleteText];
  [field_ setClearingPreEditText:NO];

  // Generally do not notify the autocomplete system of a text change unless the
  // change was a direct result of a user event.  One exception is if the marked
  // text changed, which could happen through a delayed IME recognition
  // callback.
  bool proceed_without_user_event = false;

  // The IME exception does not work for Korean text, because Korean does not
  // seem to ever have marked text.  It simply replaces or modifies previous
  // characters as you type.  Always proceed without user input if the
  // Korean keyboard is currently active.
  NSString* current_language = [[field_ textInputMode] primaryLanguage];

  if ([current_language hasPrefix:@"ko-"]) {
    proceed_without_user_event = true;
  } else {
    NSString* current_marked_text = [field_ markedText];

    // The IME exception kicks in if the current marked text is not equal to the
    // previous marked text.  Two nil strings should be considered equal, so
    // There is logic to avoid calling into isEqual: in that case.
    proceed_without_user_event =
        (marked_text_before_change_ || current_marked_text) &&
        ![current_marked_text isEqual:marked_text_before_change_];
  }

  if (!processing_user_event && !proceed_without_user_event)
    return;

  // TODO(crbug.com/564599): OnAfterPossibleChange() now takes an argument. It
  // use to not take an argument and was defaulting to false, so as it is
  // unclear what the correct value is, using what was that before seems
  // consistent.
  OnAfterPossibleChange(false);
  OnBeforePossibleChange();
}

void OmniboxViewIOS::OnAccept() {
  // It may be unsafe to modify the contents of the field.
  // Note that by happy coincidence, the |textFieldDidReturn| delegate method
  // always returns NO, which is the desired behavior in this situation.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return;
  }

  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));

  WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB;
  if (model()) {
    model()->AcceptInput(disposition);
  }
  RevertAll();
}

void OmniboxViewIOS::OnClear() {
  [field_ clearAutocompleteText];
  [field_ exitPreEditState];
}

void OmniboxViewIOS::OnCopy() {
  UIPasteboard* board = [UIPasteboard generalPasteboard];
  NSString* selectedText = nil;
  NSInteger start_location = 0;
  if ([field_ isPreEditing]) {
    selectedText = [field_ preEditText];
    start_location = 0;
  } else {
    UITextRange* selected_range = [field_ selectedTextRange];
    selectedText = [field_ textInRange:selected_range];
    UITextPosition* start = [field_ beginningOfDocument];
    // The following call to |-offsetFromPosition:toPosition:| gives the offset
    // in terms of the number of "visible characters."  The documentation does
    // not specify whether this means glyphs or UTF16 chars.  This does not
    // matter for the current implementation of AdjustTextForCopy(), but it may
    // become an issue at some point.
    start_location =
        [field_ offsetFromPosition:start toPosition:[selected_range start]];
  }
  base::string16 text = base::SysNSStringToUTF16(selectedText);

  GURL url;
  bool write_url = false;
  // Model can be nullptr in tests.
  if (model())
    model()->AdjustTextForCopy(start_location, &text, &url, &write_url);

  // Create the pasteboard item manually because the pasteboard expects a single
  // item with multiple representations.  This is expressed as a single
  // NSDictionary with multiple keys, one for each representation.
  NSMutableDictionary* item = [NSMutableDictionary dictionaryWithCapacity:2];
  [item setObject:base::SysUTF16ToNSString(text)
           forKey:(NSString*)kUTTypePlainText];

  if (write_url)
    [item setObject:net::NSURLWithGURL(url) forKey:(NSString*)kUTTypeURL];

  board.items = [NSArray arrayWithObject:item];
}

void OmniboxViewIOS::WillPaste() {
  if (model())
    model()->OnPaste();

  [field_ exitPreEditState];
}

// static
UIColor* OmniboxViewIOS::GetSecureTextColor(
    security_state::SecurityLevel security_level,
    bool in_dark_mode) {
  if (security_level == security_state::EV_SECURE ||
      security_level == security_state::SECURE) {
    return in_dark_mode ? IncognitoSecureTextColor() : SecureTextColor();
  }

  // Don't color strikethrough in dark mode. See https://crbug.com/635004#c6
  if (security_level == security_state::DANGEROUS && !in_dark_mode)
    return ErrorTextColor();

  return nil;
}

NSAttributedString* OmniboxViewIOS::ApplyTextAttributes(
    const base::string16& text) {
  NSMutableAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(text)];
  // Cache a pointer to the attributed string to allow the superclass'
  // virtual method invocations to add attributes.
  DCHECK(attributing_display_string_ == nil);
  base::AutoReset<NSMutableAttributedString*> resetter(
      &attributing_display_string_, as);
  if (model())
    UpdateTextStyle(text, model()->CurrentTextIsURL(),
                    AutocompleteSchemeClassifierImpl());
  return as;
}

void OmniboxViewIOS::UpdateAppearance() {
  // If Siri is thinking, treat that as user input being in progress.  It is
  // unsafe to modify the text field while voice entry is pending.
  if (model() && model()->ResetDisplayTexts()) {
    // Revert everything to the baseline look.
    RevertAll();
  } else if (model() && !model()->has_focus() &&
             !ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    // Even if the change wasn't "user visible" to the model, it still may be
    // necessary to re-color to the URL string.  Only do this if the omnibox is
    // not currently focused.
    NSAttributedString* as =
        ApplyTextAttributes(model()->GetPermanentDisplayText());
    [field_ setText:as userTextLength:[as length]];
  }
}

void OmniboxViewIOS::OnDeleteBackward() {
  if ([field_ text].length == 0) {
    // If the user taps backspace while the pre-edit text is showing,
    // OnWillChange is invoked before this method and sets the text to an empty
    // string, so use the |clearingPreEditText| to determine if the chip should
    // be cleared or not.
    if ([field_ clearingPreEditText]) {
      // In the case where backspace is tapped while in pre-edit mode,
      // OnWillChange is called but OnDidChange is never called so ensure the
      // clearingPreEditText flag is set to false again.
      [field_ setClearingPreEditText:NO];
      // Explicitly set the input-in-progress flag. Normally this is set via
      // in model()->OnAfterPossibleChange, but in this case the text has been
      // set to the empty string by OnWillChange so when OnAfterPossibleChange
      // checks if the text has changed it does not see any difference so it
      // never sets the input-in-progress flag.
      if (model())
        model()->SetInputInProgress(YES);
    } else {
      RemoveQueryRefinementChip();
    }
  }
}

void OmniboxViewIOS::ClearText() {
  // It may be unsafe to modify the contents of the field.
  if (ShouldIgnoreUserInputDueToPendingVoiceSearch()) {
    return;
  }

  // Ensure omnibox is first responder. This will bring up the keyboard so the
  // user can start typing a new query.
  if (![field_ isFirstResponder])
    [field_ becomeFirstResponder];
  if ([field_ text].length == 0) {
    // If |field_| is empty, remove the query refinement chip.
    RemoveQueryRefinementChip();
  } else {
    // Otherwise, just remove the text in the omnibox.
    // Calling -[UITextField setText:] does not trigger
    // -[id<UITextFieldDelegate> textDidChange] so it must be called explicitly.
    OnClear();
    [field_ setText:@""];
    OnDidChange(YES);
  }
  // Calling OnDidChange() can trigger a scroll event, which removes focus from
  // the omnibox.
  [field_ becomeFirstResponder];
}

void OmniboxViewIOS::RemoveQueryRefinementChip() {
  controller_->OnChanged();
}

bool OmniboxViewIOS::ShouldIgnoreUserInputDueToPendingVoiceSearch() {
  // When the response of the iOS voice entry is pending a spinning wheel is
  // visible.  The spinner's location is marked in [self text] as a Unicode
  // "Object Replacement Character".
  // http://www.fileformat.info/info/unicode/char/fffc/index.htm
  NSString* objectReplacementChar =
      [NSString stringWithFormat:@"%C", (unichar)0xFFFC];
  return [[field_ text] rangeOfString:objectReplacementChar].length > 0;
}

void OmniboxViewIOS::EndEditing() {
  if (model() && model()->has_focus()) {
    CloseOmniboxPopup();

    model()->OnWillKillFocus();
    model()->OnKillFocus();
    if ([field_ isPreEditing])
      [field_ exitPreEditState];

    // The controller looks at the current pre-edit state, so the call to
    // OnKillFocus() must come after exiting pre-edit.
    controller_->OnKillFocus();

    // Blow away any in-progress edits.
    RevertAll();
    DCHECK(![field_ hasAutocompleteText]);
  }
}

void OmniboxViewIOS::HideKeyboard() {
  [field_ resignFirstResponder];
}

void OmniboxViewIOS::FocusOmnibox() {
  [field_ becomeFirstResponder];
}

BOOL OmniboxViewIOS::IsPopupOpen() {
  DCHECK(popup_provider_);
  return popup_provider_->IsPopupOpen();
}

int OmniboxViewIOS::GetIcon(bool offlinePage) const {
  if (!IsEditingOrEmpty()) {
    if (offlinePage) {
      return IDR_IOS_OMNIBOX_OFFLINE;
    }
    return GetIconForSecurityState(
        controller()->GetLocationBarModel()->GetSecurityLevel());
  }
  return GetIconForAutocompleteMatchType(
      model() ? model()->CurrentMatch(nullptr).type
              : AutocompleteMatchType::URL_WHAT_YOU_TYPED,
      /* is_starred */ false, /* is_incognito */ false);
}

int OmniboxViewIOS::GetOmniboxTextLength() const {
  return [field_ displayedText].length();
}

void OmniboxViewIOS::EmphasizeURLComponents() {
// TODO(rohitrao): Implement this function using code like below.  This code
// is being left out for now because it was not present before the OmniboxView
// rewrite.
#if 0
  // When editing is in progress, the url text is not colored, so there is
  // nothing to emphasize.  (Calling SetText() in that situation would also be
  // harmful, as it would reset the carat position to the end of the text.)
  if (!IsEditingOrEmpty())
    SetText(GetText());
#endif
}

#pragma mark - OmniboxPopupViewSuggestionsDelegate

void OmniboxViewIOS::OnTopmostSuggestionImageChanged(
    AutocompleteMatchType::Type match_type,
    base::Optional<SuggestionAnswer::AnswerType> answer_type,
    GURL favicon_url) {
  [left_image_consumer_ setLeftImageForAutocompleteType:match_type
                                             answerType:answer_type
                                             faviconURL:favicon_url];
}

void OmniboxViewIOS::OnResultsChanged(const AutocompleteResult& result) {
  if (ignore_popup_updates_) {
    // Please contact rohitrao@ if the following DCHECK ever fires.  If
    // |ignore_popup_updates_| is true but |result| is not empty, then the new
    // prerender code in ChromeOmniboxClientIOS will incorrectly discard its
    // prerender.
    // TODO(crbug.com/754050): Remove this whole method once we are reasonably
    // confident that we are not throwing away prerenders.
    DCHECK(result.empty());
  }
}

void OmniboxViewIOS::OnPopupDidScroll() {
  if (!IsIPadIdiom()) {
    this->HideKeyboard();
  }
}

void OmniboxViewIOS::OnSelectedMatchForAppending(const base::string16& str) {
  // Exit preedit state and append the match. Refocus if necessary.
  if ([field_ isPreEditing])
    [field_ exitPreEditState];
  this->SetUserText(str);
  this->FocusOmnibox();
}

void OmniboxViewIOS::OnSelectedMatchForOpening(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const base::string16& pasted_text,
    size_t index) {
  this->OpenMatch(match, disposition, alternate_nav_url, pasted_text, index,
                  base::TimeTicks());
}
