// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file provides a builders for base::Value::Dict and base::Value::List.
// These aren't specific to extensions and could move up to base/ if there's
// interest from other sub-projects.
//
// The pattern is to write:
//
//  base::Value::[Dict|List] result([Dictionary|List]Builder()
//      .Set(args)
//      .Set(args)
//      .Build());
//
// The Build() method invalidates its builder, and returns ownership of the
// built value.
//
// These objects are intended to be used as temporaries rather than stored
// anywhere, so the use of non-const reference parameters is likely to cause
// less confusion than usual.

#ifndef EXTENSIONS_COMMON_VALUE_BUILDER_H_
#define EXTENSIONS_COMMON_VALUE_BUILDER_H_

#include <memory>
#include <string>
#include <utility>

#include "base/strings/string_piece_forward.h"
#include "base/values.h"

namespace extensions {

class DictionaryBuilder {
 public:
  DictionaryBuilder();
  explicit DictionaryBuilder(const base::Value::Dict& init);

  DictionaryBuilder(const DictionaryBuilder&) = delete;
  DictionaryBuilder& operator=(const DictionaryBuilder&) = delete;

  ~DictionaryBuilder();

  base::Value::Dict Build() {
    base::Value::Dict result = std::move(dict_);
    dict_ = base::Value::Dict();
    return result;
  }

  // Immediately serializes the current state to JSON. Can be called as many
  // times as you like.
  std::string ToJSON() const;

  template <typename T>
  DictionaryBuilder& Set(base::StringPiece key, T in_value) {
    dict_.Set(key, std::move(in_value));
    return *this;
  }

 private:
  base::Value::Dict dict_;
};

class ListBuilder {
 public:
  ListBuilder();

  ListBuilder(const ListBuilder&) = delete;
  ListBuilder& operator=(const ListBuilder&) = delete;

  ~ListBuilder();

  base::Value::List Build() {
    base::Value::List result = std::move(list_);
    list_ = base::Value::List();
    return result;
  }

  template <typename T>
  ListBuilder& Append(T in_value) {
    list_.Append(std::move(in_value));
    return *this;
  }

  // Utility for appending a collection. Is this templating simplistic? Yes.
  // But if it's good enough for the STL, it's good enough for this class.
  template <typename InputIt>
  ListBuilder& Append(InputIt first, InputIt last) {
    for (; first != last; ++first)
      list_.Append(*first);
    return *this;
  }

 private:
  base::Value::List list_;
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_VALUE_BUILDER_H_
