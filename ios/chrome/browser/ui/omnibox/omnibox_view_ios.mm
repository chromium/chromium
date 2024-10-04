// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/omnibox/omnibox_view_ios.h"

#import <CoreText/CoreText.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <string>

#import "base/command_line.h"
#import "base/ios/device_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_controller.h"
#import "components/omnibox/browser/omnibox_edit_model.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_focus_delegate.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_metrics_helper.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_ui_features.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_util.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_view_consumer.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/grit/ios_theme_resources.h"
#import "ios/web/public/navigation/referrer.h"
#import "net/base/apple/url_conversions.h"
#import "ui/base/device_form_factor.h"
#import "ui/base/page_transition_types.h"
#import "ui/base/resource/resource_bundle.h"
#import "ui/base/window_open_disposition.h"
#import "ui/gfx/image/image.h"

using base::UserMetricsAction;

#pragma mark - OminboxViewIOS

OmniboxViewIOS::OmniboxViewIOS(OmniboxTextFieldIOS* field,
                               std::unique_ptr<OmniboxClient> client,
                               ProfileIOS* profile,
                               id<OmniboxCommands> omnibox_focuser,
                               id<OmniboxFocusDelegate> focus_delegate,
                               id<ToolbarCommands> toolbar_commands_handler,
                               id<OmniboxViewConsumer> consumer,
                               bool is_lens_overlay)
    : OmniboxView(std::move(client)),
      field_(field),
      omnibox_focuser_(omnibox_focuser),
      focus_delegate_(focus_delegate),
      toolbar_commands_handler_(toolbar_commands_handler),
      consumer_(consumer),
      ignore_popup_updates_(false),
      is_lens_overlay_(is_lens_overlay),
      popup_provider_(nullptr) {
  DCHECK(field_);
}

OmniboxViewIOS::~OmniboxViewIOS() = default;

void OmniboxViewIOS::OnReceiveClipboardURLForOpenMatch(
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    std::optional<GURL> optional_gurl) {
  if (!optional_gurl) {
    return;
  }

  GURL url = std::move(optional_gurl).value();

  AutocompleteController* autocomplete_controller =
      controller()->autocomplete_controller();

  AcceptThumbnailEdits();
  OmniboxPopupSelection selection(autocomplete_controller->InjectAdHocMatch(
      autocomplete_controller->clipboard_provider()->NewClipboardURLMatch(
          url)));
  model()->OpenSelection(selection, match_selection_timestamp, disposition);
}

void OmniboxViewIOS::OnReceiveClipboardTextForOpenMatch(
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    std::optional<std::u16string> optional_text) {
  if (!optional_text) {
    return;
  }

  std::u16string text = std::move(optional_text).value();

  ClipboardProvider* clipboard_provider =
      controller()->autocomplete_controller()->clipboard_provider();
  std::optional<AutocompleteMatch> new_match =
      clipboard_provider->NewClipboardTextMatch(text);

  if (!new_match) {
    return;
  }

  AcceptThumbnailEdits();
  OmniboxPopupSelection selection(
      controller()->autocomplete_controller()->InjectAdHocMatch(
          new_match.value()));
  model()->OpenSelection(selection, match_selection_timestamp, disposition);
}

void OmniboxViewIOS::OnReceiveClipboardImageForOpenMatch(
    const AutocompleteMatch& match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    std::optional<gfx::Image> optional_image) {
  ClipboardProvider* clipboard_provider =
      controller()->autocomplete_controller()->clipboard_provider();
  clipboard_provider->NewClipboardImageMatch(
      optional_image,
      base::BindOnce(&OmniboxViewIOS::OnReceiveImageMatchForOpenMatch,
                     weak_ptr_factory_.GetWeakPtr(), disposition,
                     alternate_nav_url, pasted_text, selected_line,
                     match_selection_timestamp));
}

void OmniboxViewIOS::OnReceiveImageMatchForOpenMatch(
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t selected_line,
    base::TimeTicks match_selection_timestamp,
    std::optional<AutocompleteMatch> optional_match) {
  if (!optional_match) {
    return;
  }
  AcceptThumbnailEdits();
  OmniboxPopupSelection selection(
      controller()->autocomplete_controller()->InjectAdHocMatch(
          optional_match.value()));
  model()->OpenSelection(selection, match_selection_timestamp, disposition);
}

