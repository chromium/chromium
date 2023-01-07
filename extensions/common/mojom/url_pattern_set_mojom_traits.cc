// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/url_pattern_set_mojom_traits.h"

namespace mojo {

bool StructTraits<extensions::mojom::URLPatternDataView, URLPattern>::Read(
    extensions::mojom::URLPatternDataView data,
    URLPattern* out) {
  std::string pattern;

  if (!data.ReadPattern(&pattern))
    return false;

  // TODO(jstritar): We don't want the URLPattern to fail parsing when the
  // scheme is invalid. Instead, the pattern should parse but it should not
  // match the invalid patterns. We get around this by setting the valid
  // schemes after parsing the pattern. Update these method calls once we can
  // ignore scheme validation with URLPattern parse options. crbug.com/90544
  out->SetValidSchemes(URLPattern::SCHEME_ALL);
  URLPattern::ParseResult result = out->Parse(pattern);
  out->SetValidSchemes(data.valid_schemes());

  return URLPattern::ParseResult::kSuccess == result;
}

bool StructTraits<extensions::mojom::URLPatternSetDataView,
                  extensions::URLPatternSet>::
    Read(extensions::mojom::URLPatternSetDataView data,
         extensions::URLPatternSet* out) {
  std::vector<URLPattern> mojo_patterns;
  if (!data.ReadPatterns(&mojo_patterns))
    return false;
  for (const auto& pattern : mojo_patterns)
    out->AddPattern(pattern);

  if (mojo_patterns.size() != out->patterns().size())
    return false;

  return true;
}

}  // namespace mojo
