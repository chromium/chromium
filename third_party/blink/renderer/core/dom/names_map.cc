// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "third_party/blink/renderer/core/dom/names_map.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
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
    Set(source, source.Characters8());
    return;
  }

  Set(source, source.Characters16());
}

void NamesMap::Add(const AtomicString& key, const AtomicString& value) {
  // AddResult
  auto add_result = data_.insert(key, base::Optional<SpaceSplitString>());
  if (add_result.is_new_entry) {
    add_result.stored_value->value =
        base::make_optional<SpaceSplitString>(SpaceSplitString());
  }
  add_result.stored_value->value.value().Add(value);
}

// Parser for HTML partmap attribute. See
// http://drafts.csswg.org/css-shadow-parts/ but with modifications.
//
// This implements a modified version where the key is first and the value is
// second and => is not used to separate key and value. It also allows an ident
// token on its own as a short-hand for forwarding with the same name.

// The states that can occur while parsing the part map and their transitions.
// A "+" indicates that this transition should consume the current character.
// A "*" indicates that this is invalid input, usually the decision here is
// to just do our best and recover gracefully.
enum State {
  kPreKey,     // Searching for the start of a key:
               //   space, comma, colon* -> kPreKey+
               //   else -> kKey
  kKey,        // Searching for the end of a key:
               //   comma -> kPreKey+
               //   colon -> kPreValue+
               //   space -> kPostKey+
               //   else -> kKey+
  kPostKey,    // Searching for a delimiter:
               //   comma -> kPreKey+
               //   colon -> kPreValue+
               //   space, else* -> kPostKey+
  kPreValue,   // Searching for the start of a value:
               //   comma -> kPreKey+
               //   colon*, space -> kPreValue+
               //   else -> kValue+
  kValue,      // Searching for the end of a value:
               //   comma -> kPreKey+
               //   colon*, space -> kPostValue+
               //   else -> kValue+
  kPostValue,  // Searching for the comma after the value:
               //   comma -> kPreKey+
               //   colon*, else -> kPostValue+
};

template <typename CharacterType>
void NamesMap::Set(const AtomicString& source,
                   const CharacterType* characters) {
  Clear();
  unsigned length = source.length();

  // The character we are examining.
  unsigned cur = 0;
  // The start of the current token.
  unsigned start = 0;
  State state = kPreKey;
  AtomicString key;
  while (cur < length) {
    // Almost all cases break, ensuring that some input is consumed and we avoid
    // an infinite loop. For the few transitions which should happen without
    // consuming input we fall through into the new state's case. This makes it
    // easy to see the cases which don't consume input and check that they lead
    // to a case which does.
    //
    // The only state which should set a value for key is kKey, as we leave the
    // state.
    switch (state) {
      case kPreKey:
        // Skip any number of spaces, commas and colons. When we find something
        // else, it is the start of a key.
        if ((IsHTMLSpaceOrComma<CharacterType>(characters[cur]) ||
             IsColon<CharacterType>(characters[cur]))) {
          break;
        }
        start = cur;
        state = kKey;
        FALLTHROUGH;
      case kKey:
        // At a comma this was a key without a value, the implicit value is the
        // same as the key.
        if (IsComma<CharacterType>(characters[cur])) {
          key = AtomicString(characters + start, cur - start);
          Add(key, key);
          state = kPreKey;
          // At a colon, we have found the end of the key and we expect a value.
        } else if (IsColon<CharacterType>(characters[cur])) {
          key = AtomicString(characters + start, cur - start);
          state = kPreValue;
          // At a space, we have found the end of the key.
        } else if (IsHTMLSpace<CharacterType>(characters[cur])) {
          key = AtomicString(characters + start, cur - start);
          state = kPostKey;
        }
        break;
      case kPostKey:
        // At a comma this was a key without a value, the implicit value is the
        // same as the key.
        if (IsComma<CharacterType>(characters[cur])) {
          Add(key, key);
          state = kPreKey;
          // At a colon this was a key with a value, we expect a value.
        } else if (IsColon<CharacterType>(characters[cur])) {
          state = kPreValue;
        }
        // Spaces should be consumed. We consume other characters too
        // although the input is invalid
        break;
      case kPreValue:
        // At a comma this was a key without a value, the implicit value is the
        // same as the key.
        if (IsComma<CharacterType>(characters[cur])) {
          Add(key, key);
          state = kPreKey;
          // If we reach a non-space character, we have found the start of the
          // value.
        } else if (IsColon<CharacterType>(characters[cur]) ||
                   IsHTMLSpace<CharacterType>(characters[cur])) {
          break;
        } else {
          start = cur;
          state = kValue;
        }
        break;
      case kValue:
        // At a comma, we have found the end of the value and expect
        // the next key.
        if (IsComma<CharacterType>(characters[cur])) {
          Add(key, AtomicString(characters + start, cur - start));
          state = kPreKey;
          // At a space or colon, we have found the end of the value,
          // although a colon is invalid here.
        } else if (IsHTMLSpace<CharacterType>(characters[cur]) ||
                   IsColon<CharacterType>(characters[cur])) {
          Add(key, AtomicString(characters + start, cur - start));
          state = kPostValue;
        }
        break;
      case kPostValue:
        // At a comma, we start looking for the next key.
        if (IsComma<CharacterType>(characters[cur])) {
          state = kPreKey;
        }
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
      key = AtomicString(characters + start, cur - start);
      FALLTHROUGH;
    case kPostKey:
    case kPreValue:
      // The string ends after a key but with nothing else useful.
      Add(key, key);
      break;
    case kValue:
      // The string ends with a value.
      Add(key, AtomicString(characters + start, cur - start));
      break;
    case kPostValue:
      break;
  }
}

base::Optional<SpaceSplitString> NamesMap::Get(const AtomicString& key) const {
  auto it = data_.find(key);
  return it != data_.end() ? it->value : base::nullopt;
}

}  // namespace blink
