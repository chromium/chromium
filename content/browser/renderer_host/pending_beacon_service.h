// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_
#define CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_

#include "base/memory/singleton.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
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
  class Beacon : public blink::mojom::PendingBeacon {
   public:
    // Browser-side pending beacon constructor. Parameters correspond to the
    // renderer-side PendingBeacon class, except for 'id' which is a
    // browser-side identifier to distinguish which beacons belong to which
    // documents. This will be used to determine if a beacon should be sent when
    // a particular document is discarded or hidden (each document has a
    // `PendingBeaconHost` which keeps a vector of IDs for the beacons it owns).
    // API explainer can be found at:
    // https://github.com/darrenw/docs/blob/main/explainers/beacon_api.md
    explicit Beacon(
        const base::UnguessableToken& id,
        const GURL& url,
        blink::mojom::BeaconMethod method,
        base::TimeDelta timeout,
        mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver);
    ~Beacon() override;
    void Deactivate() override;

   private:
    mojo::Receiver<blink::mojom::PendingBeacon> receiver_;

    const base::UnguessableToken id_;
    const GURL url_;
    [[maybe_unused]] const blink::mojom::BeaconMethod method_;
    [[maybe_unused]] const base::TimeDelta timeout_;
    bool active_ = true;

    friend class PendingBeaconService;
  };

  PendingBeaconService();

  std::vector<std::unique_ptr<Beacon>> beacons_;

  friend struct base::DefaultSingletonTraits<PendingBeaconService>;
};
}  // namespace content

#endif  /// CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_