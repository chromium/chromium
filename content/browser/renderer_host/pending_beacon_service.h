// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_

#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/browser/renderer_host/pending_beacon_host.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace content {

// Singleton class that manages all the pending beacons for a browser
// instance.
class CONTENT_EXPORT PendingBeaconService {
 public:
  static PendingBeaconService* GetInstance();
  PendingBeaconService(const PendingBeaconService&) = delete;
  PendingBeaconService& operator=(const PendingBeaconService&) = delete;

  // Creates a new Beacon, adds it to this service's vector of owned `beacons_`,
  // and connects the new beacon as a receiver for the incoming PendingReceiver.
  void CreateBeacon(mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
                    const base::UnguessableToken& id,
                    const GURL& url,
                    blink::mojom::BeaconMethod method,
                    base::TimeDelta timeout);

  ~PendingBeaconService();

 private:
  PendingBeaconService();

  std::vector<std::unique_ptr<Beacon>> beacons_;

  friend struct base::DefaultSingletonTraits<PendingBeaconService>;
};
}  // namespace content

#endif  /// CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_