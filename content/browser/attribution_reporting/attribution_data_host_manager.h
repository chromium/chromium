// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_

#include <string>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/common/tokens/tokens.h"

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

  // Registers a new data host which is associated with a navigation. The
  // context origin will be provided at a later time in
  // `NotifyNavigationForDataHost()` called with the same
  // `attribution_src_token`. Returns `false` if `attribution_src_token` was
  // already registered.
  virtual bool RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) = 0;

  // Notifies the manager that an attribution enabled navigation has registered
  // a source header. May be called multiple times for the same navigation.
  // Important: `header_value` is untrusted.
  virtual void NotifyNavigationRedirectRegistation(
      const blink::AttributionSrcToken& attribution_src_token,
      const std::string& header_value,
      url::Origin reporting_origin,
      const url::Origin& source_origin) = 0;

  // Notifies the manager that we have received a navigation for a given data
  // host. This may arrive before or after the attribution configuration is
  // available for a given data host.
  virtual void NotifyNavigationForDataHost(
      const blink::AttributionSrcToken& attribution_src_token,
      const url::Origin& source_origin,
      const url::Origin& destination_origin) = 0;

  // Notifies the manager that a navigation associated with a data host failed
  // and should no longer be tracked.
  virtual void NotifyNavigationFailure(
      const blink::AttributionSrcToken& attribution_src_token) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_H_
