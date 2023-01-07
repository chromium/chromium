// Copyright 2022 The Chromium Authors
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

namespace network {
class SharedURLLoaderFactory;
}

namespace content {

class Beacon;

// Singleton class that manages sending pending beacons as well as persisting
// and restoring pending beacons to disk for crash recovery.
// TODO(crbug.com/1293679): Implement beacon persistence.
class CONTENT_EXPORT PendingBeaconService {
 public:
  static PendingBeaconService* GetInstance();
  PendingBeaconService(const PendingBeaconService&) = delete;
  PendingBeaconService& operator=(const PendingBeaconService&) = delete;

  ~PendingBeaconService();

  void SendBeacons(const std::vector<std::unique_ptr<Beacon>>& beacons,
                   network::SharedURLLoaderFactory* shared_url_loader_factory);

 private:
  PendingBeaconService();
  friend struct base::DefaultSingletonTraits<PendingBeaconService>;
};
}  // namespace content

#endif  /// CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_SERVICE_H_
