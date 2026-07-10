// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef MOJO_PUBLIC_CPP_BASE_LANGUAGE_TAG_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_LANGUAGE_TAG_MOJOM_TRAITS_H_

#include <string_view>

#include "base/component_export.h"
#include "base/i18n/language_tag.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/language_tag.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_SHARED_TRAITS)
    StructTraits<mojo_base::mojom::LanguageTagDataView,
                 base::i18n::LanguageTag> {
  static std::string_view tag_string(const base::i18n::LanguageTag& tag) {
    return tag.tag_string();
  }
  static bool Read(mojo_base::mojom::LanguageTagDataView data,
                   base::i18n::LanguageTag* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_LANGUAGE_TAG_MOJOM_TRAITS_H_
