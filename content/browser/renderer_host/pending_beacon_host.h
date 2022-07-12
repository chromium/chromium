// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "third_party/blink/public/mojom/frame/pending_beacon.mojom.h"

namespace network {
class DataElement;
}  // namespace network

namespace content {

class Beacon;
class PendingBeaconService;

// PendingBeaconHost holds and manages a single blink::Document's pending
// beacons in the browser process.
//
// PendingBeaconHost is created once per document and bound to a RenderFrameHost
// by calling `PendingBeaconHost::CreateForCurrentDocument`. See also
// `DocumentUserData` for the lifetime of this class.
//
// PendingBeaconHost creates a new Beacon when `CreateBeacon` is called remotely
// by a document.
//
// PendingBeaconHost is also responsible for triggering the sending of beacons:
// -  When `SendBeacon` is called, the corresponding queued beacon is sent out
//    and removed from the host.
// -  When the associated document is discarded or deleted, the host sends out
//    all queued beacons in its destructor.
// -  TODO(crbug.com/1293679): When the associated document is hidden, the host
//    should send out beacons according to their timeout field.
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
                    blink::mojom::BeaconMethod method,
                    base::TimeDelta timeout) override;

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

// Browser-side representation of a pending beacon. These are stored in
// a PendingBeaconHost. Their lifetime is until they are sent - this happens
// either when the PendingBeaconHost is destroyed, or the beacon's `SendNow`
// method is called.
class Beacon : public blink::mojom::PendingBeacon {
 public:
  // Browser-side pending beacon constructor. Parameters correspond to the
  // renderer-side PendingBeacon class.
  // API explainer can be found at:
  // https://github.com/WICG/unload-beacon/blob/main/README.md
  Beacon(const GURL& url,
         blink::mojom::BeaconMethod method,
         base::TimeDelta timeout,
         PendingBeaconHost* beacon_host,
         mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver);
  ~Beacon() override;

  // Deletes this beacon from its containing PendingBeaconHost.
  void Deactivate() override;

  // Sets request data for the pending beacon.
  void SetRequestData(scoped_refptr<network::ResourceRequestBody> request_body,
                      const std::string& content_type) override;

  // Sends the beacon immediately, and deletes it from its containing
  // PendingBeaconHost.
  void SendNow() override;

  // Creates a request based on the beacon's url and data.
  // * If `method_` is GET, the request url is constructed from `url_`.
  // * If `method_` is POST, the request url is from `url_`, and the request
  //   content is from `request_body_` and `content_type_`.
  const std::unique_ptr<network::ResourceRequest> GenerateResourceRequest()
      const;

  const std::string& content_type() const {
    DCHECK(method_ != blink::mojom::BeaconMethod::kGet ||
           content_type_.empty());
    return content_type_;
  }
  const std::vector<network::DataElement>& request_elements() const {
    DCHECK(method_ != blink::mojom::BeaconMethod::kGet ||
           request_elements_.empty());
    return request_elements_;
  }

 private:
  mojo::Receiver<blink::mojom::PendingBeacon> receiver_;

  // The beacon host that owns this beacon. raw_ptr is safe here as the host's
  // lifetime will always be longer than the individual beacon's.
  raw_ptr<PendingBeaconHost> beacon_host_;
  const GURL url_;
  [[maybe_unused]] const blink::mojom::BeaconMethod method_;
  [[maybe_unused]] const base::TimeDelta timeout_;

  // The request content type for POST beacon. If `method_` is GET, this field
  // should not be used.
  std::string content_type_;
  // The beacon data represented as data elements. If `method_` is GET, this
  // field should not be used.
  std::vector<network::DataElement> request_elements_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
