// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/extension_id_mojom_traits.h"

#include "components/crx_file/id_util.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/extension_id.mojom-shared.h"
#include "mojo/public/cpp/bindings/struct_traits.h"

namespace mojo {

bool StructTraits<
    extensions::mojom::ExtensionIdDataView,
    extensions::ExtensionId>::Read(extensions::mojom::ExtensionIdDataView data,
                                   extensions::ExtensionId* out_id) {
  std::string extension_id_from_wire;
  if (!data.ReadId(&extension_id_from_wire)) {
    return false;
  }

  if (!crx_file::id_util::IdIsValid(extension_id_from_wire)) {
    // The extension_id sent over mojom is invalid so fail mojom validation.
    return false;
  }

  *out_id = extension_id_from_wire;
  return true;
}

}  // namespace mojo
