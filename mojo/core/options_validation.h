// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Functions to help with verifying various |Mojo...Options| structs from the
// (public, C) API. These are "extensible" structs, which all have |struct_size|
// as their first member. All fields (other than |struct_size|) are optional,
// but any |flags| specified must be known to the system (otherwise, an error of
// |MOJO_RESULT_UNIMPLEMENTED| should be returned).

#ifndef MOJO_CORE_OPTIONS_VALIDATION_H_
#define MOJO_CORE_OPTIONS_VALIDATION_H_

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "mojo/core/system_impl_export.h"
#include "mojo/public/c/system/types.h"

namespace mojo {
namespace core {

template <class Options>
class UserOptionsReader {
 public:
  // Constructor from a |const* Options| (which it checks -- this constructor
  // has side effects!).
  // Note: We initialize |options_reader_| without checking, since we do a check
  // in |GetSizeForReader()|.
  explicit UserOptionsReader(const Options* options) {
    CHECK(options && IsAligned<MOJO_ALIGNOF(Options)>(options));
    options_ = GetSizeForReader(options) == 0 ? nullptr : options;
    static_assert(offsetof(Options, struct_size) == 0,
                  "struct_size not first member of Options");
    // TODO(vtl): Enable when MSVC supports this (C++11 extended sizeof):
    //   static_assert(sizeof(Options::struct_size) == sizeof(uint32_t),
    //                 "Options::struct_size not a uint32_t");
    // (Or maybe assert that its type is uint32_t?)
  }

  UserOptionsReader(const UserOptionsReader&) = delete;
  UserOptionsReader& operator=(const UserOptionsReader&) = delete;

  bool is_valid() const { return !!options_; }

  const Options& options() const {
    DCHECK(is_valid());
    return *options_;
  }

  // Checks that the given (variable-size) |options| passed to the constructor
  // (plausibly) has a member at the given offset with the given size. You
  // probably want to use |OPTIONS_STRUCT_HAS_MEMBER()| instead.
  bool HasMember(size_t offset, size_t size) const {
    DCHECK(is_valid());
    // We assume that |offset| and |size| are reasonable, since they should come
    // from |offsetof(Options, some_member)| and |sizeof(Options::some_member)|,
    // respectively.
    return options().struct_size >= offset + size;
  }

 private:
  static inline size_t GetSizeForReader(const Options* options) {
    uint32_t struct_size = *reinterpret_cast<const uint32_t*>(options);
    if (struct_size < sizeof(uint32_t))
      return 0;

    return std::min(static_cast<size_t>(struct_size), sizeof(Options));
  }

  template <size_t alignment>
  static bool IsAligned(const void* pointer) {
    return reinterpret_cast<uintptr_t>(pointer) % alignment == 0;
  }

  raw_ptr<const Options> options_;
};

// Macro to invoke |UserOptionsReader<Options>::HasMember()| parametrized by
// member name instead of offset and size.
//
// (We can't just give |HasMember()| a member pointer template argument instead,
// since there's no good/strictly-correct way to get an offset from that.)
//
// TODO(vtl): With C++11, use |sizeof(Options::member)| instead of (the
// contortion below). We might also be able to pull out the type |Options| from
// |reader| (using |decltype|) instead of requiring a parameter.
#define OPTIONS_STRUCT_HAS_MEMBER(Options, member, reader) \
  reader.HasMember(offsetof(Options, member), sizeof(reader.options().member))

}  // namespace core
}  // namespace mojo

#endif  // MOJO_CORE_OPTIONS_VALIDATION_H_
