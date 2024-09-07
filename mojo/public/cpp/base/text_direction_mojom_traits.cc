// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/text_direction_mojom_traits.h"

namespace mojo {

// static
mojo_base::mojom::TextDirection
EnumTraits<mojo_base::mojom::TextDirection, base::i18n::TextDirection>::ToMojom(
    base::i18n::TextDirection text_direction) {
  switch (text_direction) {
    case base::i18n::UNKNOWN_DIRECTION:
      return mojo_base::mojom::TextDirection::UNKNOWN_DIRECTION;
    case base::i18n::RIGHT_TO_LEFT:
      return mojo_base::mojom::TextDirection::RIGHT_TO_LEFT;
    case base::i18n::LEFT_TO_RIGHT:
      return mojo_base::mojom::TextDirection::LEFT_TO_RIGHT;
  }
  NOTREACHED();
}

// static
bool EnumTraits<mojo_base::mojom::TextDirection, base::i18n::TextDirection>::
    FromMojom(mojo_base::mojom::TextDirection input,
              base::i18n::TextDirection* out) {
  switch (input) {
    case mojo_base::mojom::TextDirection::UNKNOWN_DIRECTION:
      *out = base::i18n::UNKNOWN_DIRECTION;
      return true;
    case mojo_base::mojom::TextDirection::RIGHT_TO_LEFT:
      *out = base::i18n::RIGHT_TO_LEFT;
      return true;
    case mojo_base::mojom::TextDirection::LEFT_TO_RIGHT:
      *out = base::i18n::LEFT_TO_RIGHT;
      return true;
  }
  return false;
}

}  // namespace mojo
