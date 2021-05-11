// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/value_builder.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"

namespace extensions {

// DictionaryBuilder

DictionaryBuilder::DictionaryBuilder() : dict_(new base::DictionaryValue) {}

DictionaryBuilder::DictionaryBuilder(const base::DictionaryValue& init)
    : dict_(init.CreateDeepCopy()) {}

DictionaryBuilder::~DictionaryBuilder() {}

std::string DictionaryBuilder::ToJSON() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      *dict_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

// ListBuilder

ListBuilder::ListBuilder() : list_(new base::ListValue) {}
ListBuilder::~ListBuilder() {}

}  // namespace extensions
