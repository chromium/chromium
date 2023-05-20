// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

namespace attribution_reporting {
class SuitableOrigin;

struct SourceRegistration;
struct TriggerRegistration;
}  // namespace attribution_reporting

namespace base {
class TimeDelta;
}  // namespace base

namespace content {

class AttributionManager;

struct GlobalRenderFrameHostId;

// Manages a receiver set of all ongoing `AttributionDataHost`s and forwards
// events to the `AttributionManager` that owns `this`. Because attributionsrc
// requests may continue until after we have detached a frame, all browser
// process data needed to validate sources/triggers is stored alongside each
// receiver.
class CONTENT_EXPORT AttributionDataHostManagerImpl
    : public AttributionDataHostManager,
      public blink::mojom::AttributionDataHost {
 public:
  explicit AttributionDataHostManagerImpl(
      AttributionManager* attribution_manager);
  AttributionDataHostManagerImpl(const AttributionDataHostManager&) = delete;
  AttributionDataHostManagerImpl& operator=(
      const AttributionDataHostManagerImpl&) = delete;
  AttributionDataHostManagerImpl(AttributionDataHostManagerImpl&&) = delete;
  AttributionDataHostManagerImpl& operator=(AttributionDataHostManagerImpl&&) =
      delete;
  ~AttributionDataHostManagerImpl() override;

  // AttributionDataHostManager:
  void RegisterDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      attribution_reporting::SuitableOrigin context_origin,
      bool is_within_fenced_frame,
      attribution_reporting::mojom::RegistrationType,
      GlobalRenderFrameHostId render_frame_id,
      int64_t last_navigation_id) override;
  bool RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token,
      AttributionInputEvent input_event) override;

  void NotifyNavigationRegistrationStarted(
      const blink::AttributionSrcToken& attribution_src_token,
      const attribution_reporting::SuitableOrigin& source_origin,
      blink::mojom::AttributionNavigationType nav_type,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id,
      int64_t navigation_id) override;
  void NotifyNavigationRegistrationData(
      const blink::AttributionSrcToken& attribution_src_token,
      const net::HttpResponseHeaders* headers,
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::SuitableOrigin& source_origin,
      AttributionInputEvent input_event,
      blink::mojom::AttributionNavigationType nav_type,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id,
      int64_t navigation_id,
      network::AttributionReportingRuntimeFeatures,
      bool is_final_response) override;

  void NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      absl::optional<int64_t> navigation_id,
      attribution_reporting::SuitableOrigin source_origin,
      bool is_within_fenced_frame,
      AttributionInputEvent input_event,
      GlobalRenderFrameHostId render_frame_id) override;
  void NotifyFencedFrameReportingBeaconData(
      BeaconId beacon_id,
      network::AttributionReportingRuntimeFeatures,
      url::Origin reporting_origin,
      const net::HttpResponseHeaders* headers,
      bool is_final_response) override;

 private:
  class ReceiverContext;

  struct DeferredReceiverTimeout;
  struct DeferredReceiver;
  struct NavigationDataHost;

  // Represents a set of attribution sources which registered in a top-level
  // navigation redirect or a beacon chain, and associated info to process them.
  class SourceRegistrations;

  using SourceRegistrationsId =
      absl::variant<blink::AttributionSrcToken, BeaconId>;

  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration) override;
  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration,
      absl::optional<network::TriggerVerification> verification) override;
  void OsSourceDataAvailable(std::vector<GURL> registration_urls) override;
  void OsTriggerDataAvailable(std::vector<GURL> registration_urls) override;

  const ReceiverContext* GetReceiverContextForSource();
  const ReceiverContext* GetReceiverContextForTrigger();

  void OnReceiverDisconnected();

  struct RegistrarAndHeader;

  void ParseSource(base::flat_set<SourceRegistrations>::iterator,
                   attribution_reporting::SuitableOrigin reporting_origin,
                   RegistrarAndHeader);
  void HandleNextWebDecode(const SourceRegistrations&);
  void OnWebSourceParsed(SourceRegistrationsId,
                         data_decoder::DataDecoder::ValueOrError result);

  void HandleNextOsDecode(const SourceRegistrations&);

  using OsParseResult =
      base::expected<net::structured_headers::List, std::string>;
  void OnOsSourceParsed(SourceRegistrationsId, OsParseResult);

  void MaybeOnRegistrationsFinished(
      base::flat_set<SourceRegistrations>::const_iterator);

  void MaybeSetupDeferredReceivers(int64_t navigation_id);
  void StartDeferredReceiversTimeoutTimer(base::TimeDelta);
  void ProcessDeferredReceiversTimeout();
  void MaybeBindDeferredReceivers(int64_t navigation_id, bool due_to_timeout);

  // Owns `this`.
  raw_ptr<AttributionManager> attribution_manager_;

  mojo::ReceiverSet<blink::mojom::AttributionDataHost, ReceiverContext>
      receivers_;

  // Map which stores pending receivers for data hosts which are going to
  // register sources associated with a navigation. These are not added to
  // `receivers_` until the necessary browser process information is available
  // to validate the attribution sources which is after the navigation starts.
  base::flat_map<blink::AttributionSrcToken, NavigationDataHost>
      navigation_data_host_map_;

  // If eligible, sources can be registered during a navigation. These
  // registrations can complete after the navigation ends. On the landing page,
  // we defer the registration of triggers until all the source registrations
  // initiated during the navigation complete.
  //
  // Navigation-linked source registrations can happen via 3 channels:
  //
  // 1. Foreground: in the main navigation request, upon receiving a redirection
  //    or the final response via `NotifyNavigationRegistrationData`, if it
  //    contains attribution headers, the source is parsed asynchronously by the
  //    DataDecoder.
  // 2. Background: an attribution-specific request can be sent, when the
  //    navigation starts. It can resolve before or after the navigation ends.
  //    `RegisterNavigationDataHost` is used to open a pipe which stays
  //    connected for the duration of the request, including redirections which
  //    can also register sources.
  // 3. Fenced Frame: Via `NotifyFencedFrameReportingBeaconStarted` &
  //    `NotifyFencedFrameReportingBeaconData`. There can be multiple beacons
  //    for a single navigation.
  //
  // Given a navigation, registrations can happen on all channels
  // simultaneously.

  // Stores deferred receivers. When all ongoing source registrations linked to
  // a navigation complete, the receivers get bound and removed from the list.
  base::flat_map<int64_t, std::vector<DeferredReceiver>> deferred_receivers_;

  // Keeps track of ongoing background source registrations.
  base::flat_set<int64_t> ongoing_background_registrations_;
  // Stores registrations received on foreground redirects or via a Fenced
  // Frame Beacon.
  base::flat_set<SourceRegistrations> registrations_;

  // Guardrail to ensure a receiver in `deferred_receivers_` always eventually
  // gets bound. We use a single timer. When we `MaybeSetupDeferredReceivers`
  // for a navigation, we add a timeout in the queue. If it isn't already
  // running, we start the timer. When the timer expires, we pop a timeout from
  // the queue and bind its deferred receivers, if they aren't already. If the
  // queue is not empty, we re-start the timer for the timeout at the front of
  // the queue.
  base::circular_deque<DeferredReceiverTimeout> deferred_receivers_timeouts_;
  base::OneShotTimer deferred_receivers_timeouts_timer_;

  data_decoder::DataDecoder data_decoder_;

  base::WeakPtrFactory<AttributionDataHostManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
