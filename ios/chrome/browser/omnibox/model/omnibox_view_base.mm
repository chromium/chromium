// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/model/omnibox_view_base.h"

#import <algorithm>
#import <memory>
#import <string>
#import <utility>

#import "base/feature_list.h"
#import "base/metrics/histogram_macros.h"
#import "base/strings/strcat.h"
#import "base/strings/string_util.h"
#import "base/strings/utf_string_conversions.h"
#import "build/build_config.h"
#import "components/bookmarks/browser/bookmark_model.h"
#import "components/omnibox/browser/autocomplete_controller.h"
#import "components/omnibox/browser/autocomplete_input.h"
#import "components/omnibox/browser/autocomplete_match.h"
#import "components/omnibox/browser/autocomplete_match_type.h"
#import "components/omnibox/browser/location_bar_model.h"
#import "components/omnibox/browser/omnibox_field_trial.h"
#import "components/omnibox/common/omnibox_features.h"
#import "components/search/search.h"
#import "components/search_engines/template_url_service.h"
#import "extensions/buildflags/buildflags.h"
#import "ios/chrome/browser/omnibox/model/omnibox_controller_ios.h"
#import "ios/chrome/browser/omnibox/model/omnibox_edit_model_ios.h"
#import "ui/base/l10n/l10n_util.h"
#import "ui/base/ui_base_features.h"
#import "url/url_constants.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
// GN doesn't understand conditional includes, so we need nogncheck here.
#import "extensions/common/constants.h"  // nogncheck
#endif

// static
std::u16string OmniboxViewBase::StripJavascriptSchemas(
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
std::u16string OmniboxViewBase::SanitizeTextForPaste(
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
        // Found a non-trailing whitespace sequence without CR/LF.  Copy it.
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

    // The preceding whitespace sequence contained CR/LF.  Convert to a single
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

OmniboxViewBase::~OmniboxViewBase() = default;

void OmniboxViewBase::SetUserText(const std::u16string& text) {
  SetUserText(text, true);
}

void OmniboxViewBase::SetUserText(const std::u16string& text,
                                  bool update_popup) {
  model()->SetUserText(text);
  SetWindowTextAndCaretPos(text, text.length(), update_popup, true);
}

void OmniboxViewBase::RevertAll() {
  // This will clear the model's `user_input_in_progress_`.
  model()->Revert();

  // This will stop the `AutocompleteController`. This should happen after
  // `user_input_in_progress_` is cleared above; otherwise, closing the popup
  // will trigger unnecessary `AutocompleteClassifier::Classify()` calls to
  // try to update the views which are unnecessary since they'll be thrown
  // away during the model revert anyways.
  CloseOmniboxPopup();

  TextChanged();
}

void OmniboxViewBase::CloseOmniboxPopup() {
  controller()->StopAutocomplete(/*clear_result=*/true);
}

void OmniboxViewBase::GetState(State* state) {
  state->text = GetText();
  GetSelectionBounds(&state->sel_start, &state->sel_end);
}

OmniboxViewBase::StateChanges OmniboxViewBase::GetStateChanges(
    const State& before,
    const State& after) {
  OmniboxViewBase::StateChanges state_changes;
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

  // When the user has deleted text, we don't allow inline autocomplete.  Make
  // sure to not flag cases like selecting part of the text and then pasting
  // (or typing) the prefix of that selection.  (We detect these by making
  // sure the caret, which should be after any insertion, hasn't moved
  // forward of the old selection start.)
  state_changes.just_deleted_text =
      before.text.length() > after.text.length() &&
      after.sel_start <= std::min(before.sel_start, before.sel_end);

  return state_changes;
}

OmniboxViewBase::OmniboxViewBase(std::unique_ptr<OmniboxClient> client)
    : controller_(std::make_unique<OmniboxControllerIOS>(
          /*view=*/this,
          std::move(client))) {}

OmniboxEditModelIOS* OmniboxViewBase::model() {
  return const_cast<OmniboxEditModelIOS*>(
      const_cast<const OmniboxViewBase*>(this)->model());
}

const OmniboxEditModelIOS* OmniboxViewBase::model() const {
  return controller_->edit_model();
}

OmniboxControllerIOS* OmniboxViewBase::controller() {
  return const_cast<OmniboxControllerIOS*>(
      const_cast<const OmniboxViewBase*>(this)->controller());
}

const OmniboxControllerIOS* OmniboxViewBase::controller() const {
  return controller_.get();
}

void OmniboxViewBase::TextChanged() {
  model()->OnChanged();
}
