// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_test_utils.h"

#include <stdint.h>

#include <algorithm>
#include <limits>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/buildflag.h"
#include "components/attribution_reporting/aggregatable_trigger_data.h"
#include "components/attribution_reporting/destination_set.h"
#include "components/attribution_reporting/event_trigger_data.h"
#include "components/attribution_reporting/filters.h"
#include "components/attribution_reporting/source_registration.h"
#include "components/attribution_reporting/source_registration_error.mojom.h"
#include "components/attribution_reporting/source_type.mojom.h"
#include "components/attribution_reporting/suitable_origin.h"
#include "components/attribution_reporting/trigger_registration.h"
#include "content/browser/attribution_reporting/attribution_config.h"
#include "content/browser/attribution_reporting/attribution_data_host_manager.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "content/public/browser/attribution_data_model.h"
#include "content/public/browser/navigation_handle.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "services/network/public/cpp/trigger_attestation.h"
#include "services/network/public/cpp/trigger_attestation_test_utils.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/attribution_reporting/attribution_input_event_tracker_android.h"
#endif

namespace content {

namespace {

using ::attribution_reporting::FilterPair;
using ::attribution_reporting::SuitableOrigin;
using ::attribution_reporting::mojom::SourceType;

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Property;

const char kDefaultSourceOrigin[] = "https://impression.test/";
const char kDefaultDestinationOrigin[] = "https://sub.conversion.test/";
const char kDefaultReportOrigin[] = "https://report.test/";

// Default expiry time for impressions for testing.
const int64_t kExpiryTime = 30;

}  // namespace

// static
MockAttributionHost* MockAttributionHost::Override(WebContents* web_contents) {
#if BUILDFLAG(IS_ANDROID)
  auto* old_host = AttributionHost::FromWebContents(web_contents);
  if (auto* input_event_tracker = old_host->input_event_tracker()) {
    input_event_tracker->RemoveObserverForTesting(web_contents);
  }
#endif
  auto host = base::WrapUnique(new MockAttributionHost(web_contents));
  auto* raw = host.get();
  web_contents->SetUserData(AttributionHost::UserDataKey(), std::move(host));
  return raw;
}

MockAttributionHost::MockAttributionHost(WebContents* web_contents)
    : AttributionHost(web_contents) {}

MockAttributionHost::~MockAttributionHost() = default;

MockDataHost::MockDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  receiver_.Bind(std::move(data_host));
}

MockDataHost::~MockDataHost() = default;

void MockDataHost::WaitForSourceData(size_t num_source_data) {
  min_source_data_count_ = num_source_data;
  if (source_data_.size() >= min_source_data_count_) {
    return;
  }
  wait_loop_.Run();
}

void MockDataHost::WaitForTriggerData(size_t num_trigger_data) {
  min_trigger_data_count_ = num_trigger_data;
  if (trigger_data_.size() >= min_trigger_data_count_) {
    return;
  }
  wait_loop_.Run();
}

void MockDataHost::SourceDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::SourceRegistration data) {
  source_data_.push_back(std::move(data));
  if (source_data_.size() < min_source_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

void MockDataHost::TriggerDataAvailable(
    attribution_reporting::SuitableOrigin reporting_origin,
    attribution_reporting::TriggerRegistration data,
    absl::optional<network::TriggerAttestation> attestation) {
  trigger_data_.push_back(std::move(data));
  if (trigger_data_.size() < min_trigger_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

MockAttributionObserver::MockAttributionObserver() = default;

MockAttributionObserver::~MockAttributionObserver() = default;

base::GUID DefaultExternalReportID() {
  return base::GUID::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
}

ConfigurableStorageDelegate::ConfigurableStorageDelegate()
    : AttributionStorageDelegate(AttributionConfig{
          .max_sources_per_origin = std::numeric_limits<int>::max(),
          .source_event_id_cardinality = absl::nullopt,
          .max_destinations_per_source_site_reporting_origin =
              std::numeric_limits<int>::max(),
          .rate_limit =
              {
                  .time_window = base::TimeDelta::Max(),
                  .max_source_registration_reporting_origins =
                      std::numeric_limits<int64_t>::max(),
                  .max_attribution_reporting_origins =
                      std::numeric_limits<int64_t>::max(),
                  .max_attributions = std::numeric_limits<int64_t>::max(),
              },
          .event_level_limit =
              {
                  .navigation_source_trigger_data_cardinality =
                      std::numeric_limits<uint64_t>::max(),
                  .event_source_trigger_data_cardinality =
                      std::numeric_limits<uint64_t>::max(),
                  .navigation_source_randomized_response_rate = 0,
                  .event_source_randomized_response_rate = 0,
                  .max_reports_per_destination =
                      std::numeric_limits<int>::max(),
                  .max_attributions_per_navigation_source =
                      std::numeric_limits<int>::max(),
                  .max_attributions_per_event_source =
                      std::numeric_limits<int>::max(),
              },
          .aggregate_limit =
              {
                  .max_reports_per_destination =
                      std::numeric_limits<int>::max(),
                  .aggregatable_budget_per_source =
                      std::numeric_limits<int64_t>::max(),
                  .min_delay = base::TimeDelta(),
                  .delay_span = base::TimeDelta(),
              },
      }) {}

ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

void ConfigurableStorageDelegate::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::Time ConfigurableStorageDelegate::GetEventLevelReportTime(
    const CommonSourceInfo& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return source.source_time() + report_delay_;
}

base::Time ConfigurableStorageDelegate::GetAggregatableReportTime(
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return trigger_time + report_delay_;
}

base::TimeDelta ConfigurableStorageDelegate::GetDeleteExpiredSourcesFrequency()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delete_expired_sources_frequency_;
}

base::TimeDelta
ConfigurableStorageDelegate::GetDeleteExpiredRateLimitsFrequency() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return delete_expired_rate_limits_frequency_;
}

base::GUID ConfigurableStorageDelegate::NewReportID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return DefaultExternalReportID();
}

absl::optional<AttributionStorageDelegate::OfflineReportDelayConfig>
ConfigurableStorageDelegate::GetOfflineReportDelayConfig() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return offline_report_delay_config_;
}

