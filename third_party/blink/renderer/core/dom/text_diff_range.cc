// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/text_diff_range.h"

#include "third_party/blink/renderer/platform/wtf/text/string_view.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

#if EXPENSIVE_DCHECKS_ARE_ON()
void TextDiffRange::CheckValid(const String& old_text,
                               const String& new_text) const {
  DCHECK_EQ(old_text.length() - old_size + new_size, new_text.length())
      << old_text << " => " << new_text << " " << old_text.length() << "-"
      << old_size << "+" << new_size << " => " << new_text.length();
  DCHECK_EQ(StringView(old_text, 0, offset), StringView(new_text, 0, offset))
      << old_text << " => " << new_text;
  DCHECK_EQ(StringView(old_text, OldEndOffset()),
            StringView(new_text, NewEndOffset()))
      << old_text << " => " << new_text;
}
#endif  // EXPENSIVE_DCHECKS_ARE_ON()

}  // namespace blink
