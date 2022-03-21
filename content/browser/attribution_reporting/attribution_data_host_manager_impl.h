// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_

#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace content {

class AttributionManager;

// Manages a receiver set of all ongoing `AttributionDataHost`s and forwards
// events to the AttributionManager which owns `this`. Because attributionsrc
// requests may continue until after we have detached a frame, all browser
// process data needed to validate sources/triggers is frozen and stored
// alongside each receiver.
class CONTENT_EXPORT AttributionDataHostManagerImpl
    : public AttributionDataHostManager,
      public blink::mojom::AttributionDataHost {
 public:
  explicit AttributionDataHostManagerImpl(
      AttributionManager* attribution_manager);
  AttributionDataHostManagerImpl(const AttributionDataHostManager& other) =
      delete;
  AttributionDataHostManagerImpl& operator=(
      const AttributionDataHostManagerImpl& other) = delete;
  AttributionDataHostManagerImpl(AttributionDataHostManagerImpl&& other) =
      delete;
  AttributionDataHostManagerImpl& operator=(
      AttributionDataHostManagerImpl&& other) = delete;
  ~AttributionDataHostManagerImpl() override;

  // AttributionDataHostManager:
  void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      url::Origin context_origin) override;
  void RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override;
  void NotifyNavigationForDataHost(
      const blink::AttributionSrcToken& attribution_src_token,
      const url::Origin& source_origin,
      const url::Origin& destination_origin) override;
  void NotifyNavigationFailure(
      const blink::AttributionSrcToken& attribution_src_token) override;

  // TODO(johnidel): Add support for navigation bound data hosts.

 private:
  // Represents frozen data from the browser process associated with each
  // receiver.
  struct FrozenContext;

  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      blink::mojom::AttributionSourceDataPtr data) override;
  void TriggerDataAvailable(
      blink::mojom::AttributionTriggerDataPtr data) override;

  // Owns `this`.
  raw_ptr<AttributionManager> attribution_manager_;

  mojo::ReceiverSet<blink::mojom::AttributionDataHost, FrozenContext>
      receivers_;

  // Map which stores pending receivers for data hosts which are going to
  // register sources associated with a navigation. These are not added to
  // `receivers_` until the necessary browser process information is available
  // to validate the attribution sources which is after the navigation finishes.
  base::flat_map<blink::AttributionSrcToken,
                 mojo::PendingReceiver<blink::mojom::AttributionDataHost>>
      navigation_data_host_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
