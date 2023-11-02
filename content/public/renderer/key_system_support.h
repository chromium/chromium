// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_KEY_SYSTEM_SUPPORT_H_
#define CONTENT_PUBLIC_RENDERER_KEY_SYSTEM_SUPPORT_H_

#include <string>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "media/mojo/mojom/key_system_support.mojom.h"

namespace content {

using KeySystemCapabilityPtrMap =
    base::flat_map<std::string, media::mojom::KeySystemCapabilityPtr>;
using KeySystemSupportCB =
    base::RepeatingCallback<void(KeySystemCapabilityPtrMap)>;

// Observes key system support updates. The callback `cb` will be called with
// the current key system support, then called every time the key system support
// changes.
CONTENT_EXPORT void ObserveKeySystemSupportUpdate(KeySystemSupportCB cb);

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_KEY_SYSTEM_SUPPORT_H_
