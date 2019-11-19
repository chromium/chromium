// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ATOMIC_STRING_TABLE_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_TEXT_ATOMIC_STRING_TABLE_H_

#include "base/macros.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/string_impl.h"
#include "third_party/blink/renderer/platform/wtf/threading.h"
#include "third_party/blink/renderer/platform/wtf/wtf_export.h"

namespace WTF {

// The underlying storage that keeps the map of unique AtomicStrings. This is
// not thread safe and each Threading has one.
class WTF_EXPORT AtomicStringTable final {
  USING_FAST_MALLOC(AtomicStringTable);

 public:
  AtomicStringTable();
  ~AtomicStringTable();

  // Gets the shared table for the current thread.
  static AtomicStringTable& Instance() {
    return WtfThreading().GetAtomicStringTable();
  }

  // Used by system initialization to preallocate enough storage for all of
  // the static strings.
  void ReserveCapacity(unsigned size);

  // Inserting strings into the table. Note that the return value from adding
  // a UChar string may be an LChar string as the table will attempt to
  // convert the string to save memory if possible.
  StringImpl* Add(StringImpl*);
  scoped_refptr<StringImpl> Add(const LChar* chars, unsigned length);
  scoped_refptr<StringImpl> Add(const UChar* chars, unsigned length);

  // Adding UTF8.
  // Returns null if the characters contain invalid utf8 sequences.
  // Pass null for the charactersEnd to automatically detect the length.
  scoped_refptr<StringImpl> AddUTF8(const char* characters_start,
                                    const char* characters_end);

  // This is for ~StringImpl to unregister a string before destruction since
  // the table is holding weak pointers. It should not be used directly.
  void Remove(StringImpl*);

 private:
  template <typename T, typename HashTranslator>
  inline scoped_refptr<StringImpl> AddToStringTable(const T& value);

  HashSet<StringImpl*> table_;

  DISALLOW_COPY_AND_ASSIGN(AtomicStringTable);
};

}  // namespace WTF

using WTF::AtomicStringTable;

#endif