std::u16string OmniboxViewIOS::GetText() const {
  return base::SysNSStringToUTF16([field_ displayedText]);
}

void OmniboxViewIOS::SetWindowTextAndCaretPos(const std::u16string& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  // Do not call SetUserText() here, as the user has not triggered this change.
  // Instead, set the field's text directly.
  [field_ setText:base::SysUTF16ToNSString(text)];

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(text)];
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
  RevertThumbnailEdits();
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
      NSMaxRange(current_selection_) != [field_.text length];
  if (model())
    model()->StartAutocomplete(current_selection_.length != 0,
                               prevent_inline_autocomplete);

  UpdatePopupAppearance();
}

void OmniboxViewIOS::UpdatePopupAppearance() {
  if (!popup_provider_) {
    return;
  }
  popup_provider_->SetTextAlignment([field_ bestTextAlignment]);
  popup_provider_->SetSemanticContentAttribute(
      [field_ bestSemanticContentAttribute]);
}

void OmniboxViewIOS::OnTemporaryTextMaybeChanged(
    const std::u16string& display_text,
    const AutocompleteMatch& match,
    bool save_original_selection,
    bool notify_text_changed) {
  SetWindowTextAndCaretPos(display_text, display_text.size(), false, false);
  if (model())
    model()->OnChanged();
}

void OmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& display_text,
    std::vector<gfx::Range> selections,
    const std::u16string& prefix_autocompletion,
    const std::u16string& inline_autocompletion) {
  if (display_text == GetText())
    return;

  NSAttributedString* as = [[NSMutableAttributedString alloc]
      initWithString:base::SysUTF16ToNSString(display_text)];
  // TODO(crbug.com/40122891): This `user_text_length` calculation  isn't
  //  accurate when there's prefix autocompletion. This should be addressed
  //  before we experiment with prefix autocompletion on iOS.
  size_t user_text_length = display_text.size() - inline_autocompletion.size();
  [field_ setText:as userTextLength:user_text_length];
}

