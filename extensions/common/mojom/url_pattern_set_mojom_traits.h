// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_URL_PATTERN_SET_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_URL_PATTERN_SET_MOJOM_TRAITS_H_

#include "extensions/common/mojom/url_pattern_set.mojom-shared.h"
#include "extensions/common/url_pattern.h"
#include "extensions/common/url_pattern_set.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
struct StructTraits<extensions::mojom::URLPatternDataView, URLPattern> {
  static int32_t valid_schemes(const URLPattern& pattern) {
    return pattern.valid_schemes();
  }

  static const std::string& pattern(const URLPattern& pattern) {
    return pattern.GetAsString();
  }

  static bool Read(extensions::mojom::URLPatternDataView data, URLPattern* out);
};

template <>
struct StructTraits<extensions::mojom::URLPatternSetDataView,
                    extensions::URLPatternSet> {
  static const std::set<URLPattern>& patterns(
      const extensions::URLPatternSet& set) {
    return set.patterns();
  }

  static bool Read(extensions::mojom::URLPatternSetDataView data,
                   extensions::URLPatternSet* out);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_URL_PATTERN_SET_MOJOM_TRAITS_H_
