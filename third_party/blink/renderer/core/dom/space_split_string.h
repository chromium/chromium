/*
 * Copyright (C) 2007, 2008, 2010, 2011, 2012 Apple Inc. All rights reserved.
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
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SPACE_SPLIT_STRING_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SPACE_SPLIT_STRING_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class CORE_EXPORT SpaceSplitString {
  USING_FAST_MALLOC(SpaceSplitString);

 public:
  SpaceSplitString() = default;
  explicit SpaceSplitString(const AtomicString& string) { Set(string); }

  bool operator!=(const SpaceSplitString& other) const {
    return data_ != other.data_;
  }

  void Set(const AtomicString&);
  void Clear() { data_ = nullptr; }

  bool Contains(const AtomicString& string) const {
    return data_ && data_->Contains(string);
  }
  bool ContainsAll(const SpaceSplitString& names) const {
    return !names.data_ || (data_ && data_->ContainsAll(*names.data_));
  }
  void Add(const AtomicString&);
  bool Remove(const AtomicString&);
  void Remove(wtf_size_t index);
  void ReplaceAt(wtf_size_t index, const AtomicString&);

  // https://dom.spec.whatwg.org/#concept-ordered-set-serializer
  // The ordered set serializer takes a set and returns the concatenation of the
  // strings in set, separated from each other by U+0020, if set is non-empty,
  // and the empty string otherwise.
  AtomicString SerializeToString() const;

  wtf_size_t size() const { return data_ ? data_->size() : 0; }
  bool IsNull() const { return !data_; }
  const AtomicString& operator[](wtf_size_t i) const { return (*data_)[i]; }

 private:
  class CORE_EXPORT Data : public RefCounted<Data> {
    USING_FAST_MALLOC(Data);

   public:
    static scoped_refptr<Data> Create(const AtomicString&);
    static scoped_refptr<Data> CreateUnique(const Data&);

    ~Data();

    bool Contains(const AtomicString& string) const {
      return vector_.Contains(string);
    }

    bool ContainsAll(Data&);

    void Add(const AtomicString&);
    void Remove(unsigned index);

    bool IsUnique() const { return key_string_.IsNull(); }
    wtf_size_t size() const { return vector_.size(); }
    const AtomicString& operator[](wtf_size_t i) const { return vector_[i]; }
    AtomicString& operator[](wtf_size_t i) { return vector_[i]; }

   private:
    explicit Data(const AtomicString&);
    explicit Data(const Data&);

    void CreateVector(const AtomicString&);
    template <typename CharacterType>
    inline void CreateVector(const AtomicString&,
                             const CharacterType*,
                             unsigned);

    AtomicString key_string_;
    Vector<AtomicString, 4> vector_;
  };
  typedef HashMap<AtomicString, Data*> DataMap;

  static DataMap& SharedDataMap();

  void EnsureUnique() {
    if (data_ && !data_->IsUnique())
      data_ = Data::CreateUnique(*data_);
  }

  scoped_refptr<Data> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_DOM_SPACE_SPLIT_STRING_H_
