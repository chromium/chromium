// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_TEXT_DIRECTION_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_TEXT_DIRECTION_MOJOM_TRAITS_H_

#include "base/component_export.h"
#include "base/i18n/rtl.h"
#include "mojo/public/mojom/base/text_direction.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    EnumTraits<mojo_base::mojom::TextDirection, base::i18n::TextDirection> {
  static mojo_base::mojom::TextDirection ToMojom(
      base::i18n::TextDirection text_direction);
  static bool FromMojom(mojo_base::mojom::TextDirection input,
                        base::i18n::TextDirection* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_TEXT_DIRECTION_MOJOM_TRAITS_H_
