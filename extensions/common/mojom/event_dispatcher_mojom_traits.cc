// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/mojom/event_dispatcher_mojom_traits.h"

#include "url/mojom/url_gurl_mojom_traits.h"

namespace mojo {

bool StructTraits<extensions::mojom::EventFilteringInfoDataView,
                  extensions::EventFilteringInfo>::
    Read(extensions::mojom::EventFilteringInfoDataView data,
         extensions::EventFilteringInfo* out) {
  *out = extensions::EventFilteringInfo();
  if (!data.ReadUrl(&out->url))
    return false;
  if (!data.ReadServiceType(&out->service_type))
    return false;
  if (data.has_instance_id())
    out->instance_id = data.instance_id();
  if (!data.ReadWindowType(&out->window_type))
    return false;
  if (data.has_window_exposed_by_default())
    out->window_exposed_by_default = data.window_exposed_by_default();
  return true;
}

}  // namespace mojo
