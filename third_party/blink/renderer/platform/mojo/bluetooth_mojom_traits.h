// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_BLUETOOTH_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_BLUETOOTH_MOJOM_TRAITS_H_

#include "device/bluetooth/public/mojom/uuid.mojom-shared.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace mojo {

template <>
struct PLATFORM_EXPORT StructTraits<bluetooth::mojom::UUIDDataView,
                                    blink::String> {
  static const blink::String& uuid(const blink::String& input) { return input; }

  static bool Read(bluetooth::mojom::UUIDDataView, blink::String* output);

  static bool IsNull(const blink::String& input) { return input.IsNull(); }

  static void SetToNull(blink::String* output);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MOJO_BLUETOOTH_MOJOM_TRAITS_H_
