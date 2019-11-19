/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2009, 2010, 2011 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "third_party/blink/renderer/core/html/forms/type_ahead.h"

#include "third_party/blink/renderer/core/events/keyboard_event.h"
#include "third_party/blink/renderer/platform/wtf/text/character_names.h"

namespace blink {

TypeAhead::TypeAhead(TypeAheadDataSource* data_source)
    : data_source_(data_source), repeating_char_(0) {}

constexpr base::TimeDelta kTypeAheadTimeout = base::TimeDelta::FromSecondsD(1);

static String StripLeadingWhiteSpace(const String& string) {
  unsigned length = string.length();

  unsigned i;
  for (i = 0; i < length; ++i) {
    if (string[i] != kNoBreakSpaceCharacter && !IsSpaceOrNewline(string[i]))
      break;
  }

  return string.Substring(i, length - i);
}

int TypeAhead::HandleEvent(const KeyboardEvent& event,
                           MatchModeFlags match_mode) {
  if (last_type_time_) {
    if (event.PlatformTimeStamp() < *last_type_time_)
      return -1;

    if (event.PlatformTimeStamp() - *last_type_time_ > kTypeAheadTimeout)
      buffer_.Clear();
  } else {
    // If |last_type_time_| is null, there should be no type ahead session in
    // progress. Thus, |buffer_|, which represents a partial match, should be
    // empty.
    DCHECK(buffer_.IsEmpty());
  }
  last_type_time_ = event.PlatformTimeStamp();

  UChar c = event.charCode();
  buffer_.Append(c);

  int option_count = data_source_->OptionCount();
  if (option_count < 1)
    return -1;

  int search_start_offset = 1;
  String prefix;
  if (match_mode & kCycleFirstChar && c == repeating_char_) {
    // The user is likely trying to cycle through all the items starting
    // with this character, so just search on the character.
    prefix = String(&c, 1);
    repeating_char_ = c;
  } else if (match_mode & kMatchPrefix) {
    prefix = buffer_.ToString();
    if (buffer_.length() > 1) {
      repeating_char_ = 0;
      search_start_offset = 0;
    } else {
      repeating_char_ = c;
    }
  }

  if (!prefix.IsEmpty()) {
    int selected = data_source_->IndexOfSelectedOption();
    int index = (selected < 0 ? 0 : selected) + search_start_offset;
    index %= option_count;

    // Compute a case-folded copy of the prefix string before beginning the
    // search for a matching element. This code uses foldCase to work around the
    // fact that String::startWith does not fold non-ASCII characters. This code
    // can be changed to use startWith once that is fixed.
    String prefix_with_case_folded(prefix.FoldCase());
    for (int i = 0; i < option_count; ++i, index = (index + 1) % option_count) {
      // Fold the option string and check if its prefix is equal to the folded
      // prefix.
      String text = data_source_->OptionAtIndex(index);
      if (StripLeadingWhiteSpace(text).FoldCase().StartsWith(
              prefix_with_case_folded))
        return index;
    }
  }

  if (match_mode & kMatchIndex) {
    bool ok = false;
    int index = buffer_.ToString().ToInt(&ok);
    if (index > 0 && index <= option_count)
      return index - 1;
  }
  return -1;
}

bool TypeAhead::HasActiveSession(const KeyboardEvent& event) {
  if (!last_type_time_)
    return false;
  base::TimeDelta delta = event.PlatformTimeStamp() - *last_type_time_;
  return delta <= kTypeAheadTimeout;
}

void TypeAhead::ResetSession() {
  last_type_time_.reset();
  buffer_.Clear();
}

}  // namespace blink
