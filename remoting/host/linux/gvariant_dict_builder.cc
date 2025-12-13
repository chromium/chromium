// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/gvariant_dict_builder.h"

#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

GVariantDictBuilder::GVariantDictBuilder() = default;
GVariantDictBuilder::~GVariantDictBuilder() = default;

GVariantDictBuilder& GVariantDictBuilder::AddVariant(
    std::string_view key,
    gvariant::GVariantRef<"v"> variant) {
  entries_.emplace_back(key, variant);
  return *this;
}

gvariant::GVariantRef<"a{sv}"> GVariantDictBuilder::Build() {
  return gvariant::GVariantFrom(entries_);
}

}  // namespace remoting
