// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_view_ios.h"

#import <CoreText/CoreText.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import <string>

#import "base/command_line.h"
#import "base/ios/device_util.h"
#import "base/ios/ios_util.h"
#import "base/metrics/user_metrics.h"
#import "base/metrics/user_metrics_action.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/clipboard_provider.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_client.h"
#import "components/omnibox/common/omnibox_focus_state.h"
#import "components/open_from_clipboard/clipboard_recent_content.h"
#import "ios/chrome/browser/autocomplete/model/autocomplete_scheme_classifier_impl.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_text_controller.h"
#import "ios/chrome/browser/omnibox/public/omnibox_metrics_helper.h"
#import "ios/chrome/browser/omnibox/public/omnibox_ui_features.h"
#import "ios/chrome/browser/omnibox/public/omnibox_util.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_focus_delegate.h"
#import "ios/chrome/browser/omnibox/ui/omnibox_text_field_ios.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/commands/toolbar_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/pasteboard_util.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
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

#pragma mark - Static

// static
std::u16string OmniboxViewIOS::StripJavascriptSchemas(
    const std::u16string& text) {
  const std::u16string kJsPrefix(
      base::StrCat({url::kJavaScriptScheme16, u":"}));

  bool found_JavaScript = false;
  size_t i = 0;
  // Find the index of the first character that isn't whitespace, a control
  // character, or a part of a JavaScript: scheme.
  while (i < text.size()) {
    if (base::IsUnicodeWhitespace(text[i]) || (text[i] < 0x20)) {
      ++i;
    } else {
      if (!base::EqualsCaseInsensitiveASCII(text.substr(i, kJsPrefix.length()),
                                            kJsPrefix)) {
        break;
      }

      // We've found a JavaScript scheme. Continue searching to ensure that
      // strings like "javascript:javascript:alert()" are fully stripped.
      found_JavaScript = true;
      i += kJsPrefix.length();
    }
  }

  // If we found any "JavaScript:" schemes in the text, return the text starting
  // at the first non-whitespace/control character after the last instance of
  // the scheme.
  if (found_JavaScript) {
    return text.substr(i);
  }

  return text;
}

// static
std::u16string OmniboxViewIOS::SanitizeTextForPaste(
    const std::u16string& text) {
  if (text.empty()) {
    return std::u16string();  // Nothing to do.
  }

  size_t end = text.find_first_not_of(base::kWhitespaceUTF16);
  if (end == std::u16string::npos) {
    return u" ";  // Convert all-whitespace to single space.
  }
  // Because `end` points at the first non-whitespace character, the loop
  // below will skip leading whitespace.

  // Reserve space for the sanitized output.
  std::u16string output;
  output.reserve(text.size());  // Guaranteed to be large enough.

  // Copy all non-whitespace sequences.
  // Do not copy trailing whitespace.
  // Copy all other whitespace sequences that do not contain CR/LF.
  // Convert all other whitespace sequences that do contain CR/LF to either ' '
  // or nothing, depending on whether there are any other sequences that do not
  // contain CR/LF.
  bool output_needs_lf_conversion = false;
  bool seen_non_lf_whitespace = false;
  const auto copy_range = [&text, &output](size_t begin, size_t end) {
    output +=
        text.substr(begin, (end == std::u16string::npos) ? end : (end - begin));
  };
  constexpr char16_t kNewline[] = {'\n', 0};
  constexpr char16_t kSpace[] = {' ', 0};
  while (true) {
    // Copy this non-whitespace sequence.
    size_t begin = end;
    end = text.find_first_of(base::kWhitespaceUTF16, begin + 1);
    copy_range(begin, end);

    // Now there is either a whitespace sequence, or the end of the string.
    if (end != std::u16string::npos) {
      // There is a whitespace sequence; see if it contains CR/LF.
      begin = end;
      end = text.find_first_not_of(base::kWhitespaceNoCrLfUTF16, begin);
      if ((end != std::u16string::npos) && (text[end] != '\n') &&
          (text[end] != '\r')) {
        // Found a non-trailing whitespace sequence without CR/LF. Copy it.
        seen_non_lf_whitespace = true;
        copy_range(begin, end);
        continue;
      }
    }

    // `end` either points at the end of the string or a CR/LF.
    if (end != std::u16string::npos) {
      end = text.find_first_not_of(base::kWhitespaceUTF16, end + 1);
    }
    if (end == std::u16string::npos) {
      break;  // Ignore any trailing whitespace.
    }

    // The preceding whitespace sequence contained CR/LF. Convert to a single
    // LF that we'll fix up below the loop.
    output_needs_lf_conversion = true;
    output += '\n';
  }

  // Convert LFs to ' ' or '' depending on whether there were non-LF whitespace
  // sequences.
  if (output_needs_lf_conversion) {
    base::ReplaceChars(output, kNewline,
                       seen_non_lf_whitespace ? kSpace : std::u16string(),
                       &output);
  }

  return StripJavascriptSchemas(output);
}

