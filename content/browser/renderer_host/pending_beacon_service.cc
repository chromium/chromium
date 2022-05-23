// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_service.h"

#include "base/time/time.h"
#include "content/browser/renderer_host/pending_beacon_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace content {

PendingBeaconService* PendingBeaconService::GetInstance() {
  return base::Singleton<PendingBeaconService>::get();
}

PendingBeaconService::PendingBeaconService() = default;
PendingBeaconService::~PendingBeaconService() = default;

void PendingBeaconService::CreateBeacon(
    mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
    const base::UnguessableToken& id,
    const GURL& url,
    blink::mojom::BeaconMethod method,
    base::TimeDelta timeout) {
  auto beacon =
      std::make_unique<Beacon>(id, url, method, timeout, std::move(receiver));
  beacons_.emplace_back(std::move(beacon));
}

}  // namespace content
