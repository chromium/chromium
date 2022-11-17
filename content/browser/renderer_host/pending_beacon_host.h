// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_
#define CONTENT_BROWSER_RENDERER_HOST_PENDING_BEACON_HOST_H_

#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "content/public/browser/document_user_data.h"
#include "content/public/browser/render_process_host_observer.h"
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
// PendingBeaconHost is responsible for preparing beacons and forwards them to
// `PendingBeaconService` for sending. A "beacon-sending" operation can be
// initiated from either the renderer process, or from the browser process:
//
// 1. From renderer. The `SendBeacon()` method handles beacon-sending requests
//    initiated from renderer. It can be called in one of the following
//    scenarios:
//    A. When JavaScript executes `PendingBeacon.sendNow()`, which connects to
//       receiver `Beacon::SendNow()`.
//    B. When the associated document enters `hidden` state, and the renderer's
//       `PendingBeaconDispatcher` schedules and dispatches the request
//       according to individual PendingBeacon's backgroundTimeout property.
//    C. When the individual PendingBeacon's timer of timeout property expires.
//
// 2. From browser. PendingBeaconHost can trigger the sending of beacons by
//    itself:
//    A. When the associated document is discarded or deleted, PendingBeaconHost
//       sends out all queued beacons from its destructor.
//    B. When the associated document's renderer process crashes,
//       RenderFrameHost becomes non-live and its DocumentUserData won't be
//       cleared immediately, i.e. `DeleteForCurrentDocument()` and destructor
//       won't be called, so queued beacons won't be sent there.
//       Instead, PendingBeaconHost sends them out on being notified by
//       `RenderProcessExited()`.
//    C. When the associated document enters `pagehide` state, i.e. the user has
//       navigated away from the document, PendingBeaconHost sends out all
//       queued beacons.
class CONTENT_EXPORT PendingBeaconHost
    : public blink::mojom::PendingBeaconHost,
      public DocumentUserData<PendingBeaconHost>,
      public RenderProcessHostObserver {
 public:
  ~PendingBeaconHost() override;
  PendingBeaconHost(const PendingBeaconHost&) = delete;
  PendingBeaconHost& operator=(const PendingBeaconHost&) = delete;

  // Please keep in sync with "PendingBeaconHostAction" in
  // tools/metrics/histograms/enums.xml. These values should not be renumbered.
  enum class Action {
    kNone = 0,
    // Creates a pending beacon. Initiated by renderer.
    kCreate = 1,
    // About to send a pending beacon. Initiated by renderer.
    kSend = 2,
    // Deletes a pending beacon. Initiated by renderer.
    kDelete = 3,
    // Network service is sending a pending beacon.
    kNetworkSend = 4,
    // Network service receives response for a pending beacon.
    // Note that the number of this action is not guaranteed to match the one of
    // `kNetworkSend`, as RFH may not be alive when a response is sent back.
    kNetworkComplete = 5,

    kMaxValue = kNetworkComplete,
  };
  // Please keep in sync with "PendingBeaconHostBatchAction" in
  // tools/metrics/histograms/enums.xml. These values should not be renumbered.
  enum class BatchAction {
    kNone = 0,
    // The browser process sends all beacons in this host's dtor.
    kSendAllOnHostDestroy = 1,
    // The browser process sends all beacons when a navigation happens.
    kSendAllOnNavigation = 2,
    // The browser process sends all beacons when this renderer process exits.
    kSendAllOnProcessExit = 3,

    kMaxValue = kSendAllOnProcessExit,
  };

  // Creates a new browser-side `Beacon` instance and stores it in this host.
  void CreateBeacon(mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver,
                    const GURL& url,
                    blink::mojom::BeaconMethod method) override;

  // Deletes the `beacon` if exists.
  void DeleteBeacon(Beacon* beacon);
  // Sends out the `beacon` if exists.
  //
  // This method handles beacon-sending requests from the renderer. See class
  // doc for more details.
  void SendBeacon(Beacon* beacon);

  void SetReceiver(
      mojo::PendingReceiver<blink::mojom::PendingBeaconHost> receiver);

  // Forces sending out all `beacons_` on navigating away (pagehide).
  //
  // Whether or not the page is put into BackForwardCache is not relevant.
  //
  // "Unlike `SendBeacon()` which is triggered by the renderer, this method is
  // called only by the browser process itself.
  //
  // https://github.com/WICG/pending-beacon/issues/30
  void SendAllOnNavigation();

  // `RenderProcessHostObserver` implementation.
  // Sends out all queued beacons when renderer process crashes or exits.
  // See `PendingBeaconHost` 2-B for more details.
  void RenderProcessExited(RenderProcessHost*,
                           const ChildProcessTerminationInfo&) override;
  void RenderProcessHostDestroyed(RenderProcessHost*) override;

 private:
  friend class DocumentUserData<PendingBeaconHost>;

  explicit PendingBeaconHost(
      RenderFrameHost* rfh,
      scoped_refptr<network::SharedURLLoaderFactory> shared_url_factory,
      PendingBeaconService* service);

  // Sends out all stored `beacons_` and clear the references.
  void SendAll(const BatchAction& action);
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
  // https://github.com/WICG/pending-beacon/blob/main/README.md
  Beacon(const GURL& url,
         blink::mojom::BeaconMethod method,
         PendingBeaconHost* beacon_host,
         mojo::PendingReceiver<blink::mojom::PendingBeacon> receiver);
  ~Beacon() override;

  // `blink::mojom::PendingBeacon` overrides (used by the renderer):
  // Deletes this beacon from its containing PendingBeaconHost.
  void Deactivate() override;
  // Sets request data for the pending beacon.
  //
  // It is only allowed when this beacon's `BeaconMethod` is kPost.
  // `request_body` must
  //    - Contain only single data element. Complex body is not allowed.
  //    - Contain NO `kChunkedDataPipe` data element.
  //    The above restrictions come from how PendingBeaconService handles
  //    requests.
  void SetRequestData(scoped_refptr<network::ResourceRequestBody> request_body,
                      const std::string& content_type) override;
  // Sets request url for the pending beacon.
  //
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
