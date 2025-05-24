// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/names_map.h"

#include <memory>

#include "third_party/blink/renderer/core/dom/space_split_string.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

NamesMap::NamesMap(const AtomicString& string) {
  Set(string);
}

void NamesMap::Set(const AtomicString& source) {
  if (source.IsNull()) {
    Clear();
    return;
  }
  if (source.Is8Bit()) {
    Set(source.Span8());
    return;
  }

  Set(source.Span16());
}

void NamesMap::Add(const AtomicString& key, const AtomicString& value) {
  // AddResult
  auto add_result = data_.insert(key, nullptr);
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        MakeGarbageCollected<SpaceSplitStringWrapper>();
  }
  add_result.stored_value->value->value.Add(value);
}

// Parser for HTML exportparts attribute. See
// http://drafts.csswg.org/css-shadow-parts/.
//
// Summary is that we are parsing a comma-separated list of part-mappings. A
// part mapping is a part name or 2 colon-separated part names. If any
// part-mapping is invalid, we ignore it and continue parsing after the next
// comma. Part names are delimited by space, comma or colon. Apart from that,
// whitespace is not significant.

// The states that can occur while parsing the part map and their transitions.
// A "+" indicates that this transition should consume the current character.  A
// "*" indicates that this is invalid input. In general invalid input causes us
// to reject the current mapping and returns us to searching for a comma.
enum State {
  kPreKey,     // Searching for the start of a key:
               //   space, comma -> kPreKey+
               //   colon* -> kError+
               //   else -> kKey
  kKey,        // Searching for the end of a key:
               //   comma -> kPreKey+
               //   colon -> kPreValue+
               //   space -> kPostKey+
               //   else -> kKey+
  kPostKey,    // Searching for a delimiter:
               //   comma -> kPreKey+
               //   colon -> kPreValue+
               //   space -> kPostKey+
               //   else* -> kError+
  kPreValue,   // Searching for the start of a value:
               //   colon* -> kPostValue+
               //   comma* -> kPreKey+
               //   space -> kPreValue+
               //   else -> kValue+
  kValue,      // Searching for the end of a value:
               //   comma -> kPreKey+
               //   space -> kPostValue+
               //   colon* -> kError+
               //   else -> kValue+
  kPostValue,  // Searching for the comma after the value:
               //   comma -> kPreKey+
               //   colon*, else* -> kError+
  kError,      // Searching for the comma after an error:
               //   comma -> kPreKey+
               //   else* -> kError+
};

template <typename CharacterType>
void NamesMap::Set(base::span<const CharacterType> characters) {
  Clear();

  // The character we are examining.
  size_t cur = 0;
  // The start of the current token.
  size_t start = 0;
  State state = kPreKey;
  // The key and value are held here until we succeed in parsing a valid
  // part-mapping.
  AtomicString key;
  AtomicString value;
  while (cur < characters.size()) {
    const CharacterType current_char = characters[cur];
    // All cases break, ensuring that some input is consumed and we avoid
    // an infinite loop.
    //
    // The only state which should set a value for key is kKey, as we leave the
    // state.
    switch (state) {
      case kPreKey:
        // Skip any number of spaces, commas. When we find something else, it is
        // the start of a key.
        if (IsHTMLSpaceOrComma<CharacterType>(current_char)) {
          break;
        }
        // Colon is invalid here.
        if (IsColon<CharacterType>(current_char)) {
          state = kError;
          break;
        }
        start = cur;
        state = kKey;
        break;
      case kKey:
        // At a comma this was a key without a value, the implicit value is the
        // same as the key.
        if (IsComma<CharacterType>(current_char)) {
          key = AtomicString(characters.subspan(start, cur - start));
          Add(key, key);
          state = kPreKey;
          // At a colon, we have found the end of the key and we expect a value.
        } else if (IsColon<CharacterType>(current_char)) {
          key = AtomicString(characters.subspan(start, cur - start));
          state = kPreValue;
          // At a space, we have found the end of the key.
        } else if (IsHTMLSpace<CharacterType>(current_char)) {
          key = AtomicString(characters.subspan(start, cur - start));
          state = kPostKey;
        }
        break;
      case kPostKey:
        // At a comma this was a key without a value, the implicit value is the
        // same as the key.
        if (IsComma<CharacterType>(current_char)) {
          Add(key, key);
          state = kPreKey;
          // At a colon this was a key with a value, we expect a value.
        } else if (IsColon<CharacterType>(current_char)) {
          state = kPreValue;
          // Anything else except space is invalid.
        } else if (!IsHTMLSpace<CharacterType>(current_char)) {
          key = g_null_atom;
          state = kError;
        }
        break;
      case kPreValue:
        // Colon is invalid.
        if (IsColon<CharacterType>(current_char)) {
          state = kError;
          // Comma is invalid.
        } else if (IsComma<CharacterType>(current_char)) {
          state = kPreKey;
          // Space is ignored.
        } else if (IsHTMLSpace<CharacterType>(current_char)) {
          break;
          // If we reach a non-space character, we have found the start of the
          // value.
        } else {
          start = cur;
          state = kValue;
        }
        break;
      case kValue:
        // At a comma, we have found the end of the value and expect
        // the next key.
        if (IsComma<CharacterType>(current_char)) {
          value = AtomicString(characters.subspan(start, cur - start));
          Add(key, value);
          state = kPreKey;
          // At a space, we have found the end of the value, store it.
        } else if (IsHTMLSpace<CharacterType>(current_char) ||
                   IsColon<CharacterType>(current_char)) {
          value = AtomicString(characters.subspan(start, cur - start));
          state = kPostValue;
          // A colon is invalid.
        } else if (IsColon<CharacterType>(current_char)) {
          state = kError;
        }
        break;
      case kPostValue:
        // At a comma, accept what we have and start looking for the next key.
        if (IsComma<CharacterType>(current_char)) {
          Add(key, value);
          state = kPreKey;
          // Anything else except a space is invalid.
        } else if (!IsHTMLSpace<CharacterType>(current_char)) {
          state = kError;
        }
        break;
      case kError:
        // At a comma, start looking for the next key.
        if (IsComma<CharacterType>(current_char)) {
          state = kPreKey;
        }
        // Anything else is consumed.
        break;
    }

    ++cur;
  }

  // We have reached the end of the string, add whatever we had into the map.
  switch (state) {
    case kPreKey:
      break;
    case kKey:
      // The string ends with a key.
      key = AtomicString(characters.subspan(start, cur - start));
      [[fallthrough]];
    case kPostKey:
      // The string ends with a key.
      Add(key, key);
      break;
    case kPreValue:
      break;
    case kValue:
      // The string ends with a value.
      value = AtomicString(characters.subspan(start, cur - start));
      [[fallthrough]];
    case kPostValue:
      Add(key, value);
      break;
    case kError:
      break;
  }
}

SpaceSplitString* NamesMap::Get(const AtomicString& key) const {
  auto it = data_.find(key);
  return it != data_.end() ? &it->value->value : nullptr;
}

}  // namespace blink
