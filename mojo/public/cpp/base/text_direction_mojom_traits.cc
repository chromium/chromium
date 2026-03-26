// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/base/text_direction_mojom_traits.h"

#include "base/notreached.h"

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
base::i18n::TextDirection
EnumTraits<mojo_base::mojom::TextDirection, base::i18n::TextDirection>::
    FromMojom(mojo_base::mojom::TextDirection input) {
  switch (input) {
    case mojo_base::mojom::TextDirection::UNKNOWN_DIRECTION:
      return base::i18n::UNKNOWN_DIRECTION;
    case mojo_base::mojom::TextDirection::RIGHT_TO_LEFT:
      return base::i18n::RIGHT_TO_LEFT;
    case mojo_base::mojom::TextDirection::LEFT_TO_RIGHT:
      return base::i18n::LEFT_TO_RIGHT;
  }
  NOTREACHED();
}

}  // namespace mojo
