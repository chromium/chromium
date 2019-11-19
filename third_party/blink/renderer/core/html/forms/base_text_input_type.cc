/*
 * This file is part of the WebKit project.
 *
 * Copyright (C) 2009 Michelangelo De Simone <micdesim@gmail.com>
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "third_party/blink/renderer/core/html/forms/base_text_input_type.h"

#include "third_party/blink/renderer/bindings/core/v8/script_regexp.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"

namespace blink {

BaseTextInputType::BaseTextInputType(HTMLInputElement& element)
    : TextFieldInputType(element) {}

BaseTextInputType::~BaseTextInputType() = default;

int BaseTextInputType::MaxLength() const {
  return GetElement().maxLength();
}

int BaseTextInputType::MinLength() const {
  return GetElement().minLength();
}

bool BaseTextInputType::TooLong(
    const String& value,
    TextControlElement::NeedsToCheckDirtyFlag check) const {
  int max = GetElement().maxLength();
  if (max < 0)
    return false;
  if (check == TextControlElement::kCheckDirtyFlag) {
    // Return false for the default value or a value set by a script even if
    // it is longer than maxLength.
    if (!GetElement().HasDirtyValue() || !GetElement().LastChangeWasUserEdit())
      return false;
  }
  return value.length() > static_cast<unsigned>(max);
}

bool BaseTextInputType::TooShort(
    const String& value,
    TextControlElement::NeedsToCheckDirtyFlag check) const {
  int min = GetElement().minLength();
  if (min <= 0)
    return false;
  if (check == TextControlElement::kCheckDirtyFlag) {
    // Return false for the default value or a value set by a script even if
    // it is shorter than minLength.
    if (!GetElement().HasDirtyValue() || !GetElement().LastChangeWasUserEdit())
      return false;
  }
  // An empty string is excluded from minlength check.
  unsigned len = value.length();
  return len > 0 && len < static_cast<unsigned>(min);
}

bool BaseTextInputType::PatternMismatch(const String& value) const {
  const AtomicString& raw_pattern =
      GetElement().FastGetAttribute(html_names::kPatternAttr);
  // Empty values can't be mismatched
  if (raw_pattern.IsNull() || value.IsEmpty())
    return false;
  if (!regexp_ || pattern_for_regexp_ != raw_pattern) {
    std::unique_ptr<ScriptRegexp> raw_regexp(
        new ScriptRegexp(raw_pattern, kTextCaseSensitive, kMultilineDisabled,
                         ScriptRegexp::UTF16));
    if (!raw_regexp->IsValid()) {
      GetElement().GetDocument().AddConsoleMessage(
          ConsoleMessage::Create(mojom::ConsoleMessageSource::kRendering,
                                 mojom::ConsoleMessageLevel::kError,
                                 "Pattern attribute value " + raw_pattern +
                                     " is not a valid regular expression: " +
                                     raw_regexp->ExceptionMessage()));
      regexp_.reset(raw_regexp.release());
      pattern_for_regexp_ = raw_pattern;
      return false;
    }
    String pattern = "^(?:" + raw_pattern + ")$";
    regexp_.reset(new ScriptRegexp(pattern, kTextCaseSensitive,
                                   kMultilineDisabled, ScriptRegexp::UTF16));
    pattern_for_regexp_ = raw_pattern;
  } else if (!regexp_->IsValid()) {
    return false;
  }

  int match_length = 0;
  int value_length = value.length();
  int match_offset = regexp_->Match(value, 0, &match_length);
  bool mismatched = match_offset != 0 || match_length != value_length;
  return mismatched;
}

bool BaseTextInputType::SupportsPlaceholder() const {
  return true;
}

bool BaseTextInputType::SupportsSelectionAPI() const {
  return true;
}

}  // namespace blink
