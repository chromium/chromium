// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace blink::mojom {
class AttributionDataHost;
}  // namespace blink::mojom

namespace url {
class Origin;
}  // namespace url

namespace content {

// Interface responsible for coordinating `AttributionDataHost`s received from
// the renderer.
class AttributionDataHostManager {
 public:
  virtual ~AttributionDataHostManager() = default;

  // Registers a new data host with the browser process for the given context
  // origin. This is only called for events which are not associated with a
  // navigation.
  virtual void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      url::Origin context_origin) = 0;

  // TODO(johnidel): Add support for navigation bound data hosts.
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
