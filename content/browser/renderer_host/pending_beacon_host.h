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
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace content {

class PendingBeaconService;

// Holds a set of IDs (i.e. UnguessableTokens) for a document's
// pending beacons. This class is responsible for triggering the sending
// of beacons when a document is either discarded or hidden.
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

  void SetReceiver(
      mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver);

 private:
  friend class DocumentUserData<PendingBeaconHost>;

  explicit PendingBeaconHost(RenderFrameHost* rfh,
                             PendingBeaconService* service);

  // The IDs for the beacons that the document owns.
  std::vector<base::UnguessableToken> beacon_ids_;
  mojo::Receiver<blink::mojom::PendingBeaconHost> receiver_;

  // Service is a singleton object that has a lifetime longer than any
  // instance of PendingBeaconHost is expected to, so this pointer should be
  // safe.
  raw_ptr<PendingBeaconService> service_;

  DOCUMENT_USER_DATA_KEY_DECL();
};

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
  explicit Beacon(const base::UnguessableToken& id,
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
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_