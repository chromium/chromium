// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_LINUX_GVARIANT_DICT_BUILDER_H_
#define REMOTING_HOST_LINUX_GVARIANT_DICT_BUILDER_H_

#include <string_view>
#include <utility>
#include <vector>

#include "remoting/host/linux/gvariant_ref.h"

namespace remoting {

// Helper class for constructing a GLib VariantDict of type `a{sv}`, i.e. string
// keys and variant values.
class GVariantDictBuilder {
 public:
  GVariantDictBuilder();
  ~GVariantDictBuilder();

  // Important: For booleans, use C++'s `true` and `false` instead of GLib's
  // `TRUE` and `FALSE` macros. Those macros are expanded into `1` and `0`, so
  // the variants would be of type `i`.
  template <typename T>
  GVariantDictBuilder& Add(std::string_view key, const T& value) {
    return AddVariant(key, gvariant::GVariantFrom(gvariant::BoxedRef(value)));
  }

  GVariantDictBuilder& AddVariant(std::string_view key,
                                  gvariant::GVariantRef<"v"> variant);

  gvariant::GVariantRef<"a{sv}"> Build();

 private:
  std::vector<std::pair<std::string, gvariant::GVariantRef<"v">>> entries_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_LINUX_GVARIANT_DICT_BUILDER_H_