void OmniboxViewIOS::SetAdditionalText(const std::u16string& text) {
  if (!IsRichAutocompletionEnabled()) {
    return;
  }

  if (IsRichAutocompletionEnabled(
          RichAutocompletionImplementation::kNoAdditionalText)) {
    [consumer_ setOmniboxHasRichInline:text.length()];
    return;
  }

  if (!text.length()) {
    [consumer_ updateAdditionalText:nil];
    return;
  }

  // TODO(b/325035406): Temporary string and colors. Update if needed.
  NSString* additional_text = base::SysUTF16ToNSString(u" - " + text);
  [consumer_ updateAdditionalText:additional_text];
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

void OmniboxViewIOS::GetSelectionBounds(std::u16string::size_type* start,
                                        std::u16string::size_type* end) const {
  if ([field_ isFirstResponder]) {
    NSRange selected_range = [field_ selectedNSRange];
    *start = selected_range.location;
    *end = selected_range.location + field_.autocompleteText.length;
  } else {
    *start = *end = 0;
  }
}

size_t OmniboxViewIOS::GetAllSelectionsLength() const {
  return 0;
}

gfx::NativeView OmniboxViewIOS::GetNativeView() const {
  return gfx::NativeView();
}

gfx::NativeView OmniboxViewIOS::GetRelativeWindowForPopup() const {
  return gfx::NativeView();
}

void OmniboxViewIOS::OnDidBeginEditing() {
  // If Open from Clipboard offers a suggestion, the popup may be opened when
  // `OnSetFocus` is called on the model. The state of the popup is saved early
  // to ignore that case.
  DCHECK(popup_provider_);
  bool popup_was_open_before_editing_began = popup_provider_->IsPopupOpen();

  // Make sure the omnibox popup's semantic content attribute is set correctly.
  popup_provider_->SetSemanticContentAttribute(
      [field_ bestSemanticContentAttribute]);

  OnBeforePossibleChange();

  if (model()) {
    model()->OnSetFocus(/*control_down=*/false);

    if (is_lens_overlay_ && field_.userText.length) {
      model()->SetUserText(base::SysNSStringToUTF16(field_.userText));
      model()->StartAutocomplete(/*has_selected_text=*/false,
                                 /*prevent_inline_autocomplete=*/true);
    } else {
      model()->StartZeroSuggestRequest();
    }
  }

  // If the omnibox is displaying a URL and the popup is not showing, set the
  // field into pre-editing state.  If the omnibox is displaying search terms,
  // leave the default behavior of positioning the cursor at the end of the
  // text.  If the popup is already open, that means that the omnibox is
  // regaining focus after a popup scroll took focus away, so the pre-edit
  // behavior should not be invoked. When `is_lens_overlay_` is true, the
  // omnibox only display search terms.
  if (!popup_was_open_before_editing_began && !is_lens_overlay_) {
    [field_ enterPreEditState];
  }

  // `location_bar_` is only forwarding the call to the BVC. This should only
  // happen when the omnibox is being focused and it starts showing the popup;
  // if the popup was already open, no need to call this.
  if (!popup_was_open_before_editing_began)
    [focus_delegate_ omniboxDidBecomeFirstResponder];
}

bool OmniboxViewIOS::OnWillChange(NSRange range, NSString* new_text) {
  bool ok_to_change = true;

  if ([field_ isPreEditing]) {
    [field_ setClearingPreEditText:YES];

    // Exit the pre-editing state in OnWillChange() instead of OnDidChange(), as
    // that allows IME to continue working.  The following code selects the text
    // as if the pre-edit fake selection was real.
    [field_ exitPreEditState];

    // Reset `range` to be of zero-length at location zero, as the field will be
    // now cleared.
    range = NSMakeRange(0, 0);
  }

  // Figure out the old and current (new) selections.  Assume the new selection
  // will be of zero-length, located at the end of `new_text`.
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
      NSString* userText = field_.userText;

      if (new_text.length == 1 && range.location == userText.length) {
        old_range =
            NSMakeRange(userText.length, field_.autocompleteText.length);
      }
    } else if (deleting_text) {
      NSString* userText = field_.userText;

      if ([new_text length] == 0 && range.location == [userText length] - 1) {
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
    std::u16string pastedText = base::SysNSStringToUTF16(field_.text);
    std::u16string newText = OmniboxView::SanitizeTextForPaste(pastedText);
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
    // There is logic to avoid calling into isEqualToString: in that case.
    proceed_without_user_event =
        (marked_text_before_change_ || current_marked_text) &&
        ![current_marked_text isEqualToString:marked_text_before_change_];
  }

  if (!processing_user_event && !proceed_without_user_event)
    return;

  // TODO(crbug.com/41225237): OnAfterPossibleChange() now takes an argument. It
  // use to not take an argument and was defaulting to false, so as it is
  // unclear what the correct value is, using what was that before seems
  // consistent.
  OnAfterPossibleChange(false);
  OnBeforePossibleChange();
}

void OmniboxViewIOS::OnAccept() {
  base::RecordAction(UserMetricsAction("MobileOmniboxUse"));
  base::RecordAction(UserMetricsAction("IOS.Omnibox.AcceptDefaultSuggestion"));

  // TODO(crbug.com/359150039): handle accept with empty text.
  if (model()) {
    AcceptThumbnailEdits();
    model()->OpenSelection();
  }
  RevertAll();
}

void OmniboxViewIOS::OnClear() {
  [field_ clearAutocompleteText];
  [field_ exitPreEditState];
}

void OmniboxViewIOS::OnCopy() {
  NSString* selectedText = nil;
  NSInteger start_location = 0;
  if ([field_ isPreEditing]) {
    selectedText = field_.text;
    start_location = 0;
  } else {
    UITextRange* selected_range = [field_ selectedTextRange];
    selectedText = [field_ textInRange:selected_range];
    UITextPosition* start = [field_ beginningOfDocument];
    // The following call to `-offsetFromPosition:toPosition:` gives the offset
    // in terms of the number of "visible characters."  The documentation does
    // not specify whether this means glyphs or UTF16 chars.  This does not
    // matter for the current implementation of AdjustTextForCopy(), but it may
    // become an issue at some point.
    start_location =
        [field_ offsetFromPosition:start toPosition:[selected_range start]];
  }
  std::u16string text = base::SysNSStringToUTF16(selectedText);

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
           forKey:UTTypePlainText.identifier];

  if (write_url && url.is_valid()) {
    [item setObject:net::NSURLWithGURL(url) forKey:UTTypeURL.identifier];
  }

  StoreItemInPasteboard(item);
}

