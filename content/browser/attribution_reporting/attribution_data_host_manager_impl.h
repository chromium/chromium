// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
#define CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/expected.h"
#include "components/attribution_reporting/data_host.mojom.h"
#include "components/attribution_reporting/registration_eligibility.mojom-forward.h"
#include "components/attribution_reporting/registration_header_error.h"
#include "content/browser/attribution_reporting/attribution_background_registrations_id.h"
#include "content/browser/attribution_reporting/attribution_beacon_id.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_reporting.mojom-forward.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "net/http/structured_headers.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/blink/public/common/tokens/tokens.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"

class GURL;

namespace attribution_reporting {
class SuitableOrigin;

enum class Registrar;

struct OsRegistrationItem;
struct RegistrationInfo;
struct SourceRegistration;
struct TriggerRegistration;
}  // namespace attribution_reporting

namespace content {

class AttributionManager;
class AttributionSuitableContext;
struct GlobalRenderFrameHostId;

// Manages a receiver set of all ongoing `AttributionDataHost`s and forwards
// events to the `AttributionManager` that owns `this`. Because attributionsrc
// requests may continue until after we have detached a frame, all browser
// process data needed to validate sources/triggers is stored alongside each
// receiver.
class CONTENT_EXPORT AttributionDataHostManagerImpl final
    : public AttributionDataHostManager,
      public attribution_reporting::mojom::DataHost {
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
      mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
      AttributionSuitableContext,
      attribution_reporting::mojom::RegistrationEligibility,
      bool is_for_background_requests) override;
  bool RegisterNavigationDataHost(
      mojo::PendingReceiver<attribution_reporting::mojom::DataHost> data_host,
      const blink::AttributionSrcToken& attribution_src_token) override;
  bool NotifyNavigationWithBackgroundRegistrationsWillStart(
      const blink::AttributionSrcToken& attribution_src_token,
      size_t expected_registrations) override;

  void NotifyNavigationRegistrationStarted(
      AttributionSuitableContext suitable_context,
      const blink::AttributionSrcToken& attribution_src_token,
      int64_t navigation_id,
      std::string devtools_request_id) override;
  bool NotifyNavigationRegistrationData(
      const blink::AttributionSrcToken& attribution_src_token,
      const net::HttpResponseHeaders* headers,
      GURL reporting_url) override;
  void NotifyNavigationRegistrationCompleted(
      const blink::AttributionSrcToken& attribution_src_token) override;

  void NotifyBackgroundRegistrationStarted(
      BackgroundRegistrationsId id,
      AttributionSuitableContext,
      attribution_reporting::mojom::RegistrationEligibility,
      std::optional<blink::AttributionSrcToken> attribution_src_token,
      std::optional<std::string> devtools_request_id) override;
  bool NotifyBackgroundRegistrationData(BackgroundRegistrationsId id,
                                        const net::HttpResponseHeaders* headers,
                                        GURL reporting_url) override;
  void NotifyBackgroundRegistrationCompleted(
      BackgroundRegistrationsId id) override;

  void NotifyFencedFrameReportingBeaconStarted(
      BeaconId beacon_id,
      AttributionSuitableContext,
      std::optional<int64_t> navigation_id,
      std::string devtools_request_id) override;
  void NotifyFencedFrameReportingBeaconData(
      BeaconId beacon_id,
      GURL reporting_url,
      const net::HttpResponseHeaders* headers,
      bool is_final_response) override;
  base::WeakPtr<AttributionDataHostManager> AsWeakPtr() override;

 private:
  class RegistrationContext;
  class NavigationForPendingRegistration;
  class OsRegistrationsBuffer;
  enum class OsRegistrationsBufferFlushReason;

  // Timer that can be used to be notified of sequential events. It uses a
  // single timer. When `Start` is called a timeout is added in a queue. If it
  // isn't already running, it starts the timer. When the timer expires, it pops
  // a timeout from the queue and runs the registered callback. If the queue is
  // not empty, it re-starts the timer for the timeout at the front of the
  // queue.
  class SequentialTimeoutsTimer {
   public:
    explicit SequentialTimeoutsTimer(base::TimeDelta delay);
    ~SequentialTimeoutsTimer();
    void Start(base::OnceClosure callback);

   private:
    struct Timeout {
      Timeout(base::TimeTicks time, base::OnceClosure callback);
      Timeout(Timeout&&);
      Timeout& operator=(Timeout&&);
      ~Timeout();

      base::TimeTicks time;
      base::OnceClosure callback;
    };

