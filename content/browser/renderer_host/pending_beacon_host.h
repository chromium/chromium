// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace network {
class DataElement;
}  // namespace network

namespace content {

class Beacon;
class PendingBeaconService;

// `PendingBeaconHost` holds and manages a single blink::Document's pending
// beacons in the browser process.
//
// PendingBeaconHost is created once per document and bound to a RenderFrameHost
// by `PendingBeaconHost::CreateForCurrentDocument()` called from
// `RenderFrameHostImpl::GetPendingBeaconHost()`.
//
// PendingBeaconHost's lifetime is roughly the same as a single document (except
// when crashing). See `DocumentUserData` for more details about lifetime.
//
// PendingBeaconHost creates a new Beacon when `CreateBeacon()` is called
// remotely from a document in renderer.
//
// PendingBeaconHost receives `SendBeacon()` requests initiated from renderer
// and forwards it to PendingBeaconService. The requests can be initiated in one
// of the following scenarios:
// -  When JavaScript executes `PendingBeacon.sendNow()`, which connects to
//    receiver `Beacon::SendNow()`.
// -  When the associated document enters `hidden` state, and the renderer's
//    `PendingBeaconDispatcher` schedules and dispatches the request according
//    to individual PendingBeacon's backgroundTimeout property.
// -  When the individual PendingBeacon's timer of timeout property expires.
//
// PendingBeaconHost is also responsible for triggering the sending of beacons:
// -  When the associated document is discarded or deleted, PendingBeaconHost
//    sends out all queued beacons from its destructor.
// -  TODO(crbug.com/1293679): When the associated document's renderer process
//    crashes, PendingBeaconHost sends out all queued beacon after being
//    notified by RenderProcessHostDestroyed.
class CONTENT_EXPORT PendingBeaconHost
    : public blink::mojom::PendingBeaconHost,
      public DocumentUserData<PendingBeaconHost> {
 public:
  ~PendingBeaconHost() override;
  PendingBeaconHost(const PendingBeaconHost&) = delete;
  PendingBeaconHost& operator=(const PendingBeaconHost&) = delete;

  // Creates a new browser-side `Beacon` instance and stores it in this host.
  void CreateBeacon(mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
                    const GURL& url,
                    blink::mojom::BeaconMethod method) override;

  // Deletes the `beacon` if exists.
  void DeleteBeacon(Beacon* beacon);
  // Sends out the `beacon` if exists.
  void SendBeacon(Beacon* beacon);

  void SetReceiver(
      mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver);

 private:
  friend class DocumentUserData<PendingBeaconHost>;

  explicit PendingBeaconHost(
      RenderFrameHost* rfh,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_factory,
      PendingBeaconService* service);

  // Encapsulates how `beacons` are sent.
  void Send(const std::vector<std::unique_ptr<Beacon>>& beacons);

  // Stores all the browser-side instances of `Beacon`.
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

// `Beacon` is the browser-side representation of a PendingBeacon.
// It is created and stored in a `PendingBeaconHost`. Hence, their lifetime is
// until they are sent, which happens in one of the following scenarios:
//   - When the PendingBeaconHost is destroyed.
//   - When the beacon's `SendNow()` method is called.
class Beacon : public blink::mojom::PendingBeacon {
 public:
  // Browser-side pending beacon constructor. Parameters correspond to the
  // renderer-side PendingBeacon class.
  // API explainer can be found at:
  // https://github.com/WICG/unload-beacon/blob/main/README.md
  Beacon(const GURL& url,
         blink::mojom::BeaconMethod method,
         PendingBeaconHost* beacon_host,
         mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver);
  ~Beacon() override;

  // Deletes this beacon from its containing PendingBeaconHost.
  void Deactivate() override;

  // Sets request data for the pending beacon.
  // It is only allowed when this beacon's `BeaconMethod` is kPost.
  // `request_body` must
  //    - Contain only single data element. Complex body is not allowed.
  //    - Contain NO `kChunkedDataPipe` data element.
  //    The above restrictions come from how PendingBeaconService handles
  //    requests.
  void SetRequestData(scoped_refptr<network::ResourceRequestBody> request_body,
                      const std::string& content_type) override;

  // Sets request url for the pending beacon.
  // The spec only allows GET beacons to update its own URL. So `BeaconMethod`
  // must be kGet when calling this.
  void SetRequestURL(const GURL& url) override;

  // Sends the beacon immediately, and deletes it from its containing
  // PendingBeaconHost.
  void SendNow() override;

  // Creates a request based on the beacon's url and data.
  // * If `method_` is GET, the request url is constructed from `url_`.
  // * If `method_` is POST, the request url is from `url_`, and the request
  //   content is from `request_body_`, `content_type_`, and `request_element_`.
  const std::unique_ptr<network::ResourceRequest> GenerateResourceRequest()
      const;

  const std::string& content_type() const {
    DCHECK(method_ != blink::mojom::BeaconMethod::kGet ||
           content_type_.empty());
    return content_type_;
  }

 private:
  mojo::Receiver<blink::mojom::PendingBeacon> receiver_;

  // Points to the PendingBeaconHost that owns the instance of this beacon.
  // raw_ptr is safe here as the `beacon_host_`'s lifetime will always be longer
  // than the individual beacons it owns.
  raw_ptr<PendingBeaconHost> beacon_host_;
  // The request URL this beacon will be sent to.
  GURL url_;
  // The request method that will be used to send this beacon.
  const blink::mojom::BeaconMethod method_;

  // The request content type for POST beacon. If `method_` is GET, this field
  // should not be used.
  std::string content_type_;
  // The beacon data represented as a data element. If `method_` is GET, this
  // field should not be used.
  absl::optional<network::DataElement> request_element_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
