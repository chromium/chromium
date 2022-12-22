// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/value_builder.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/strings/string_piece.h"

namespace extensions {

// DictionaryBuilder

DictionaryBuilder::DictionaryBuilder() = default;

DictionaryBuilder::DictionaryBuilder(const base::Value::Dict& init)
    : dict_(init.Clone()) {}

DictionaryBuilder::~DictionaryBuilder() = default;

std::string DictionaryBuilder::ToJSON() const {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      dict_, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

// ListBuilder

ListBuilder::ListBuilder() = default;
ListBuilder::~ListBuilder() = default;

}  // namespace extensions