void OmniboxViewIOS::WillPaste() {
  if (model())
    model()->OnPaste();

  [field_ exitPreEditState];
}

void OmniboxViewIOS::UpdateAppearance() {
  // If Siri is thinking, treat that as user input being in progress.  It is
  // unsafe to modify the text field while voice entry is pending.
  if (model() && model()->ResetDisplayTexts()) {
    // Revert everything to the baseline look.
    RevertAll();
  } else if (model() && !model()->has_focus()) {
    // Even if the change wasn't "user visible" to the model, it still may be
    // necessary to re-color to the URL string.  Only do this if the omnibox is
    // not currently focused.
    NSAttributedString* as = [[NSMutableAttributedString alloc]
        initWithString:base::SysUTF16ToNSString(
                           model()->GetPermanentDisplayText())];
    [field_ setText:as userTextLength:[as length]];
  }
}

void OmniboxViewIOS::OnDeleteBackward() {
  if (field_.text.length == 0) {
    // If the user taps backspace while the pre-edit text is showing,
    // OnWillChange is invoked before this method and sets the text to an empty
    // string, so use the `clearingPreEditText` to determine if the chip should
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
    }
  }
}

void OmniboxViewIOS::OnAcceptAutocomplete() {
  current_selection_ = [field_ selectedNSRange];
  OnDidChange(/*processing_user_event=*/true);
}

void OmniboxViewIOS::OnRemoveAdditionalText() {
  if (model()) {
    model()->UpdateInput(/*has_selected_text=*/false,
                         /*prevent_inline_autocomplete=*/true);
  }
}