void ConfigurableStorageDelegate::ShuffleReports(
    std::vector<AttributionReport>& reports) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (reverse_reports_on_shuffle_) {
    base::ranges::reverse(reports);
  }
}

AttributionStorageDelegate::RandomizedResponse
ConfigurableStorageDelegate::GetRandomizedResponse(
    const CommonSourceInfo& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return randomized_response_;
}

void ConfigurableStorageDelegate::set_max_attributions_per_source(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.event_level_limit.max_attributions_per_navigation_source = max;
  config_.event_level_limit.max_attributions_per_event_source = max;
}

void ConfigurableStorageDelegate::set_max_sources_per_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.max_sources_per_origin = max;
}

void ConfigurableStorageDelegate::set_max_reports_per_destination(
    AttributionReport::Type report_type,
    int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (report_type) {
    case AttributionReport::Type::kEventLevel:
      config_.event_level_limit.max_reports_per_destination = max;
      break;
    case AttributionReport::Type::kAggregatableAttribution:
      config_.aggregate_limit.max_reports_per_destination = max;
      break;
  }
}

void ConfigurableStorageDelegate::
    set_max_destinations_per_source_site_reporting_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.max_destinations_per_source_site_reporting_origin = max;
}

void ConfigurableStorageDelegate::set_aggregatable_budget_per_source(
    int64_t max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.aggregate_limit.aggregatable_budget_per_source = max;
}

void ConfigurableStorageDelegate::set_rate_limits(
    AttributionConfig::RateLimitConfig c) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(c.Validate());
  config_.rate_limit = c;
}

void ConfigurableStorageDelegate::set_delete_expired_sources_frequency(
    base::TimeDelta frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete_expired_sources_frequency_ = frequency;
}

void ConfigurableStorageDelegate::set_delete_expired_rate_limits_frequency(
    base::TimeDelta frequency) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delete_expired_rate_limits_frequency_ = frequency;
}

void ConfigurableStorageDelegate::set_report_delay(
    base::TimeDelta report_delay) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  report_delay_ = report_delay;
}

void ConfigurableStorageDelegate::set_offline_report_delay_config(
    absl::optional<OfflineReportDelayConfig> config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  offline_report_delay_config_ = config;
}

void ConfigurableStorageDelegate::set_reverse_reports_on_shuffle(bool reverse) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reverse_reports_on_shuffle_ = reverse;
}

void ConfigurableStorageDelegate::set_randomized_response_rates(
    double navigation,
    double event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  config_.event_level_limit.navigation_source_randomized_response_rate =
      navigation;
  config_.event_level_limit.event_source_randomized_response_rate = event;
}

void ConfigurableStorageDelegate::set_randomized_response(
    RandomizedResponse randomized_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  randomized_response_ = std::move(randomized_response);
}

void ConfigurableStorageDelegate::set_trigger_data_cardinality(
    uint64_t navigation,
    uint64_t event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(navigation, 0u);
  DCHECK_GT(event, 0u);

  config_.event_level_limit.navigation_source_trigger_data_cardinality =
      navigation;
  config_.event_level_limit.event_source_trigger_data_cardinality = event;
}

void ConfigurableStorageDelegate::set_source_event_id_cardinality(
    uint64_t cardinality) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(cardinality, 0u);

  config_.source_event_id_cardinality = cardinality;
}

MockAttributionManager::MockAttributionManager() = default;

MockAttributionManager::~MockAttributionManager() = default;

void MockAttributionManager::AddObserver(AttributionObserver* observer) {
  observers_.AddObserver(observer);
}