#pragma mark - Public

OmniboxViewIOS::OmniboxViewIOS(OmniboxTextFieldIOS* field,
                               std::unique_ptr<OmniboxClient> client,
                               ProfileIOS* profile,
                               id<OmniboxCommands> omnibox_focuser,
                               id<ToolbarCommands> toolbar_commands_handler)
    : controller_(std::make_unique<OmniboxControllerIOS>(
          /*view=*/this,
          std::move(client))),
      field_(field),
      ignore_popup_updates_(false) {}

OmniboxViewIOS::~OmniboxViewIOS() = default;

OmniboxEditModelIOS* OmniboxViewIOS::model() {
  return const_cast<OmniboxEditModelIOS*>(
      const_cast<const OmniboxViewIOS*>(this)->model());
}

const OmniboxEditModelIOS* OmniboxViewIOS::model() const {
  return controller_->edit_model();
}

OmniboxControllerIOS* OmniboxViewIOS::controller() {
  return const_cast<OmniboxControllerIOS*>(
      const_cast<const OmniboxViewIOS*>(this)->controller());
}

const OmniboxControllerIOS* OmniboxViewIOS::controller() const {
  return controller_.get();
}

std::u16string OmniboxViewIOS::GetText() const {
  return base::SysNSStringToUTF16([field_ displayedText]);
}

void OmniboxViewIOS::SetUserText(const std::u16string& text) {
  SetUserText(text, true);
}

void OmniboxViewIOS::SetUserText(const std::u16string& text,
                                 bool update_popup) {
  model()->SetUserText(text);
  SetWindowTextAndCaretPos(text, text.length(), update_popup, true);
}

void OmniboxViewIOS::SetWindowTextAndCaretPos(const std::u16string& text,
                                              size_t caret_pos,
                                              bool update_popup,
                                              bool notify_text_changed) {
  [omnibox_text_controller_ setWindowText:text
                                 caretPos:caret_pos
                        startAutocomplete:update_popup
                        notifyTextChanged:notify_text_changed];
}

void OmniboxViewIOS::SetCaretPos(size_t caret_pos) {
  [omnibox_text_controller_ setCaretPos:caret_pos];
}

void OmniboxViewIOS::RevertAll() {
  ignore_popup_updates_ = true;
  // This will clear the model's `user_input_in_progress_`.
  model()->Revert();

  // This will stop the `AutocompleteController`. This should happen after
  // `user_input_in_progress_` is cleared above; otherwise, closing the popup
  // will trigger unnecessary `AutocompleteClassifier::Classify()` calls to
  // try to update the views which are unnecessary since they'll be thrown
  // away during the model revert anyways.
  CloseOmniboxPopup();

  TextChanged();
  ignore_popup_updates_ = false;
}

void OmniboxViewIOS::UpdatePopup() {
  [omnibox_text_controller_ startAutocompleteAfterEdit];
}

void OmniboxViewIOS::CloseOmniboxPopup() {
  controller()->StopAutocomplete(/*clear_result=*/true);
}

void OmniboxViewIOS::OnInlineAutocompleteTextMaybeChanged(
    const std::u16string& user_text,
    const std::u16string& inline_autocompletion) {
  [omnibox_text_controller_
      updateAutocompleteIfTextChanged:user_text
                       autocompletion:inline_autocompletion];
}

void OmniboxViewIOS::SetAdditionalText(const std::u16string& text) {
  [omnibox_text_controller_ setAdditionalText:text];
}

void OmniboxViewIOS::OnBeforePossibleChange() {
  GetState(&state_before_change_);
  marked_text_before_change_ = [[field_ markedText] copy];
}

