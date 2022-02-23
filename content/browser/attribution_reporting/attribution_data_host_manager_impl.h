// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"
#include "url/origin.h"

namespace content {

class AttributionManager;
class BrowserContext;

// Manages a receiver set of all ongoing `AttributionDataHost`s and forwards
// events to the AttributionManager which owns `this`. Because attributionsrc
// requests may continue until after we have detached a frame, all browser
// process data needed to validate sources/triggers is frozen and stored
// alongside each receiver.
class CONTENT_EXPORT AttributionDataHostManagerImpl
    : public AttributionDataHostManager,
      public blink::mojom::AttributionDataHost {
 public:
  AttributionDataHostManagerImpl(BrowserContext* storage_partition,
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

  // TODO(johnidel): Add support for navigation bound data hosts.

 private:
  // Represents frozen data from the browser process associated with each
  // receiver.
  struct FrozenContext {
    // Top-level origin the data host was created in.
    url::Origin context_origin;

    // Source type of this context. Note that data hosts which result in
    // triggers still have a source type of` kEvent` as they share the same web
    // API surface.
    CommonSourceInfo::SourceType source_type;
  };

  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      blink::mojom::AttributionSourceDataPtr data) override;

  // Safe because the owning `AttributionManager` is guaranteed to outlive the
  // browser context.
  raw_ptr<BrowserContext> browser_context_;

  // Owns `this`.
  raw_ptr<AttributionManager> attribution_manager_;

  mojo::ReceiverSet<blink::mojom::AttributionDataHost, FrozenContext>
      receivers_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
