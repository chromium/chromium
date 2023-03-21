// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_

#include <stddef.h>

#include <string>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/conversions/attribution_data_host.mojom.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/types/expected.h"
#include "net/http/structured_headers.h"
#endif

namespace attribution_reporting {
class SuitableOrigin;

struct SourceRegistration;
struct TriggerRegistration;
}  // namespace attribution_reporting

namespace base {
class TimeDelta;
class TimeTicks;
}  // namespace base

namespace content {

class AttributionManager;
class AttributionTrigger;

struct GlobalRenderFrameHostId;

#if BUILDFLAG(IS_ANDROID)
struct OsRegistration;
#endif

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
      GlobalRenderFrameHostId render_frame_id) override;
  bool RegisterNavigationDataHost(
      mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token,
      AttributionInputEvent input_event) override;
  void NotifyNavigationRedirectRegistration(
      const blink::AttributionSrcToken& attribution_src_token,
      const net::HttpResponseHeaders* headers,
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::SuitableOrigin& source_origin,
      AttributionInputEvent input_event,
      blink::mojom::AttributionNavigationType nav_type,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id) override;
  void NotifyNavigationForDataHost(
      const blink::AttributionSrcToken& attribution_src_token,
      const attribution_reporting::SuitableOrigin& source_origin,
      blink::mojom::AttributionNavigationType nav_type,
      bool is_within_fenced_frame,
      GlobalRenderFrameHostId render_frame_id) override;
  void NotifyNavigationFailure(
      const blink::AttributionSrcToken& attribution_src_token) override;
  void NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      attribution_reporting::SuitableOrigin source_origin,
      bool is_within_fenced_frame,
      AttributionInputEvent input_event,
      GlobalRenderFrameHostId render_frame_id) override;
  void NotifyFencedFrameReportingBeaconSent(BeaconId beacon_id) override;
  void NotifyFencedFrameReportingBeaconData(
      BeaconId beacon_id,
      url::Origin reporting_origin,
      const net::HttpResponseHeaders* headers,
      bool is_final_response) override;

 private:
  class ReceiverContext;

  struct DelayedTrigger;
  struct NavigationDataHost;

  // Represents a set of attribution sources which registered in a top-level
  // navigation redirect or a beacon chain, and associated info to process them.
  class SourceRegistrations;

  using SourceRegistrationsId =
      absl::variant<blink::AttributionSrcToken, BeaconId>;

#if BUILDFLAG(IS_ANDROID)
  using TriggerPayload = absl::variant<AttributionTrigger, OsRegistration>;
#else
  using TriggerPayload = AttributionTrigger;
#endif

  // blink::mojom::AttributionDataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration) override;
  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration,
      absl::optional<network::TriggerAttestation> attestation) override;
#if BUILDFLAG(IS_ANDROID)
  void OsSourceDataAvailable(const GURL& registration_url) override;
  void OsTriggerDataAvailable(const GURL& registration_url) override;
#endif

  const ReceiverContext* GetReceiverContextForSource();
  void OnReceiverDisconnected();
  void OnSourceEligibleDataHostFinished(base::TimeTicks register_time);

  struct RegistrarAndHeader;

  void ParseSource(base::flat_set<SourceRegistrations>::iterator,
                   attribution_reporting::SuitableOrigin reporting_origin,
                   const RegistrarAndHeader&);
  void OnSourceParsed(
      SourceRegistrationsId,
      base::FunctionRef<void(const SourceRegistrations&)> handle_result);
  void OnWebSourceParsed(
      SourceRegistrationsId,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      const std::string& header_value,
      data_decoder::DataDecoder::ValueOrError result);

#if BUILDFLAG(IS_ANDROID)
  using OsParseResult =
      base::expected<net::structured_headers::ParameterizedItem, std::string>;
  void OnOsSourceParsed(SourceRegistrationsId, OsParseResult);
#endif

  void MaybeOnRegistrationsFinished(
      base::flat_set<SourceRegistrations>::const_iterator);

  void HandleTrigger(TriggerPayload, GlobalRenderFrameHostId);
  void MaybeBufferTrigger(
      base::FunctionRef<TriggerPayload(const ReceiverContext&)>);
  void SetTriggerTimer(base::TimeDelta delay);
  void ProcessDelayedTrigger();

  // Owns `this`.
  raw_ptr<AttributionManager> attribution_manager_;

  mojo::ReceiverSet<blink::mojom::AttributionDataHost, ReceiverContext>
      receivers_;

  // Map which stores pending receivers for data hosts which are going to
  // register sources associated with a navigation. These are not added to
  // `receivers_` until the necessary browser process information is available
  // to validate the attribution sources which is after the navigation finishes.
  base::flat_map<blink::AttributionSrcToken, NavigationDataHost>
      navigation_data_host_map_;

  // Stores registrations received for redirects within a navigation or a
  // beacon.
  base::flat_set<SourceRegistrations> registrations_;

  // The number of connected receivers that may register a source. Used to
  // determine whether to buffer triggers. Event receivers are counted here
  // until they register a trigger.
  size_t data_hosts_in_source_mode_ = 0;
  base::OneShotTimer trigger_timer_;
  base::circular_deque<DelayedTrigger> delayed_triggers_;

  base::WeakPtrFactory<AttributionDataHostManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