bool OmniboxViewIOS::OnAfterPossibleChange() {
  State new_state;
  GetState(&new_state);
  // Manually update the selection state after calling GetState().
  new_state.sel_start = current_selection_.location;
  new_state.sel_end = current_selection_.location + current_selection_.length;

  OmniboxViewIOS::StateChanges state_changes =
      GetStateChanges(state_before_change_, new_state);

  const bool something_changed =
      model() && model()->OnAfterPossibleChange(state_changes);

  if (model()) {
    model()->OnChanged();
  }

  // TODO(crbug.com/379695536): Find a different place to call this. Give the
  // omnibox a chance to update the alignment for a text direction change.
  [field_ updateTextDirection];
  return something_changed;
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

bool OmniboxViewIOS::OnWillChange(NSRange range, NSString* new_text) {
  bool ok_to_change = true;

  if ([field_ isPreEditing]) {
    [field_ setClearingPreEditText:YES];

    // Exit the pre-editing state in OnWillChange() instead of OnDidChange(), as
    // that allows IME to continue working. The following code selects the text
    // as if the pre-edit fake selection was real.
    [field_ exitPreEditState];

    // Reset `range` to be of zero-length at location zero, as the field will be
    // now cleared.
    range = NSMakeRange(0, 0);
  }

  // Figure out the old and current (new) selections. Assume the new selection
  // will be of zero-length, located at the end of `new_text`.
  NSRange old_range = range;
  NSRange new_range = NSMakeRange(range.location + [new_text length], 0);

  // We may need to fix up the old and new ranges in the case where autocomplete
  // text was showing. If there is autocomplete text, assume it was selected.
  // If the change is deleting one character from the end of the actual text,
  // disallow the change, but clear the autocomplete text and call OnDidChange
  // directly. If there is autocomplete text AND a text field selection, or if
  // the user entered multiple characters, clear the autocomplete text and
  // pretend it never existed.
  if ([field_ hasAutocompleteText]) {
    bool adding_text = (range.length < [new_text length]);
    bool deleting_text = (range.length > [new_text length]);

    if (adding_text) {
      // TODO(crbug.com/379695322): What about cases where [new_text length] >
      // 1?  This could happen if an IME completion inserts multiple characters
      // at once, or if the user pastes some text in. Let's loosen this test to
      // allow multiple characters, as long as the "old range" ends at the end
      // of the permanent text.
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

  // Store the displayed text. Older version of Chrome used to clear the
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
    std::u16string newText = OmniboxViewIOS::SanitizeTextForPaste(pastedText);
    if (pastedText != newText) {
      [field_ setText:base::SysUTF16ToNSString(newText)];
    }
  }

  // Clear the autocomplete text, since the omnibox model does not expect to see
  // it in OnAfterPossibleChange(). Clearing the text here should not cause
  // flicker as the UI will not get a chance to redraw before the new
  // autocomplete text is set by the model.
  [field_ clearAutocompleteText];
  [field_ setClearingPreEditText:NO];

  // Generally do not notify the autocomplete system of a text change unless the
  // change was a direct result of a user event. One exception is if the marked
  // text changed, which could happen through a delayed IME recognition
  // callback.
  bool proceed_without_user_event = false;

  // The IME exception does not work for Korean text, because Korean does not
  // seem to ever have marked text. It simply replaces or modifies previous
  // characters as you type. Always proceed without user input if the
  // Korean keyboard is currently active.
  NSString* current_language = [[field_ textInputMode] primaryLanguage];

  if ([current_language hasPrefix:@"ko-"]) {
    proceed_without_user_event = true;
  } else {
    NSString* current_marked_text = [field_ markedText];

    // The IME exception kicks in if the current marked text is not equal to the
    // previous marked text. Two nil strings should be considered equal, so
    // There is logic to avoid calling into isEqualToString: in that case.
    proceed_without_user_event =
        (marked_text_before_change_ || current_marked_text) &&
        ![current_marked_text isEqualToString:marked_text_before_change_];
  }

  if (!processing_user_event && !proceed_without_user_event) {
    return;
  }

  OnAfterPossibleChange();
  OnBeforePossibleChange();
}

void OmniboxViewIOS::OnAcceptAutocomplete() {
  current_selection_ = [field_ selectedNSRange];
  OnDidChange(/*processing_user_event=*/true);
}

#pragma mark - Private

void OmniboxViewIOS::GetState(State* state) {
  state->text = GetText();
  GetSelectionBounds(&state->sel_start, &state->sel_end);
}

OmniboxViewIOS::StateChanges OmniboxViewIOS::GetStateChanges(
    const State& before,
    const State& after) {
  OmniboxViewIOS::StateChanges state_changes;
  state_changes.old_text = &before.text;
  state_changes.new_text = &after.text;
  state_changes.new_sel_start = after.sel_start;
  state_changes.new_sel_end = after.sel_end;
  const bool old_sel_empty = before.sel_start == before.sel_end;
  const bool new_sel_empty = after.sel_start == after.sel_end;
  const bool sel_same_ignoring_direction =
      std::min(before.sel_start, before.sel_end) ==
          std::min(after.sel_start, after.sel_end) &&
      std::max(before.sel_start, before.sel_end) ==
          std::max(after.sel_start, after.sel_end);
  state_changes.selection_differs =
      (!old_sel_empty || !new_sel_empty) && !sel_same_ignoring_direction;
  state_changes.text_differs = before.text != after.text;

  // When the user has deleted text, we don't allow inline autocomplete. Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection. (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  state_changes.just_deleted_text =
      before.text.length() > after.text.length() &&
      after.sel_start <= std::min(before.sel_start, before.sel_end);

  return state_changes;
}

void OmniboxViewIOS::TextChanged() {
  model()->OnChanged();
}