void OmniboxViewIOS::ClearText() {
  // Ensure omnibox is first responder. This will bring up the keyboard so the
  // user can start typing a new query.
  if (![field_ isFirstResponder])
    [field_ becomeFirstResponder];
  if (field_.text.length != 0) {
    // Remove the text in the omnibox.
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

void OmniboxViewIOS::EndEditing() {
  if (model() && model()->has_focus()) {
    CloseOmniboxPopup();

    RecordSuggestionsListScrolled(model()->GetPageClassification(),
                                  suggestions_list_scrolled_);
    suggestions_list_scrolled_ = false;

    model()->OnWillKillFocus();
    model()->OnKillFocus();
    if ([field_ isPreEditing])
      [field_ exitPreEditState];

    // The controller looks at the current pre-edit state, so the call to
    // OnKillFocus() must come after exiting pre-edit.
    [focus_delegate_ omniboxDidResignFirstResponder];

    // Blow away any in-progress edits.
    RevertAll();
    DCHECK(![field_ hasAutocompleteText]);
  }
}

void OmniboxViewIOS::HideKeyboard() {
  [field_ resignFirstResponder];
}

void OmniboxViewIOS::OnCallActionTap() {
  this->HideKeyboard();
}

void OmniboxViewIOS::FocusOmnibox() {
  [field_ becomeFirstResponder];
}

BOOL OmniboxViewIOS::IsPopupOpen() {
  if (!popup_provider_) {
    return NO;
  }
  return popup_provider_->IsPopupOpen();
}

int OmniboxViewIOS::GetOmniboxTextLength() const {
  return [field_ displayedText].length;
}

#pragma mark - OmniboxPopupViewSuggestionsDelegate

void OmniboxViewIOS::OnPopupDidScroll() {
  this->HideKeyboard();
  suggestions_list_scrolled_ = true;
}

void OmniboxViewIOS::OnSelectedMatchForAppending(const std::u16string& str) {
  // Exit preedit state and append the match. Refocus if necessary.
  if ([field_ isPreEditing])
    [field_ exitPreEditState];
  this->SetUserText(str);
  // Calling setText: does not trigger UIControlEventEditingChanged, so
  // trigger that manually.
  [field_ sendActionsForControlEvents:UIControlEventEditingChanged];
  this->FocusOmnibox();
    if (@available(iOS 17, *)) {
      // Set the caret pos to the end of the text (crbug.com/331622199).
      this->SetCaretPos(str.length());
    }
}

void OmniboxViewIOS::OnSelectedMatchForOpening(
    AutocompleteMatch match,
    WindowOpenDisposition disposition,
    const GURL& alternate_nav_url,
    const std::u16string& pasted_text,
    size_t index) {
  const auto match_selection_timestamp = base::TimeTicks();

  // Sometimes the match provided does not correspond to the autocomplete
  // result match specified by `index`. Most Visited Tiles, for example,
  // provide ad hoc matches that are not in the result at all.
  auto* autocomplete_controller = controller()->autocomplete_controller();
  if (index >= autocomplete_controller->result().size() ||
      autocomplete_controller->result().match_at(index).destination_url !=
          match.destination_url) {
    AcceptThumbnailEdits();
    OmniboxPopupSelection selection(
        autocomplete_controller->InjectAdHocMatch(match));
    model()->OpenSelection(selection, match_selection_timestamp, disposition);
    return;
  }

  // Fill in clipboard matches if they don't have a destination URL.
  if (match.destination_url.is_empty()) {
    if (match.type == AutocompleteMatchType::CLIPBOARD_URL) {
      ClipboardRecentContent* clipboard_recent_content =
          ClipboardRecentContent::GetInstance();
      clipboard_recent_content->GetRecentURLFromClipboard(base::BindOnce(
          &OmniboxViewIOS::OnReceiveClipboardURLForOpenMatch,
          weak_ptr_factory_.GetWeakPtr(), match, disposition, alternate_nav_url,
          pasted_text, index, match_selection_timestamp));
      return;
    } else if (match.type == AutocompleteMatchType::CLIPBOARD_TEXT) {
      ClipboardRecentContent* clipboard_recent_content =
          ClipboardRecentContent::GetInstance();
      clipboard_recent_content->GetRecentTextFromClipboard(base::BindOnce(
          &OmniboxViewIOS::OnReceiveClipboardTextForOpenMatch,
          weak_ptr_factory_.GetWeakPtr(), match, disposition, alternate_nav_url,
          pasted_text, index, match_selection_timestamp));
      return;
    } else if (match.type == AutocompleteMatchType::CLIPBOARD_IMAGE) {
      ClipboardRecentContent* clipboard_recent_content =
          ClipboardRecentContent::GetInstance();
      clipboard_recent_content->GetRecentImageFromClipboard(base::BindOnce(
          &OmniboxViewIOS::OnReceiveClipboardImageForOpenMatch,
          weak_ptr_factory_.GetWeakPtr(), match, disposition, alternate_nav_url,
          pasted_text, index, match_selection_timestamp));
      return;
    }
  }
  AcceptThumbnailEdits();
  model()->OpenSelection(OmniboxPopupSelection(index),
                         match_selection_timestamp, disposition);
}

#pragma mark - Thumbnail

void OmniboxViewIOS::SetThumbnailImage(UIImage* image) {
  thumbnail_image_before_edit_ = image;
  thumbnail_deleted_ = NO;
  [consumer_ setThumbnailImage:image];
  if (popup_provider_) {
    popup_provider_->SetHasThumbnail(image != nil);
  }
}

void OmniboxViewIOS::AcceptThumbnailEdits() {
  if (thumbnail_deleted_) {
    base::RecordAction(UserMetricsAction("Mobile.OmniboxThumbnail.Deleted"));
    thumbnail_image_before_edit_ = nil;
    thumbnail_deleted_ = NO;
    if (OmniboxClient* client = controller()->client()) {
      client->OnThumbnailRemoved();
    }
  }
}

void OmniboxViewIOS::RevertThumbnailEdits() {
  if (thumbnail_deleted_) {
    [consumer_ setThumbnailImage:thumbnail_image_before_edit_];
    thumbnail_deleted_ = NO;
    if (popup_provider_) {
      popup_provider_->SetHasThumbnail(thumbnail_image_before_edit_ != nil);
    }
  }
}

void OmniboxViewIOS::RemoveThumbnail() {
  thumbnail_deleted_ = YES;
  [consumer_ setThumbnailImage:nil];
  if (popup_provider_) {
    popup_provider_->SetHasThumbnail(false);
  }
  if (model()) {
    model()->UpdateInput(/*has_selected_text=*/false,
                         /*prevent_inline_autocomplete=*/true);
  }
}