    void MaybeStartTimer();
    void ProcessTimeout();

    base::TimeDelta delay_;
    base::circular_deque<Timeout> timeouts_;
    base::OneShotTimer timer_;
  };

  struct DeferredReceiver;

  // Represents a set of attribution sources or triggers which registered in a
  // top-level navigation, a beacon chain or background requests and associated
  // info to process them.
  class Registrations;

  using RegistrationsId = absl::
      variant<blink::AttributionSrcToken, BeaconId, BackgroundRegistrationsId>;

  // attribution_reporting::mojom::DataHost:
  void SourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::SourceRegistration,
      bool was_fetched_via_service_worker) override;
  void TriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      attribution_reporting::TriggerRegistration,
      bool was_fetched_via_service_worker) override;
  void OsSourceDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      std::vector<attribution_reporting::OsRegistrationItem>,
      bool was_fetched_via_service_worker) override;
  void OsTriggerDataAvailable(
      attribution_reporting::SuitableOrigin reporting_origin,
      std::vector<attribution_reporting::OsRegistrationItem>,
      bool was_fetched_via_service_worker) override;
  void ReportRegistrationHeaderError(
      attribution_reporting::SuitableOrigin reporting_origin,
      const attribution_reporting::RegistrationHeaderError&) override;

  const RegistrationContext* GetReceiverRegistrationContextForSource();
  const RegistrationContext* GetReceiverRegistrationContextForTrigger();

  [[nodiscard]] bool CheckRegistrarSupport(
      attribution_reporting::Registrar,
      attribution_reporting::mojom::RegistrationType,
      const RegistrationContext&,
      const attribution_reporting::SuitableOrigin& reporting_origin);

  void OnReceiverDisconnected();

  struct HeaderPendingDecode;
  struct RegistrationDataHeaders;
  struct PendingRegistrationData;

  void HandleRegistrationData(base::flat_set<Registrations>::iterator,
                              PendingRegistrationData);
  void HandleNextRegistrationData(base::flat_set<Registrations>::iterator);
  using InfoParseResult =
      base::expected<net::structured_headers::Dictionary, std::string>;
  void OnInfoHeaderParsed(RegistrationsId, InfoParseResult);
  void HandleRegistrationInfo(base::flat_set<Registrations>::iterator,
                              PendingRegistrationData,
                              const attribution_reporting::RegistrationInfo&);

  void ParseHeader(base::flat_set<Registrations>::iterator,
                   HeaderPendingDecode,
                   attribution_reporting::Registrar);
  void HandleNextWebDecode(const Registrations&);
  void OnWebHeaderParsed(
      RegistrationsId,
      data_decoder::DataDecoder::ValueOrError result);
  void HandleParsedWebSource(const Registrations&,
                             HeaderPendingDecode&,
                             data_decoder::DataDecoder::ValueOrError result);
  void HandleParsedWebTrigger(const Registrations&,
                              HeaderPendingDecode&,
                              data_decoder::DataDecoder::ValueOrError result);

  void HandleNextOsDecode(const Registrations&);

  void MaybeLogAuditIssueAndReportHeaderError(
      const Registrations&,
      const HeaderPendingDecode&,
      attribution_reporting::RegistrationHeaderErrorDetails);

  using OsParseResult =
      base::expected<net::structured_headers::List, std::string>;
  void OnOsHeaderParsed(RegistrationsId,
                        OsParseResult);

  void MaybeOnRegistrationsFinished(
      base::flat_set<Registrations>::const_iterator);

  void MaybeStartNavigation(int64_t navigation_id);
  void MaybeDoneWithNavigation(int64_t navigation_id, bool due_to_timeout);

  [[nodiscard]] bool AddNavigationSourceRegistrationToBatchMap(
      int64_t navigation_id,
      const attribution_reporting::SuitableOrigin& reporting_origin,
      const attribution_reporting::SourceRegistration&,
      const GlobalRenderFrameHostId&,
      const std::optional<std::string>& devtools_request_id);
  void ClearRegistrationsForNavigationBatch(int64_t navigation_id);

  void MaybeBindDeferredReceivers(int64_t navigation_id, bool due_to_timeout);
  void ClearRegistrationsDeferUntilNavigation(int64_t navigation_id);

  void MaybeBufferOsRegistrations(
      int64_t navigation_id,
      std::vector<attribution_reporting::OsRegistrationItem>,
      const RegistrationContext&);
  void MaybeFlushOsRegistrationsBuffer(int64_t navigation_id,
                                       OsRegistrationsBufferFlushReason);
  void SubmitOsRegistrations(
      std::vector<attribution_reporting::OsRegistrationItem>,
      const RegistrationContext&,
      attribution_reporting::mojom::RegistrationType);

  // In `RegisterNavigationDataHost` which, for a given navigation, will be
  // called before `NotifyNavigationRegistrationStarted`, we receive the number
  // of background registrations expected for this navigation. This allows us to
  // keep the navigation context in
  // `navigations_waiting_on_background_registrations_` until the expected
  // number of registrations has been received.
  //
  // Whenever `count` background registrations are tied with their navigation
  // context, this method is called to reduce the number of expected
  // registrations and remove the value when all are tied.
  //
  // If `due_to_timeout` is true, we will mark all expected background
  // registrations as received.
  void BackgroundRegistrationsTied(const blink::AttributionSrcToken&,
                                   size_t count,
                                   bool due_to_timeout);

  void MaybeClearBackgroundRegistrationsWaitingOnNavigation(
      const blink::AttributionSrcToken&,
      bool due_to_timeout);

  // Owns `this`.
  const raw_ref<AttributionManager> attribution_manager_;

  mojo::ReceiverSet<attribution_reporting::mojom::DataHost, RegistrationContext>
      receivers_;

  // Map which stores pending receivers for data hosts which are going to
  // register sources associated with a navigation. These are not added to
  // `receivers_` until the necessary browser process information is available
  // to validate the attribution sources which is after the navigation starts.
  base::flat_map<blink::AttributionSrcToken,
                 mojo::PendingReceiver<attribution_reporting::mojom::DataHost>>
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
  base::flat_set<int64_t> ongoing_background_datahost_registrations_;

  // Background navigation-tied registrations notifications
  // (`NotifyBackgroundRegistrationStarted`) do not know the navigation-id of
  // the navigation to which they are tied. This id is received in foreground
  // registrations notifications (`NotifyNavigationRegistrationStarted`).
  //
  // We have no guarantees on the order in which we will receive `Started`
  // calls.
  //
  // If background registrations start before the navigation does,
  // `background_registrations_waiting_on_navigation_` is used to keep track of
  // registrations waiting on the navigation context.

  // If the navigation completes before the background registrations start,
  // `navigations_waiting_on_background_registrations_` is used to keep the
  // navigation context available for use when the background registrations
  // start.

  // If the navigation is ineligible, `NotifyNavigationRegistrationStarted` is
  // never called, the navigation will never be tied and the background
  // registrations will be dropped.
  //
  // Guardrails: when waiting on background registrations or navigations, we
  // start timeouts that will ensure that we never wait indefinitely.
  base::flat_map<blink::AttributionSrcToken,
                 base::flat_set<BackgroundRegistrationsId>>
      background_registrations_waiting_on_navigation_;
  SequentialTimeoutsTimer background_registrations_waiting_on_navigation_timer_;
  base::flat_map<blink::AttributionSrcToken, NavigationForPendingRegistration>
      navigations_waiting_on_background_registrations_;
  SequentialTimeoutsTimer
      navigations_waiting_on_background_registrations_timer_;

  // Stores registrations received on foreground navigations, background
  // registrations or via a Fenced Frame Beacon.
  base::flat_set<Registrations> registrations_;

  // The OS allows associating a navigation's input event to a single call. As a
  // result, we must buffer all registrations tied to a navigation before
  // submitting them to the OS.
  base::flat_set<OsRegistrationsBuffer> os_buffers_;

  // Guardrail to ensure that a navigation which can receive registrations is
  // always eventually considered done.
  SequentialTimeoutsTimer navigation_registrations_timer_;

  // Struct to contain useful information to be mapped against for limiting
  // navigation and metric purposes.
  struct ScopesAndCountForReportingOriginPerNavigation;

  // Stores the first received non-empty attribution scopes set for each
  // reporting origin tied to each navigation keyed by the navigation ID. Used
  // to limit source registrations per reporting origin per navigation to only 1
  // unique attribution scopes set. Also keeps count of each source registration
  // attempt per reporting origin.
  base::flat_map<int64_t,
                 base::flat_map<attribution_reporting::SuitableOrigin,
                                ScopesAndCountForReportingOriginPerNavigation>>
      registrations_count_and_set_scopes_per_navigation_;

  data_decoder::DataDecoder data_decoder_;

  base::WeakPtrFactory<AttributionDataHostManagerImpl> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_ATTRIBUTION_REPORTING_ATTRIBUTION_DATA_HOST_MANAGER_IMPL_H_