void MockAttributionManager::RemoveObserver(AttributionObserver* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* MockAttributionManager::GetDataHostManager() {
  return data_host_manager_.get();
}

void MockAttributionManager::NotifySourcesChanged() {
  for (auto& observer : observers_) {
    observer.OnSourcesChanged();
  }
}

void MockAttributionManager::NotifyReportsChanged(
    AttributionReport::Type report_type) {
  for (auto& observer : observers_) {
    observer.OnReportsChanged(report_type);
  }
}

void MockAttributionManager::NotifySourceHandled(
    const StorableSource& source,
    StorableSource::Result result,
    absl::optional<uint64_t> cleared_debug_key) {
  for (auto& observer : observers_) {
    observer.OnSourceHandled(source, cleared_debug_key, result);
  }
}

void MockAttributionManager::NotifyReportSent(const AttributionReport& report,
                                              bool is_debug_report,
                                              const SendResult& info) {
  for (auto& observer : observers_) {
    observer.OnReportSent(report, is_debug_report, info);
  }
}

void MockAttributionManager::NotifySourceRegistrationFailure(
    const std::string& header_value,
    const SuitableOrigin& source_origin,
    const SuitableOrigin& reporting_origin,
    SourceType source_type,
    attribution_reporting::mojom::SourceRegistrationError error) {
  base::Time source_time = base::Time::Now();
  for (auto& observer : observers_) {
    observer.OnFailedSourceRegistration(header_value, source_time,
                                        source_origin, reporting_origin,
                                        source_type, error);
  }
}

void MockAttributionManager::NotifyTriggerHandled(
    const AttributionTrigger& trigger,
    const CreateReportResult& result,
    absl::optional<uint64_t> cleared_debug_key) {
  for (auto& observer : observers_) {
    observer.OnTriggerHandled(trigger, cleared_debug_key, result);
  }
}

void MockAttributionManager::NotifyDebugReportSent(
    const AttributionDebugReport& report,
    const int status,
    const base::Time time) {
  for (auto& observer : observers_) {
    observer.OnDebugReportSent(report, status, time);
  }
}

void MockAttributionManager::SetDataHostManager(
    std::unique_ptr<AttributionDataHostManager> manager) {
  data_host_manager_ = std::move(manager);
}

SourceObserver::SourceObserver(WebContents* contents, size_t num_impressions)
    : TestNavigationObserver(contents),
      expected_num_impressions_(num_impressions) {}

SourceObserver::~SourceObserver() = default;

void SourceObserver::OnDidFinishNavigation(
    NavigationHandle* navigation_handle) {
  if (!navigation_handle->GetImpression()) {
    if (waiting_for_null_impression_) {
      impression_loop_.Quit();
    }
    return;
  }

  last_impression_ = *(navigation_handle->GetImpression());
  num_impressions_++;

  if (!waiting_for_null_impression_ &&
      num_impressions_ >= expected_num_impressions_) {
    impression_loop_.Quit();
  }
}

// Waits for |expected_num_impressions_| navigations with impressions, and
// returns the last impression.
const blink::Impression& SourceObserver::Wait() {
  if (num_impressions_ >= expected_num_impressions_) {
    return *last_impression_;
  }
  impression_loop_.Run();
  return last_impression();
}

bool SourceObserver::WaitForNavigationWithNoImpression() {
  waiting_for_null_impression_ = true;
  impression_loop_.Run();
  waiting_for_null_impression_ = false;
  return true;
}

// Builds an impression with default values. This is done as a builder because
// all values needed to be provided at construction time.
SourceBuilder::SourceBuilder(base::Time time)
    : source_time_(time),
      expiry_(base::Milliseconds(kExpiryTime)),
      source_origin_(*SuitableOrigin::Deserialize(kDefaultSourceOrigin)),
      destination_sites_(*attribution_reporting::DestinationSet::Create(
          {net::SchemefulSite::Deserialize(kDefaultDestinationOrigin)})),
      reporting_origin_(*SuitableOrigin::Deserialize(kDefaultReportOrigin)) {}

SourceBuilder::~SourceBuilder() = default;

SourceBuilder::SourceBuilder(const SourceBuilder&) = default;

SourceBuilder::SourceBuilder(SourceBuilder&&) = default;

SourceBuilder& SourceBuilder::operator=(const SourceBuilder&) = default;

SourceBuilder& SourceBuilder::operator=(SourceBuilder&&) = default;

SourceBuilder& SourceBuilder::SetExpiry(base::TimeDelta delta) {
  expiry_ = delta;
  return *this;
}

SourceBuilder& SourceBuilder::SetEventReportWindow(base::TimeDelta delta) {
  event_report_window_ = delta;
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregatableReportWindow(
    base::TimeDelta delta) {
  aggregatable_report_window_ = delta;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceEventId(uint64_t source_event_id) {
  source_event_id_ = source_event_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceOrigin(SuitableOrigin origin) {
  source_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetDestinationOrigin(
    const SuitableOrigin& origin) {
  return SetDestinationSites({net::SchemefulSite(origin)});
}

SourceBuilder& SourceBuilder::SetDestinationSites(
    base::flat_set<net::SchemefulSite> sites) {
  destination_sites_ =
      *attribution_reporting::DestinationSet::Create(std::move(sites));
  return *this;
}

SourceBuilder& SourceBuilder::SetReportingOrigin(SuitableOrigin origin) {
  reporting_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceType(SourceType source_type) {
  source_type_ = source_type;
  return *this;
}

SourceBuilder& SourceBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

SourceBuilder& SourceBuilder::SetFilterData(
    attribution_reporting::FilterData filter_data) {
  filter_data_ = std::move(filter_data);
  return *this;
}

SourceBuilder& SourceBuilder::SetDebugKey(absl::optional<uint64_t> debug_key) {
  debug_key_ = debug_key;
  return *this;
}

SourceBuilder& SourceBuilder::SetAttributionLogic(
    StoredSource::AttributionLogic attribution_logic) {
  attribution_logic_ = attribution_logic;
  return *this;
}

SourceBuilder& SourceBuilder::SetActiveState(
    StoredSource::ActiveState active_state) {
  active_state_ = active_state;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceId(StoredSource::Id source_id) {
  source_id_ = source_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetDedupKeys(std::vector<uint64_t> dedup_keys) {
  dedup_keys_ = std::move(dedup_keys);
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregationKeys(
    attribution_reporting::AggregationKeys aggregation_keys) {
  aggregation_keys_ = std::move(aggregation_keys);
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregatableBudgetConsumed(
    int64_t aggregatable_budget_consumed) {
  aggregatable_budget_consumed_ = aggregatable_budget_consumed;
  return *this;
}

SourceBuilder& SourceBuilder::SetAggregatableDedupKeys(
    std::vector<uint64_t> dedup_keys) {
  aggregatable_dedup_keys_ = std::move(dedup_keys);
  return *this;
}

SourceBuilder& SourceBuilder::SetIsWithinFencedFrame(
    bool is_within_fenced_frame) {
  is_within_fenced_frame_ = is_within_fenced_frame;
  return *this;
}

SourceBuilder& SourceBuilder::SetDebugReporting(bool debug_reporting) {
  debug_reporting_ = debug_reporting;
  return *this;
}

CommonSourceInfo SourceBuilder::BuildCommonInfo() const {
  return CommonSourceInfo(
      source_event_id_, source_origin_, destination_sites_, reporting_origin_,
      source_time_,
      /*expiry_time=*/source_time_ + expiry_,
      /*event_report_window_time=*/
      event_report_window_
          ? absl::make_optional(source_time_ + *event_report_window_)
          : absl::nullopt,
      /*aggregatable_report_window_time=*/
      aggregatable_report_window_
          ? absl::make_optional(source_time_ + *aggregatable_report_window_)
          : absl::nullopt,
      source_type_, priority_, filter_data_, debug_key_, aggregation_keys_);
}

StorableSource SourceBuilder::Build() const {
  return StorableSource(BuildCommonInfo(), is_within_fenced_frame_,
                        debug_reporting_);
}

StoredSource SourceBuilder::BuildStored() const {
  StoredSource source(BuildCommonInfo(), attribution_logic_, active_state_,
                      source_id_, aggregatable_budget_consumed_);
  source.SetDedupKeys(dedup_keys_);
  source.SetAggregatableDedupKeys(aggregatable_dedup_keys_);
  return source;
}

AttributionTrigger DefaultTrigger() {
  return TriggerBuilder().Build();
}

TriggerBuilder::TriggerBuilder()
    : destination_origin_(
          *SuitableOrigin::Deserialize(kDefaultDestinationOrigin)),
      reporting_origin_(*SuitableOrigin::Deserialize(kDefaultReportOrigin)) {}

TriggerBuilder::~TriggerBuilder() = default;

TriggerBuilder::TriggerBuilder(const TriggerBuilder&) = default;

TriggerBuilder::TriggerBuilder(TriggerBuilder&&) = default;

TriggerBuilder& TriggerBuilder::operator=(const TriggerBuilder&) = default;

TriggerBuilder& TriggerBuilder::operator=(TriggerBuilder&&) = default;

TriggerBuilder& TriggerBuilder::SetTriggerData(uint64_t trigger_data) {
  trigger_data_ = trigger_data;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetEventSourceTriggerData(
    uint64_t event_source_trigger_data) {
  event_source_trigger_data_ = event_source_trigger_data;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDestinationOrigin(SuitableOrigin origin) {
  destination_origin_ = std::move(origin);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetReportingOrigin(SuitableOrigin origin) {
  reporting_origin_ = std::move(origin);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDedupKey(
    absl::optional<uint64_t> dedup_key) {
  dedup_key_ = dedup_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDebugKey(
    absl::optional<uint64_t> debug_key) {
  debug_key_ = debug_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableTriggerData(
    std::vector<attribution_reporting::AggregatableTriggerData>
        aggregatable_trigger_data) {
  aggregatable_trigger_data_ = std::move(aggregatable_trigger_data);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableValues(
    attribution_reporting::AggregatableValues aggregatable_values) {
  aggregatable_values_ = std::move(aggregatable_values);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregatableDedupKey(
    absl::optional<uint64_t> aggregatable_dedup_key) {
  aggregatable_dedup_key_ = aggregatable_dedup_key;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetIsWithinFencedFrame(
    bool is_within_fenced_frame) {
  is_within_fenced_frame_ = is_within_fenced_frame;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetDebugReporting(bool debug_reporting) {
  debug_reporting_ = debug_reporting;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAggregationCoordinator(
    ::aggregation_service::mojom::AggregationCoordinator
        aggregation_coordinator) {
  aggregation_coordinator_ = aggregation_coordinator;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetAttestation(
    absl::optional<network::TriggerAttestation> attestation) {
  attestation_ = std::move(attestation);
  return *this;
}

AttributionTrigger TriggerBuilder::Build(
    bool generate_event_trigger_data) const {
  std::vector<attribution_reporting::EventTriggerData> event_triggers;

  if (generate_event_trigger_data) {
    event_triggers.emplace_back(
        trigger_data_, priority_, dedup_key_,
        FilterPair{.positive =
                       attribution_reporting::Filters::ForSourceTypeForTesting(
                           SourceType::kNavigation)});

    event_triggers.emplace_back(
        event_source_trigger_data_, priority_, dedup_key_,
        attribution_reporting::FilterPair{
            .positive = attribution_reporting::Filters::ForSourceTypeForTesting(
                SourceType::kEvent)});
  }

  return AttributionTrigger(
      reporting_origin_,
      attribution_reporting::TriggerRegistration(
          FilterPair(), debug_key_,
          *attribution_reporting::AggregatableDedupKeyList::Create(
              {attribution_reporting::AggregatableDedupKey(
                  /*dedup_key=*/aggregatable_dedup_key_, FilterPair())}),
          *attribution_reporting::EventTriggerDataList::Create(
              std::move(event_triggers)),
          *attribution_reporting::AggregatableTriggerDataList::Create(
              aggregatable_trigger_data_),
          aggregatable_values_, debug_reporting_, aggregation_coordinator_),
      destination_origin_, attestation_, is_within_fenced_frame_);
}

AttributionInfoBuilder::AttributionInfoBuilder(
    StoredSource source,
    attribution_reporting::SuitableOrigin context_origin)
    : source_(std::move(source)), context_origin_(std::move(context_origin)) {}

AttributionInfoBuilder::~AttributionInfoBuilder() = default;

AttributionInfoBuilder& AttributionInfoBuilder::SetTime(base::Time time) {
  time_ = time;
  return *this;
}

AttributionInfoBuilder& AttributionInfoBuilder::SetDebugKey(
    absl::optional<uint64_t> debug_key) {
  debug_key_ = debug_key;
  return *this;
}

AttributionInfo AttributionInfoBuilder::Build() const {
  return AttributionInfo(source_, time_, debug_key_, context_origin_);
}

ReportBuilder::ReportBuilder(AttributionInfo attribution_info)
    : attribution_info_(std::move(attribution_info)),
      external_report_id_(DefaultExternalReportID()) {}

ReportBuilder::~ReportBuilder() = default;

ReportBuilder& ReportBuilder::SetTriggerData(uint64_t trigger_data) {
  trigger_data_ = trigger_data;
  return *this;
}

ReportBuilder& ReportBuilder::SetReportTime(base::Time time) {
  report_time_ = time;
  return *this;
}

ReportBuilder& ReportBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

ReportBuilder& ReportBuilder::SetExternalReportId(
    base::GUID external_report_id) {
  external_report_id_ = std::move(external_report_id);
  return *this;
}

ReportBuilder& ReportBuilder::SetRandomizedTriggerRate(double rate) {
  randomized_trigger_rate_ = rate;
  return *this;
}

ReportBuilder& ReportBuilder::SetReportId(
    AttributionReport::EventLevelData::Id id) {
  report_id_ = id;
  return *this;
}

ReportBuilder& ReportBuilder::SetReportId(
    AttributionReport::AggregatableAttributionData::Id id) {
  aggregatable_attribution_report_id_ = id;
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregatableHistogramContributions(
    std::vector<AggregatableHistogramContribution> contributions) {
  DCHECK(!contributions.empty());
  contributions_ = std::move(contributions);
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregationCoordinator(
    ::aggregation_service::mojom::AggregationCoordinator
        aggregation_coordinator) {
  aggregation_coordinator_ = aggregation_coordinator;
  return *this;
}

ReportBuilder& ReportBuilder::SetAttestationToken(
    absl::optional<std::string> attestation_token) {
  attestation_token_ = std::move(attestation_token);
  return *this;
}

AttributionReport ReportBuilder::Build() const {
  return AttributionReport(
      attribution_info_, report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::EventLevelData(trigger_data_, priority_,
                                        randomized_trigger_rate_, report_id_));
}

AttributionReport ReportBuilder::BuildAggregatableAttribution() const {
  return AttributionReport(
      attribution_info_, report_time_, external_report_id_,
      /*failed_send_attempts=*/0,
      AttributionReport::AggregatableAttributionData(
          contributions_, aggregatable_attribution_report_id_, report_time_,
          aggregation_coordinator_, attestation_token_));
}

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b) {
  const auto tie = [](const AttributionTrigger& t) {
    return std::make_tuple(t.registration(), t.destination_origin(),
                           t.is_within_fenced_frame());
  };
  return tie(a) == tie(b);
}

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b) {
  const auto tie = [](const CommonSourceInfo& source) {
    return std::make_tuple(
        source.source_event_id(), source.source_origin(),
        source.destination_sites().destinations(), source.reporting_origin(),
        source.source_time(), source.expiry_time(),
        source.event_report_window_time(),
        source.aggregatable_report_window_time(), source.source_type(),
        source.priority(), source.filter_data(), source.debug_key(),
        source.aggregation_keys());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionInfo& a, const AttributionInfo& b) {
  const auto tie = [](const AttributionInfo& attribution_info) {
    return std::make_tuple(attribution_info.source, attribution_info.time,
                           attribution_info.debug_key,
                           attribution_info.context_origin);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionStorageDelegate::FakeReport& a,
                const AttributionStorageDelegate::FakeReport& b) {
  const auto tie = [](const AttributionStorageDelegate::FakeReport& r) {
    return std::make_tuple(r.trigger_data, r.trigger_time, r.report_time);
  };
  return tie(a) == tie(b);
}

bool operator<(const AttributionStorageDelegate::FakeReport& a,
               const AttributionStorageDelegate::FakeReport& b) {
  const auto tie = [](const AttributionStorageDelegate::FakeReport& r) {
    return std::make_tuple(r.trigger_data, r.trigger_time, r.report_time);
  };
  return tie(a) < tie(b);
}

bool operator==(const StorableSource& a, const StorableSource& b) {
  const auto tie = [](const StorableSource& source) {
    return std::make_tuple(source.common_info(),
                           source.is_within_fenced_frame(),
                           source.debug_reporting());
  };
  return tie(a) == tie(b);
}

// Does not compare source IDs, as they are set by the underlying sqlite DB and
// should not be tested.
bool operator==(const StoredSource& a, const StoredSource& b) {
  const auto tie = [](const StoredSource& source) {
    return std::make_tuple(source.common_info(), source.attribution_logic(),
                           source.active_state(), source.dedup_keys(),
                           source.aggregatable_budget_consumed(),
                           source.aggregatable_dedup_keys());
  };
  return tie(a) == tie(b);
}

bool operator==(const AggregatableHistogramContribution& a,
                const AggregatableHistogramContribution& b) {
  const auto tie = [](const AggregatableHistogramContribution& contribution) {
    return std::make_tuple(contribution.key(), contribution.value());
  };
  return tie(a) == tie(b);
}

// Does not compare ID as it is set by the underlying sqlite db and
// should not be tested.
bool operator==(const AttributionReport::EventLevelData& a,
                const AttributionReport::EventLevelData& b) {
  const auto tie = [](const AttributionReport::EventLevelData& data) {
    return std::make_tuple(data.trigger_data, data.priority,
                           data.randomized_trigger_rate);
  };
  return tie(a) == tie(b);
}

// Does not compare ID as it is set by the underlying sqlite db and
// should not be tested.
// Also does not compare the assembled report as it is returned by the
// aggregation service from all the other data.
bool operator==(const AttributionReport::AggregatableAttributionData& a,
                const AttributionReport::AggregatableAttributionData& b) {
  const auto tie =
      [](const AttributionReport::AggregatableAttributionData& data) {
        return std::make_tuple(data.contributions, data.initial_report_time,
                               data.attestation_token);
      };
  return tie(a) == tie(b);
}

// Does not compare source or report IDs, as they are set by the underlying
// sqlite DB and should not be tested.
bool operator==(const AttributionReport& a, const AttributionReport& b) {
  const auto tie = [](const AttributionReport& report) {
    return std::make_tuple(report.attribution_info(), report.report_time(),
                           report.external_report_id(),
                           report.failed_send_attempts(), report.data());
  };
  return tie(a) == tie(b);
}

bool operator==(const SendResult& a, const SendResult& b) {
  const auto tie = [](const SendResult& info) {
    return std::make_tuple(info.status, info.network_error,
                           info.http_response_code);
  };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::EventLevelResult status) {
  switch (status) {
    case AttributionTrigger::EventLevelResult::kSuccess:
      return out << "success";
    case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
      return out << "successDroppedLowerPriority";
    case AttributionTrigger::EventLevelResult::kInternalError:
      return out << "internalError";
    case AttributionTrigger::EventLevelResult::
        kNoCapacityForConversionDestination:
      return out << "insufficientDestinationCapacity";
    case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      return out << "noMatchingSources";
    case AttributionTrigger::EventLevelResult::kDeduplicated:
      return out << "deduplicated";
    case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      return out << "excessiveAttributions";
    case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      return out << "priorityTooLow";
    case AttributionTrigger::EventLevelResult::kDroppedForNoise:
      return out << "noised";
    case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      return out << "excessiveReportingOrigins";
    case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      return out << "noMatchingSourceFilterData";
    case AttributionTrigger::EventLevelResult::kProhibitedByBrowserPolicy:
      return out << "prohibitedByBrowserPolicy";
    case AttributionTrigger::EventLevelResult::kNoMatchingConfigurations:
      return out << "noMatchingConfigurations";
    case AttributionTrigger::EventLevelResult::kExcessiveReports:
      return out << "excessiveReports";
    case AttributionTrigger::EventLevelResult::kFalselyAttributedSource:
      return out << "falselyAttributedSource";
    case AttributionTrigger::EventLevelResult::kReportWindowPassed:
      return out << "reportWindowPassed";
    case AttributionTrigger::EventLevelResult::kNotRegistered:
      return out << "notRegistered";
  }
}

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::AggregatableResult status) {
  switch (status) {
    case AttributionTrigger::AggregatableResult::kSuccess:
      return out << "success";
    case AttributionTrigger::AggregatableResult::kInternalError:
      return out << "internalError";
    case AttributionTrigger::AggregatableResult::
        kNoCapacityForConversionDestination:
      return out << "insufficientDestinationCapacity";
    case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
      return out << "noMatchingSources";
    case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      return out << "excessiveAttributions";
    case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      return out << "excessiveReportingOrigins";
    case AttributionTrigger::AggregatableResult::kNoHistograms:
      return out << "noHistograms";
    case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      return out << "insufficientBudget";
    case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      return out << "noMatchingSourceFilterData";
    case AttributionTrigger::AggregatableResult::kNotRegistered:
      return out << "notRegistered";
    case AttributionTrigger::AggregatableResult::kProhibitedByBrowserPolicy:
      return out << "prohibitedByBrowserPolicy";
    case AttributionTrigger::AggregatableResult::kDeduplicated:
      return out << "deduplicated";
    case AttributionTrigger::AggregatableResult::kReportWindowPassed:
      return out << "reportWindowPassed";
  }
}

std::ostream& operator<<(std::ostream& out, RateLimitResult result) {
  switch (result) {
    case RateLimitResult::kAllowed:
      return out << "kAllowed";
    case RateLimitResult::kNotAllowed:
      return out << "kNotAllowed";
    case RateLimitResult::kError:
      return out << "kError";
  }
}

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic) {
  switch (attribution_logic) {
    case StoredSource::AttributionLogic::kNever:
      return out << "kNever";
    case StoredSource::AttributionLogic::kTruthfully:
      return out << "kTruthfully";
    case StoredSource::AttributionLogic::kFalsely:
      return out << "kFalsely";
  }
}

std::ostream& operator<<(std::ostream& out,
                         StoredSource::ActiveState active_state) {
  switch (active_state) {
    case StoredSource::ActiveState::kActive:
      return out << "kActive";
    case StoredSource::ActiveState::kInactive:
      return out << "kInactive";
    case StoredSource::ActiveState::kReachedEventLevelAttributionLimit:
      return out << "kReachedEventLevelAttributionLimit";
  }
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion) {
  out << "{registration=" << conversion.registration()
      << ",destination_origin=" << conversion.destination_origin()
      << ",is_within_fenced_frame=" << conversion.is_within_fenced_frame();

  if (conversion.attestation().has_value()) {
    out << ",attestation=" << conversion.attestation().value();
  } else {
    out << ",attestation=(null)";
  }

  return out << "}";
}

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source) {
  return out << "{source_event_id=" << source.source_event_id()
             << ",source_origin=" << source.source_origin()
             << "destination_sites=" << source.destination_sites()
             << "reporting_origin=" << source.reporting_origin()
             << ",source_time=" << source.source_time()
             << ",expiry_time=" << source.expiry_time()
             << ",event_report_window_time="
             << source.event_report_window_time()
             << ",aggregatable_report_window_time="
             << source.aggregatable_report_window_time()
             << ",source_type=" << source.source_type()
             << ",priority=" << source.priority()
             << ",filter_data=" << source.filter_data() << ",debug_key="
             << (source.debug_key() ? base::NumberToString(*source.debug_key())
                                    : "null")
             << ",aggregation_keys=" << source.aggregation_keys() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info) {
  return out << "{source=" << attribution_info.source
             << ",time=" << attribution_info.time << ",debug_key="
             << (attribution_info.debug_key
                     ? base::NumberToString(*attribution_info.debug_key)
                     : "null")
             << ",context_origin=" << attribution_info.context_origin << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionStorageDelegate::FakeReport& r) {
  return out << "{trigger_data=" << r.trigger_data
             << ",trigger_time=" << r.trigger_time
             << ",report_time=" << r.report_time << "}";
}

std::ostream& operator<<(std::ostream& out, const StorableSource& source) {
  return out << "{common_info=" << source.common_info()
             << ",is_within_fenced_frame=" << source.is_within_fenced_frame()
             << ",debug_reporting=" << source.debug_reporting() << "}";
}

std::ostream& operator<<(std::ostream& out, const StoredSource& source) {
  out << "{common_info=" << source.common_info()
      << ",attribution_logic=" << source.attribution_logic()
      << ",active_state=" << source.active_state()
      << ",source_id=" << *source.source_id()
      << ",aggregatable_budget_consumed="
      << source.aggregatable_budget_consumed() << ",dedup_keys=[";

  const char* separator = "";
  for (int64_t dedup_key : source.dedup_keys()) {
    out << separator << dedup_key;
    separator = ", ";
  }

  out << "],aggregatable_dedup_keys=[";

  separator = "";
  for (int64_t dedup_key : source.aggregatable_dedup_keys()) {
    out << separator << dedup_key;
    separator = ",";
  }

  return out << "]}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AggregatableHistogramContribution& contribution) {
  return out << "{key=" << contribution.key()
             << ",value=" << contribution.value() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data) {
  return out << "{trigger_data=" << data.trigger_data
             << ",priority=" << data.priority
             << ",randomized_trigger_rate=" << data.randomized_trigger_rate
             << ",id=" << *data.id << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableAttributionData& data) {
  out << "{contributions=[";

  const char* separator = "";
  for (const auto& contribution : data.contributions) {
    out << separator << contribution;
    separator = ", ";
  }

  out << "],id=" << *data.id
      << ",initial_report_time=" << data.initial_report_time;
  if (data.attestation_token.has_value()) {
    out << ",attestation_token=" << data.attestation_token.value();
  } else {
    out << ",attestation_token=(null)";
  }
  return out << "}";
}

namespace {
std::ostream& operator<<(
    std::ostream& out,
    const absl::variant<AttributionReport::EventLevelData,
                        AttributionReport::AggregatableAttributionData>& data) {
  absl::visit([&out](const auto& v) { out << v; }, data);
  return out;
}
}  // namespace

std::ostream& operator<<(std::ostream& out, const AttributionReport& report) {
  return out << "{attribution_info=" << report.attribution_info()
             << ",report_time=" << report.report_time()
             << ",external_report_id=" << report.external_report_id()
             << ",failed_send_attempts=" << report.failed_send_attempts()
             << ",data=" << report.data() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         AttributionReport::Type report_type) {
  switch (report_type) {
    case AttributionReport::Type::kEventLevel:
      return out << "kEventLevel";
    case AttributionReport::Type::kAggregatableAttribution:
      return out << "kAggregatableAttribution";
  }
}

std::ostream& operator<<(std::ostream& out, SendResult::Status status) {
  switch (status) {
    case SendResult::Status::kSent:
      return out << "kSent";
    case SendResult::Status::kTransientFailure:
      return out << "kTransientFailure";
    case SendResult::Status::kFailure:
      return out << "kFailure";
    case SendResult::Status::kDropped:
      return out << "kDropped";
    case SendResult::Status::kFailedToAssemble:
      return out << "kFailedToAssemble";
  }
}

std::ostream& operator<<(std::ostream& out, const SendResult& info) {
  return out << "{status=" << info.status
             << ",network_error=" << net::ErrorToShortString(info.network_error)
             << ",http_response_code=" << info.http_response_code << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionDataModel::DataKey& key) {
  return out << "{reporting_origin=" << key.reporting_origin() << "}";
}

EventTriggerDataMatcherConfig::EventTriggerDataMatcherConfig(
    ::testing::Matcher<uint64_t> data,
    ::testing::Matcher<int64_t> priority,
    ::testing::Matcher<absl::optional<uint64_t>> dedup_key,
    ::testing::Matcher<const FilterPair&> filters)
    : data(std::move(data)),
      priority(std::move(priority)),
      dedup_key(std::move(dedup_key)),
      filters(std::move(filters)) {}

EventTriggerDataMatcherConfig::~EventTriggerDataMatcherConfig() = default;

::testing::Matcher<const attribution_reporting::EventTriggerData&>
EventTriggerDataMatches(const EventTriggerDataMatcherConfig& cfg) {
  return ::testing::AllOf(
      Field("data", &attribution_reporting::EventTriggerData::data, cfg.data),
      Field("priority", &attribution_reporting::EventTriggerData::priority,
            cfg.priority),
      Field("dedup_key", &attribution_reporting::EventTriggerData::dedup_key,
            cfg.dedup_key),
      Field("filters", &attribution_reporting::EventTriggerData::filters,
            cfg.filters));
}

TriggerRegistrationMatcherConfig::TriggerRegistrationMatcherConfig(
    ::testing::Matcher<const FilterPair&> filters,
    ::testing::Matcher<absl::optional<uint64_t>> debug_key,
    ::testing::Matcher<const attribution_reporting::EventTriggerDataList&>
        event_triggers,
    ::testing::Matcher<const attribution_reporting::AggregatableDedupKeyList&>
        aggregatable_dedup_keys,
    ::testing::Matcher<bool> debug_reporting,
    ::testing::Matcher<
        const attribution_reporting::AggregatableTriggerDataList&>
        aggregatable_trigger_data,
    ::testing::Matcher<const attribution_reporting::AggregatableValues&>
        aggregatable_values,
    ::testing::Matcher<::aggregation_service::mojom::AggregationCoordinator>
        aggregation_coordinator)
    : filters(std::move(filters)),
      debug_key(std::move(debug_key)),
      event_triggers(std::move(event_triggers)),
      aggregatable_dedup_keys(std::move(aggregatable_dedup_keys)),
      debug_reporting(std::move(debug_reporting)),
      aggregatable_trigger_data(std::move(aggregatable_trigger_data)),
      aggregatable_values(std::move(aggregatable_values)),
      aggregation_coordinator(std::move(aggregation_coordinator)) {}

TriggerRegistrationMatcherConfig::~TriggerRegistrationMatcherConfig() = default;

::testing::Matcher<const attribution_reporting::TriggerRegistration&>
TriggerRegistrationMatches(const TriggerRegistrationMatcherConfig& cfg) {
  return AllOf(
      Field("filters", &attribution_reporting::TriggerRegistration::filters,
            cfg.filters),
      Field("debug_key", &attribution_reporting::TriggerRegistration::debug_key,
            cfg.debug_key),
      Field("event_triggers",
            &attribution_reporting::TriggerRegistration::event_triggers,
            cfg.event_triggers),
      Field(
          "aggregatable_dedup_keys",
          &attribution_reporting::TriggerRegistration::aggregatable_dedup_keys,
          cfg.aggregatable_dedup_keys),
      Field("debug_reporting",
            &attribution_reporting::TriggerRegistration::debug_reporting,
            cfg.debug_reporting),
      Field("aggregatable_trigger_data",
            &attribution_reporting::TriggerRegistration::
                aggregatable_trigger_data,
            cfg.aggregatable_trigger_data),
      Field("aggregatable_values",
            &attribution_reporting::TriggerRegistration::aggregatable_values,
            cfg.aggregatable_values),
      Field(
          "aggregation_coordinator",
          &attribution_reporting::TriggerRegistration::aggregation_coordinator,
          cfg.aggregation_coordinator));
}

AttributionTriggerMatcherConfig::AttributionTriggerMatcherConfig(
    ::testing::Matcher<const SuitableOrigin&> reporting_origin,
    ::testing::Matcher<const attribution_reporting::TriggerRegistration&>
        registration,
    ::testing::Matcher<const SuitableOrigin&> destination_origin,
    ::testing::Matcher<bool> is_within_fenced_frame)
    : reporting_origin(std::move(reporting_origin)),
      registration(std::move(registration)),
      destination_origin(std::move(destination_origin)),
      is_within_fenced_frame(std::move(is_within_fenced_frame)) {}

AttributionTriggerMatcherConfig::~AttributionTriggerMatcherConfig() = default;

::testing::Matcher<AttributionTrigger> AttributionTriggerMatches(
    const AttributionTriggerMatcherConfig& cfg) {
  return AllOf(
      Property("reporting_origin", &AttributionTrigger::reporting_origin,
               cfg.reporting_origin),
      Property("registration", &AttributionTrigger::registration,
               cfg.registration),
      Property("destination_origin", &AttributionTrigger::destination_origin,
               cfg.destination_origin),
      Property("is_within_fenced_frame",
               &AttributionTrigger::is_within_fenced_frame,
               cfg.is_within_fenced_frame),
      Property("trigger_attestation", &AttributionTrigger::attestation,
               cfg.attestation));
}

std::vector<AttributionReport> GetAttributionReportsForTesting(
    AttributionManager* manager) {
  base::RunLoop run_loop;
  std::vector<AttributionReport> attribution_reports;
  manager->GetPendingReportsForInternalUse(
      AttributionReport::Types{
          AttributionReport::Type::kEventLevel,
          AttributionReport::Type::kAggregatableAttribution},
      /*limit=*/-1,
      base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
        attribution_reports = std::move(reports);
        run_loop.Quit();
      }));
  run_loop.Run();
  return attribution_reports;
}

std::unique_ptr<MockDataHost> GetRegisteredDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  return std::make_unique<MockDataHost>(std::move(data_host));
}

TestAggregatableSourceProvider::TestAggregatableSourceProvider(size_t size) {
  attribution_reporting::AggregationKeys::Keys::container_type keys;
  keys.reserve(size);
  for (size_t i = 0; i < size; ++i) {
    keys.emplace_back(base::NumberToString(i), i);
  }

  auto source =
      attribution_reporting::AggregationKeys::FromKeys(std::move(keys));
  DCHECK(source.has_value());
  source_ = std::move(*source);
}

TestAggregatableSourceProvider::~TestAggregatableSourceProvider() = default;

SourceBuilder TestAggregatableSourceProvider::GetBuilder(
    base::Time source_time) const {
  return SourceBuilder(source_time).SetAggregationKeys(source_);
}

TriggerBuilder DefaultAggregatableTriggerBuilder(
    const std::vector<uint32_t>& histogram_values) {
  std::vector<attribution_reporting::AggregatableTriggerData>
      aggregatable_trigger_data;

  attribution_reporting::AggregatableValues::Values aggregatable_values;

  for (size_t i = 0; i < histogram_values.size(); ++i) {
    std::string key_id = base::NumberToString(i);
    aggregatable_trigger_data.push_back(
        *attribution_reporting::AggregatableTriggerData::Create(
            absl::MakeUint128(/*high=*/i, /*low=*/0),
            /*source_keys=*/{key_id}, FilterPair()));
    aggregatable_values.emplace(std::move(key_id), histogram_values[i]);
  }

  return TriggerBuilder()
      .SetAggregatableTriggerData(std::move(aggregatable_trigger_data))
      .SetAggregatableValues(*attribution_reporting::AggregatableValues::Create(
          std::move(aggregatable_values)));
}

std::vector<AggregatableHistogramContribution>
DefaultAggregatableHistogramContributions(
    const std::vector<uint32_t>& histogram_values) {
  std::vector<AggregatableHistogramContribution> contributions;
  for (size_t i = 0; i < histogram_values.size(); ++i) {
    contributions.emplace_back(absl::MakeUint128(i, i), histogram_values[i]);
  }
  return contributions;
}

}  // namespace content
