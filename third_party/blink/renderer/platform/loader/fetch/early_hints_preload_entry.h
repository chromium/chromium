// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_EARLY_HINTS_PRELOAD_ENTRY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_EARLY_HINTS_PRELOAD_ENTRY_H_

#include "services/network/public/mojom/link_header.mojom-blink-forward.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

// Represents a resource preloaded by an Early Hints response.
struct PLATFORM_EXPORT EarlyHintsPreloadEntry {
  enum class State {
    // The resource is not used by the document yet.
    kUnused,
    // The resource was not used within a few seconds from the window's load
    // event and a warning message was shown.
    kWarnedUnused,
  };

  DISALLOW_NEW();

  EarlyHintsPreloadEntry() = default;
  EarlyHintsPreloadEntry(network::mojom::LinkAsAttribute as,
                         network::mojom::CrossOriginAttribute cross_origin)
      : as(as), cross_origin(cross_origin) {}

  State state = State::kUnused;
  // The "as" attribute value.
  network::mojom::LinkAsAttribute as =
      network::mojom::LinkAsAttribute::kUnspecified;
  // The "crossorigin" attribute value.
  network::mojom::CrossOriginAttribute cross_origin =
      network::mojom::CrossOriginAttribute::kUnspecified;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_EARLY_HINTS_PRELOAD_ENTRY_H_
