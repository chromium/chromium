// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "mojo/public/cpp/base/logfont_win_mojom_traits.h"

#include <tchar.h>

#include "base/logging.h"
#include "base/numerics/safe_conversions.h"

namespace mojo {

// static
base::span<const uint8_t>
StructTraits<mojo_base::mojom::LOGFONTDataView, ::LOGFONT>::bytes(
    const ::LOGFONT& input) {
  return base::make_span(reinterpret_cast<const uint8_t*>(&input),
                         sizeof(::LOGFONT));
}

// static
bool StructTraits<mojo_base::mojom::LOGFONTDataView, ::LOGFONT>::Read(
    mojo_base::mojom::LOGFONTDataView data,
    ::LOGFONT* out) {
  ArrayDataView<uint8_t> bytes_view;
  data.GetBytesDataView(&bytes_view);
  if (bytes_view.size() != sizeof(::LOGFONT))
    return false;

  const ::LOGFONT* font = reinterpret_cast<const ::LOGFONT*>(bytes_view.data());
  if (_tcsnlen(font->lfFaceName, LF_FACESIZE) >= LF_FACESIZE)
    return false;

  memcpy(out, font, sizeof(::LOGFONT));
  return true;
}

}  // namespace mojo
