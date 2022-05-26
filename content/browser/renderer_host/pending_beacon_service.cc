// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pending_beacon_service.h"

#include "base/time/time.h"
#include "content/browser/renderer_host/pending_beacon_host.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace content {

class Beacon;

PendingBeaconService* PendingBeaconService::GetInstance() {
  return base::Singleton<PendingBeaconService>::get();
}

PendingBeaconService::PendingBeaconService() = default;
PendingBeaconService::~PendingBeaconService() = default;

void PendingBeaconService::sendBeacons(
    const std::vector<std::unique_ptr<Beacon>>& beacons) {
  // TODO(crbug.com/1293679): Implement sending beacons.
  NOTIMPLEMENTED();
}

}  // namespace content
