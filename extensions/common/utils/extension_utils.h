// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_UTILS_EXTENSION_UTILS_H_
#define EXTENSIONS_COMMON_UTILS_EXTENSION_UTILS_H_

#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/host_id.mojom.h"

namespace extensions {

class Extension;

// Returns the extension ID or an empty string if null.
const ExtensionId& MaybeGetExtensionId(const Extension* extension);

// Returns a HostID instance based on an |extension_id|.
inline mojom::HostID GenerateHostIdFromExtensionId(
    const ExtensionId& extension_id) {
  // Note: an empty |extension_id| can be used to refer to *all* extensions.
  // See comment in dispatcher.cc.
  return mojom::HostID(mojom::HostID::HostType::kExtensions, extension_id);
}

// Returns a HostID instance based on an |extension|.
inline mojom::HostID GenerateHostIdFromExtension(const Extension* extension) {
  // Note: in some cases, a null Extension can be used. These cases should
  // result in a HostID that's kExtensions with an id that's the empty string.
  // See runtime_hooks_delegate_unittest.cc for an example.
  return GenerateHostIdFromExtensionId(MaybeGetExtensionId(extension));
}

// Returns an |extension_id| from a HostID instance. Will CHECK if
// the HostID type isn't kExtensions.
inline const ExtensionId& GenerateExtensionIdFromHostId(
    const mojom::HostID& host_id) {
  CHECK_EQ(host_id.type, mojom::HostID::HostType::kExtensions);
  return host_id.id;
}

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_UTILS_EXTENSION_UTILS_H_
