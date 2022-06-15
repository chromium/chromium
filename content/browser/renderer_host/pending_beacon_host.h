// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace content {

class Beacon;
class PendingBeaconService;

// Holds a document's pending beacons. This class is responsible for triggering
// the sending of beacons when a document is either discarded or hidden.
// TODO(crbug.com/1293679): Send beacons when document is discarded or hidden.
class CONTENT_EXPORT PendingBeaconHost
    : public blink::mojom::PendingBeaconHost,
      public DocumentUserData<PendingBeaconHost> {
 public:
  ~PendingBeaconHost() override;
  PendingBeaconHost(const PendingBeaconHost&) = delete;
  PendingBeaconHost& operator=(const PendingBeaconHost&) = delete;

  void CreateBeacon(mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
                    const GURL& url,
                    blink::mojom::BeaconMethod method,
                    base::TimeDelta timeout) override;

  void DeleteBeacon(Beacon* beacon);
  void SendBeacon(Beacon* beacon);

  void SetReceiver(
      mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver);

 private:
  friend class DocumentUserData<PendingBeaconHost>;

  explicit PendingBeaconHost(
      RenderFrameHost* rfh,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_factory,
      PendingBeaconService* service);

  std::vector<std::unique_ptr<Beacon>> beacons_;

  mojo::Receiver<blink::mojom::PendingBeaconHost> receiver_;

  // A SharedURLLoaderFactory owned by the owning document's StoragePartition,
  // to be used to send the document's beacons.
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_factory_;

  // Service is a singleton object that has a lifetime longer than any
  // instance of PendingBeaconHost is expected to, so this pointer should be
  // safe.
  raw_ptr<PendingBeaconService> service_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

// Browser-side representation of a pending beacon. These are stored in
// a PendingBeaconHost. Their lifetime is until they are sent - this happens
// either when the PendingBeaconHost is destroyed, or the beacon's `SendNow`
// method is called.
class Beacon : public blink::mojom::PendingBeacon {
 public:
  // Browser-side pending beacon constructor. Parameters correspond to the
  // renderer-side PendingBeacon class.
  // API explainer can be found at:
  // https://github.com/darrenw/docs/blob/main/explainers/beacon_api.md
  Beacon(const GURL& url,
         blink::mojom::BeaconMethod method,
         base::TimeDelta timeout,
         PendingBeaconHost* beacon_host,
         mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver);
  ~Beacon() override;

  // Deletes this beacon from its containing PendingBeaconHost.
  void Deactivate() override;
  void SetData(const std::string& data) override;
  // Sends the beacon immediately, and deletes it from its containing
  // PendingBeaconHost.
  void SendNow() override;

  const GURL& url() const { return url_; }

 private:
  mojo::Receiver<blink::mojom::PendingBeacon> receiver_;

  // The beacon host that owns this beacon. raw_ptr is safe here as the host's
  // lifetime will always be longer than the individual beacon's.
  raw_ptr<PendingBeaconHost> beacon_host_;
  const GURL url_;
  [[maybe_unused]] const blink::mojom::BeaconMethod method_;
  [[maybe_unused]] const base::TimeDelta timeout_;

  // A string containing the bytes for the data of the beacon. This will be
  // either used as the body of the beacon request for POST beacons, or
  // appended to the URL for GET beacons.
  std::string beacon_data_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
