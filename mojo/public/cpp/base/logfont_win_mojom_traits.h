// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BASE_LOGFONT_WIN_MOJOM_TRAITS_H_
#define MOJO_PUBLIC_CPP_BASE_LOGFONT_WIN_MOJOM_TRAITS_H_

#include <windows.h>

#include <cstdint>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/win/windows_types.h"
#include "mojo/public/cpp/bindings/struct_traits.h"
#include "mojo/public/mojom/base/logfont_win.mojom-shared.h"

namespace mojo {

template <>
struct COMPONENT_EXPORT(MOJO_BASE_TRAITS)
    StructTraits<mojo_base::mojom::LOGFONTDataView, ::LOGFONT> {
  static base::span<const uint8_t> bytes(const ::LOGFONT& input);
  static bool Read(mojo_base::mojom::LOGFONTDataView data, ::LOGFONT* out);
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BASE_LOGFONT_WIN_MOJOM_TRAITS_H_
