/*
 * Copyright (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2007, 2008, 2011, 2012 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "third_party/blink/renderer/core/dom/space_split_string.h"

#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/character_visitor.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/thread_specific.h"

namespace blink {

// https://dom.spec.whatwg.org/#concept-ordered-set-parser
template <typename CharacterType>
ALWAYS_INLINE void SpaceSplitString::Data::CreateVector(
    const AtomicString& source,
    base::span<const CharacterType> characters) {
  DCHECK(vector_.empty());
  HashSet<AtomicString> token_set;
  size_t start = 0;
  while (true) {
    while (start < characters.size() &&
           IsHTMLSpace<CharacterType>(characters[start])) {
      ++start;
    }
    if (start >= characters.size()) {
      break;
    }
    size_t end = start + 1;
    while (end < characters.size() &&
           IsNotHTMLSpace<CharacterType>(characters[end])) {
      ++end;
    }

    if (start == 0 && end == characters.size()) {
      vector_.push_back(source);
      return;
    }

    AtomicString token(characters.subspan(start, end - start));
    // We skip adding |token| to |token_set| for the first token to reduce the
    // cost of HashSet<>::insert(), and adjust |token_set| when the second
    // unique token is found.
    if (vector_.size() == 0) {
      vector_.push_back(std::move(token));
    } else if (vector_.size() == 1) {
      if (vector_[0] != token) {
        token_set.insert(vector_[0]);
        token_set.insert(token);
        vector_.push_back(std::move(token));
      }
    } else if (token_set.insert(token).is_new_entry) {
      vector_.push_back(std::move(token));
    }

    start = end + 1;
  }
}

void SpaceSplitString::Data::CreateVector(const AtomicString& string) {
  WTF::VisitCharacters(string,
                       [&](auto chars) { CreateVector(string, chars); });
}

bool SpaceSplitString::Data::ContainsAll(Data& other) {
  if (this == &other)
    return true;

  wtf_size_t this_size = vector_.size();
  wtf_size_t other_size = other.vector_.size();
  for (wtf_size_t i = 0; i < other_size; ++i) {
    const AtomicString& name = other.vector_[i];
    wtf_size_t j;
    for (j = 0; j < this_size; ++j) {
      if (vector_[j] == name)
        break;
    }
    if (j == this_size)
      return false;
  }
  return true;
}

void SpaceSplitString::Data::Add(const AtomicString& string) {
  DCHECK(!MightBeShared());
  DCHECK(!Contains(string));
  vector_.push_back(string);
}

void SpaceSplitString::Data::Remove(unsigned index) {
  DCHECK(!MightBeShared());
  vector_.EraseAt(index);
}

void SpaceSplitString::Add(const AtomicString& string) {
  if (Contains(string))
    return;
  EnsureUnique();
  if (data_)
    data_->Add(string);
  else
    data_ = Data::Create(string);
}

void SpaceSplitString::Remove(const AtomicString& string) {
  if (!data_) {
    return;
  }
  unsigned i = 0;
  bool changed = false;
  while (i < data_->size()) {
    if ((*data_)[i] == string) {
      if (!changed)
        EnsureUnique();
      data_->Remove(i);
      changed = true;
      continue;
    }
    ++i;
  }
}

void SpaceSplitString::Remove(wtf_size_t index) {
  DCHECK_LT(index, size());
  EnsureUnique();
  data_->Remove(index);
}

void SpaceSplitString::ReplaceAt(wtf_size_t index, const AtomicString& token) {
  DCHECK_LT(index, data_->size());
  EnsureUnique();
  (*data_)[index] = token;
}

AtomicString SpaceSplitString::SerializeToString() const {
  wtf_size_t size = this->size();
  if (size == 0)
    return g_empty_atom;
  if (size == 1)
    return (*data_)[0];
  StringBuilder builder;
  builder.Append((*data_)[0]);
  for (wtf_size_t i = 1; i < size; ++i) {
    builder.Append(' ');
    builder.Append((*data_)[i]);
  }
  return builder.ToAtomicString();
}

// static
SpaceSplitString::DataMap& SpaceSplitString::SharedDataMap() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<Persistent<DataMap>>,
                                  static_map_holder, {});
  Persistent<DataMap>& map = *static_map_holder;
  if (!map) [[unlikely]] {
    map = MakeGarbageCollected<DataMap>();
    LEAK_SANITIZER_IGNORE_OBJECT(&map);
  }
  return *map;
}

void SpaceSplitString::Set(const AtomicString& input_string) {
  if (input_string.IsNull()) {
    Clear();
    return;
  }
  data_ = Data::Create(input_string);
}

SpaceSplitString::Data* SpaceSplitString::Data::Create(
    const AtomicString& string) {
  auto result = SharedDataMap().insert(string, nullptr);
  SpaceSplitString::Data* data = result.stored_value->value;
  if (result.is_new_entry) {
    data = MakeGarbageCollected<SpaceSplitString::Data>(string);
    result.stored_value->value = data;
  }
  return data;
}

SpaceSplitString::Data* SpaceSplitString::Data::CreateUnique(
    const Data& other) {
  return MakeGarbageCollected<SpaceSplitString::Data>(other);
}

// This constructor always creates a "shared" (non-unique) Data object.
SpaceSplitString::Data::Data(const AtomicString& string)
    : might_be_shared_(true) {
  DCHECK(!string.IsNull());
  CreateVector(string);
}

// This constructor always creates a non-"shared" (unique) Data object.
SpaceSplitString::Data::Data(const SpaceSplitString::Data& other)
    : might_be_shared_(false), vector_(other.vector_) {}

std::ostream& operator<<(std::ostream& ostream, const SpaceSplitString& str) {
  return ostream << str.SerializeToString();
}

}  // namespace blink
