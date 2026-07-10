// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "mojo/public/cpp/base/language_tag_mojom_traits.h"

#include <optional>
#include <string_view>

#include "base/i18n/tag_converters.h"

namespace mojo {

using ::base::i18n::LanguageTag;
using ::base::i18n::LanguageTagConverter;

// static
bool StructTraits<
    mojo_base::mojom::LanguageTagDataView,
    LanguageTag>::Read(mojo_base::mojom::LanguageTagDataView data,
                                   LanguageTag* out) {
  std::string_view tag_string;
  if (!data.ReadTagString(&tag_string)) {
    return false;
  }
  std::optional<LanguageTag> tag =
      LanguageTagConverter::GetInstance().FromString(tag_string);
  if (!tag) {
    return false;
  }
  *out = *tag;
  return true;
}

}  // namespace mojo
