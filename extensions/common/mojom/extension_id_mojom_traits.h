// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_MOJOM_EXTENSION_ID_MOJOM_TRAITS_H_
#define EXTENSIONS_COMMON_MOJOM_EXTENSION_ID_MOJOM_TRAITS_H_

#include <string>

#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/extension_id.mojom-shared.h"
#include "mojo/public/cpp/bindings/string_traits_stl.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

template <>
class StructTraits<extensions::mojom::ExtensionIdDataView,
                   extensions::ExtensionId> {
 public:
  static const std::string& id(const extensions::ExtensionId& input) {
    return input;
  }

  // Reads `data` from the wire and returns `true` if it appears to be a valid
  // extensions::ExtensionId, and false if not. If `false` the mojom message
  // containing the extensions::mojom::ExtensionId will not arrive at the
  // receiver.
  static bool Read(extensions::mojom::ExtensionIdDataView data,
                   extensions::ExtensionId* out_id);
};

}  // namespace mojo

#endif  // EXTENSIONS_COMMON_MOJOM_EXTENSION_ID_MOJOM_TRAITS_H_
