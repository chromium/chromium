// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_test_utils.h"

#include <limits.h>

#include <algorithm>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/check_op.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner_util.h"
#include "base/test/bind.h"
#include "content/browser/attribution_reporting/attribution_aggregatable_key.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_observer.h"
#include "content/browser/attribution_reporting/rate_limit_result.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/abseil-cpp/absl/numeric/int128.h"
#include "url/gurl.h"

namespace content {

namespace {

using ::testing::AllOf;
using ::testing::Field;
using ::testing::Property;

const char kDefaultImpressionOrigin[] = "https://impression.test/";
const char kDefaultTriggerOrigin[] = "https://sub.conversion.test/";
const char kDefaultReportOrigin[] = "https://report.test/";

// Default expiry time for impressions for testing.
const int64_t kExpiryTime = 30;

}  // namespace

MockAttributionReportingContentBrowserClient::
    MockAttributionReportingContentBrowserClient() = default;

MockAttributionReportingContentBrowserClient::
    ~MockAttributionReportingContentBrowserClient() = default;

MockAttributionHost::MockAttributionHost(WebContents* web_contents)
    : AttributionHost(web_contents) {
  SetReceiverImplForTesting(this);
}

MockAttributionHost::~MockAttributionHost() {
  SetReceiverImplForTesting(nullptr);
}

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
    blink::mojom::AttributionSourceDataPtr data) {
  source_data_.push_back(std::move(data));
  if (source_data_.size() < min_source_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

void MockDataHost::TriggerDataAvailable(
    blink::mojom::AttributionTriggerDataPtr data) {
  trigger_data_.push_back(std::move(data));
  if (trigger_data_.size() < min_trigger_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

MockDataHostManager::MockDataHostManager() = default;

MockDataHostManager::~MockDataHostManager() = default;

MockAttributionObserver::MockAttributionObserver() = default;

MockAttributionObserver::~MockAttributionObserver() = default;

base::GUID DefaultExternalReportID() {
  return base::GUID::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
}

std::vector<base::GUID> DefaultExternalReportIDs(size_t size) {
  return std::vector<base::GUID>(size, DefaultExternalReportID());
}

ConfigurableStorageDelegate::ConfigurableStorageDelegate() = default;

ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

void ConfigurableStorageDelegate::DetachFromSequence() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

base::Time ConfigurableStorageDelegate::GetEventLevelReportTime(
    const CommonSourceInfo& source,
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return source.impression_time() + report_delay_;
}

base::Time ConfigurableStorageDelegate::GetAggregatableReportTime(
    base::Time trigger_time) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return trigger_time + report_delay_;
}

int ConfigurableStorageDelegate::GetMaxAttributionsPerSource(
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_attributions_per_source_;
}

int ConfigurableStorageDelegate::GetMaxSourcesPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_sources_per_origin_;
}

int ConfigurableStorageDelegate::GetMaxAttributionsPerOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_attributions_per_origin_;
}

int ConfigurableStorageDelegate::
    GetMaxDestinationsPerSourceSiteReportingOrigin() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return max_destinations_per_source_site_reporting_origin_;
}

AttributionStorageDelegate::RateLimitConfig
ConfigurableStorageDelegate::GetRateLimits() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return rate_limits_;
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
  if (reverse_reports_on_shuffle_)
    base::ranges::reverse(reports);
}

double ConfigurableStorageDelegate::GetRandomizedResponseRate(
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case AttributionSourceType::kNavigation:
      return randomized_response_rates_.navigation;
    case AttributionSourceType::kEvent:
      return randomized_response_rates_.event;
  }
}

AttributionStorageDelegate::RandomizedResponse
ConfigurableStorageDelegate::GetRandomizedResponse(
    const CommonSourceInfo& source) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return randomized_response_;
}

int64_t ConfigurableStorageDelegate::GetAggregatableBudgetPerSource() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return aggregatable_budget_per_source_;
}

uint64_t ConfigurableStorageDelegate::SanitizeTriggerData(
    uint64_t trigger_data,
    AttributionSourceType source_type) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (source_type) {
    case AttributionSourceType::kNavigation:
      if (!navigation_trigger_data_cardinality_)
        return trigger_data;

      return trigger_data % *navigation_trigger_data_cardinality_;
    case AttributionSourceType::kEvent:
      if (!event_trigger_data_cardinality_)
        return trigger_data;

      return trigger_data % *event_trigger_data_cardinality_;
  }
}

void ConfigurableStorageDelegate::set_max_attributions_per_source(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_attributions_per_source_ = max;
}

void ConfigurableStorageDelegate::set_max_sources_per_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_sources_per_origin_ = max;
}

void ConfigurableStorageDelegate::set_max_attributions_per_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_attributions_per_origin_ = max;
}

void ConfigurableStorageDelegate::
    set_max_destinations_per_source_site_reporting_origin(int max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  max_destinations_per_source_site_reporting_origin_ = max;
}

void ConfigurableStorageDelegate::set_aggregatable_budget_per_source(
    int64_t max) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  aggregatable_budget_per_source_ = max;
}

AttributionStorageDelegate::RateLimitConfig&
ConfigurableStorageDelegate::rate_limits() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return rate_limits_;
}

void ConfigurableStorageDelegate::set_rate_limits(RateLimitConfig c) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  rate_limits_ = c;
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
    AttributionRandomizedResponseRates rates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  randomized_response_rates_ = rates;
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

  navigation_trigger_data_cardinality_ = navigation;
  event_trigger_data_cardinality_ = event;
}

AttributionManager* TestManagerProvider::GetManager(
    WebContents* web_contents) const {
  return manager_;
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
  for (auto& observer : observers_)
    observer.OnSourcesChanged();
}

void MockAttributionManager::NotifyReportsChanged(
    AttributionReport::ReportType report_type) {
  for (auto& observer : observers_)
    observer.OnReportsChanged(report_type);
}

void MockAttributionManager::NotifySourceDeactivated(
    const DeactivatedSource& source) {
  for (auto& observer : observers_)
    observer.OnSourceDeactivated(source);
}

void MockAttributionManager::NotifySourceHandled(
    const StorableSource& source,
    StorableSource::Result result) {
  for (auto& observer : observers_)
    observer.OnSourceHandled(source, result);
}

void MockAttributionManager::NotifyReportSent(const AttributionReport& report,
                                              bool is_debug_report,
                                              const SendResult& info) {
  for (auto& observer : observers_)
    observer.OnReportSent(report, is_debug_report, info);
}

void MockAttributionManager::NotifyTriggerHandled(
    const CreateReportResult& result) {
  for (auto& observer : observers_)
    observer.OnTriggerHandled(result);
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
    if (waiting_for_null_impression_)
      impression_loop_.Quit();
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
  if (num_impressions_ >= expected_num_impressions_)
    return *last_impression_;
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
    : impression_time_(time),
      expiry_(base::Milliseconds(kExpiryTime)),
      impression_origin_(url::Origin::Create(GURL(kDefaultImpressionOrigin))),
      conversion_origin_(url::Origin::Create(GURL(kDefaultTriggerOrigin))),
      reporting_origin_(url::Origin::Create(GURL(kDefaultReportOrigin))) {}

SourceBuilder::~SourceBuilder() = default;

SourceBuilder::SourceBuilder(const SourceBuilder&) = default;

SourceBuilder::SourceBuilder(SourceBuilder&&) = default;

SourceBuilder& SourceBuilder::operator=(const SourceBuilder&) = default;

SourceBuilder& SourceBuilder::operator=(SourceBuilder&&) = default;

SourceBuilder& SourceBuilder::SetExpiry(base::TimeDelta delta) {
  expiry_ = delta;
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceEventId(uint64_t source_event_id) {
  source_event_id_ = source_event_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetImpressionOrigin(url::Origin origin) {
  impression_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetConversionOrigin(url::Origin origin) {
  conversion_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetReportingOrigin(url::Origin origin) {
  reporting_origin_ = std::move(origin);
  return *this;
}

SourceBuilder& SourceBuilder::SetSourceType(AttributionSourceType source_type) {
  source_type_ = source_type;
  return *this;
}

SourceBuilder& SourceBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

SourceBuilder& SourceBuilder::SetFilterData(AttributionFilterData filter_data) {
  filter_data_ = std::move(filter_data);
  return *this;
}

SourceBuilder& SourceBuilder::SetDefaultFilterData() {
  filter_data_ = AttributionFilterData::CreateForTesting(
      {{"source_type", {AttributionSourceTypeToString(source_type_)}}});
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

SourceBuilder& SourceBuilder::SetAggregatableSource(
    AttributionAggregatableSource aggregatable_source) {
  aggregatable_source_ = std::move(aggregatable_source);
  return *this;
}

CommonSourceInfo SourceBuilder::BuildCommonInfo() const {
  return CommonSourceInfo(
      source_event_id_, impression_origin_, conversion_origin_,
      reporting_origin_, impression_time_,
      /*expiry_time=*/impression_time_ + expiry_, source_type_, priority_,
      filter_data_, debug_key_, aggregatable_source_);
}

StorableSource SourceBuilder::Build() const {
  return StorableSource(BuildCommonInfo());
}

StoredSource SourceBuilder::BuildStored() const {
  StoredSource source(BuildCommonInfo(), attribution_logic_, active_state_,
                      source_id_);
  source.SetDedupKeys(dedup_keys_);
  return source;
}

AttributionTrigger DefaultTrigger() {
  return TriggerBuilder().Build();
}

TriggerBuilder::TriggerBuilder()
    : destination_origin_(url::Origin::Create(GURL(kDefaultTriggerOrigin))),
      reporting_origin_(url::Origin::Create(GURL(kDefaultReportOrigin))) {}

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

TriggerBuilder& TriggerBuilder::SetDestinationOrigin(
    url::Origin destination_origin) {
  destination_origin_ = std::move(destination_origin);
  return *this;
}

TriggerBuilder& TriggerBuilder::SetReportingOrigin(
    url::Origin reporting_origin) {
  reporting_origin_ = std::move(reporting_origin);
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

TriggerBuilder& TriggerBuilder::SetAggregatableTrigger(
    AttributionAggregatableTrigger aggregatable_trigger) {
  aggregatable_trigger_ = std::move(aggregatable_trigger);
  return *this;
}

AttributionTrigger TriggerBuilder::Build() const {
  return AttributionTrigger(trigger_data_, destination_origin_,
                            reporting_origin_, event_source_trigger_data_,
                            priority_, dedup_key_, debug_key_,
                            aggregatable_trigger_);
}

AttributionInfoBuilder::AttributionInfoBuilder(StoredSource source)
    : source_(std::move(source)) {}

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
  return AttributionInfo(source_, time_, debug_key_);
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
    absl::optional<AttributionReport::EventLevelData::Id> id) {
  report_id_ = id;
  return *this;
}

ReportBuilder& ReportBuilder::SetReportId(
    absl::optional<AttributionReport::AggregatableAttributionData::Id> id) {
  aggregatable_attribution_report_id_ = id;
  return *this;
}

ReportBuilder& ReportBuilder::SetAggregatableHistogramContributions(
    std::vector<AggregatableHistogramContribution> contributions) {
  DCHECK(!contributions.empty());
  contributions_ = std::move(contributions);
  return *this;
}

AttributionReport ReportBuilder::Build() const {
  return AttributionReport(
      attribution_info_, report_time_, external_report_id_,
      AttributionReport::EventLevelData(trigger_data_, priority_,
                                        randomized_trigger_rate_, report_id_));
}

AttributionReport ReportBuilder::BuildAggregatableAttribution() const {
  return AttributionReport(
      attribution_info_, report_time_, external_report_id_,
      AttributionReport::AggregatableAttributionData(
          contributions_, aggregatable_attribution_report_id_));
}

AggregatableKeyProtoBuilder::AggregatableKeyProtoBuilder() = default;

AggregatableKeyProtoBuilder::~AggregatableKeyProtoBuilder() = default;

AggregatableKeyProtoBuilder& AggregatableKeyProtoBuilder::SetHighBits(
    uint64_t high_bits) {
  key_.set_high_bits(high_bits);
  return *this;
}

AggregatableKeyProtoBuilder& AggregatableKeyProtoBuilder::SetLowBits(
    uint64_t low_bits) {
  key_.set_low_bits(low_bits);
  return *this;
}

proto::AttributionAggregatableKey AggregatableKeyProtoBuilder::Build() const {
  return key_;
}

AggregatableSourceProtoBuilder::AggregatableSourceProtoBuilder() = default;

AggregatableSourceProtoBuilder::~AggregatableSourceProtoBuilder() = default;

AggregatableSourceProtoBuilder& AggregatableSourceProtoBuilder::AddKey(
    std::string key_id,
    proto::AttributionAggregatableKey key) {
  (*aggregatable_source_.mutable_keys())[std::move(key_id)] = std::move(key);
  return *this;
}

proto::AttributionAggregatableSource AggregatableSourceProtoBuilder::Build()
    const {
  return aggregatable_source_;
}

AggregatableSourceMojoBuilder::AggregatableSourceMojoBuilder() = default;

AggregatableSourceMojoBuilder::~AggregatableSourceMojoBuilder() = default;

AggregatableSourceMojoBuilder& AggregatableSourceMojoBuilder::AddKey(
    std::string key_id,
    blink::mojom::AttributionAggregatableKeyPtr key) {
  aggregatable_source_.keys.emplace(std::move(key_id), std::move(key));
  return *this;
}

blink::mojom::AttributionAggregatableSourcePtr
AggregatableSourceMojoBuilder::Build() const {
  return aggregatable_source_.Clone();
}

bool operator==(const AttributionTrigger::EventTriggerData& a,
                const AttributionTrigger::EventTriggerData& b) {
  const auto tie = [](const AttributionTrigger::EventTriggerData& t) {
    return std::make_tuple(t.data, t.priority, t.dedup_key, t.filters,
                           t.not_filters);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b) {
  const auto tie = [](const AttributionTrigger& t) {
    return std::make_tuple(t.destination_origin(), t.reporting_origin(),
                           t.debug_key(), t.event_triggers(),
                           t.aggregatable_trigger());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionFilterData& a,
                const AttributionFilterData& b) {
  return a.filter_values() == b.filter_values();
}

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b) {
  const auto tie = [](const CommonSourceInfo& source) {
    return std::make_tuple(source.source_event_id(), source.impression_origin(),
                           source.conversion_origin(),
                           source.reporting_origin(), source.impression_time(),
                           source.expiry_time(), source.source_type(),
                           source.priority(), source.filter_data(),
                           source.debug_key(), source.aggregatable_source());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionInfo& a, const AttributionInfo& b) {
  const auto tie = [](const AttributionInfo& attribution_info) {
    return std::make_tuple(attribution_info.source, attribution_info.time,
                           attribution_info.debug_key);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionStorageDelegate::FakeReport& a,
                const AttributionStorageDelegate::FakeReport& b) {
  const auto tie = [](const AttributionStorageDelegate::FakeReport& r) {
    return std::make_tuple(r.trigger_data, r.report_time);
  };
  return tie(a) == tie(b);
}

bool operator<(const AttributionStorageDelegate::FakeReport& a,
               const AttributionStorageDelegate::FakeReport& b) {
  const auto tie = [](const AttributionStorageDelegate::FakeReport& r) {
    return std::make_tuple(r.trigger_data, r.report_time);
  };
  return tie(a) < tie(b);
}

bool operator==(const StorableSource& a, const StorableSource& b) {
  const auto tie = [](const StorableSource& source) {
    return std::make_tuple(source.common_info());
  };
  return tie(a) == tie(b);
}

// Does not compare source IDs, as they are set by the underlying sqlite DB and
// should not be tested.
bool operator==(const StoredSource& a, const StoredSource& b) {
  const auto tie = [](const StoredSource& source) {
    return std::make_tuple(source.common_info(), source.attribution_logic(),
                           source.active_state(), source.dedup_keys());
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
  return a.contributions == b.contributions;
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
    return std::make_tuple(info.status, info.http_response_code);
  };
  return tie(a) == tie(b);
}

bool operator==(const DeactivatedSource& a, const DeactivatedSource& b) {
  const auto tie = [](const DeactivatedSource& deactivated_source) {
    return std::make_tuple(deactivated_source.source,
                           deactivated_source.reason);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionAggregatableKey& a,
                const AttributionAggregatableKey& b) {
  const auto tie = [](const AttributionAggregatableKey& key) {
    return std::make_tuple(key.high_bits, key.low_bits);
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionAggregatableTriggerData& a,
                const AttributionAggregatableTriggerData& b) {
  const auto tie = [](const AttributionAggregatableTriggerData& trigger_data) {
    return std::make_tuple(trigger_data.key(), trigger_data.source_keys(),
                           trigger_data.filters(), trigger_data.not_filters());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionAggregatableTrigger& a,
                const AttributionAggregatableTrigger& b) {
  const auto tie =
      [](const AttributionAggregatableTrigger& aggregatable_trigger) {
        return std::make_tuple(aggregatable_trigger.trigger_data(),
                               aggregatable_trigger.values());
      };
  return tie(a) == tie(b);
}

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::EventLevelResult status) {
  switch (status) {
    case AttributionTrigger::EventLevelResult::kSuccess:
      out << "success";
      break;
    case AttributionTrigger::EventLevelResult::kSuccessDroppedLowerPriority:
      out << "successDroppedLowerPriority";
      break;
    case AttributionTrigger::EventLevelResult::kInternalError:
      out << "internalError";
      break;
    case AttributionTrigger::EventLevelResult::
        kNoCapacityForConversionDestination:
      out << "insufficientDestinationCapacity";
      break;
    case AttributionTrigger::EventLevelResult::kNoMatchingImpressions:
      out << "noMatchingSources";
      break;
    case AttributionTrigger::EventLevelResult::kDeduplicated:
      out << "deduplicated";
      break;
    case AttributionTrigger::EventLevelResult::kExcessiveAttributions:
      out << "excessiveAttributions";
      break;
    case AttributionTrigger::EventLevelResult::kPriorityTooLow:
      out << "priorityTooLow";
      break;
    case AttributionTrigger::EventLevelResult::kDroppedForNoise:
      out << "noised";
      break;
    case AttributionTrigger::EventLevelResult::kExcessiveReportingOrigins:
      out << "excessiveReportingOrigins";
      break;
    case AttributionTrigger::EventLevelResult::kNoMatchingSourceFilterData:
      out << "noMatchingSourceFilterData";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         AttributionTrigger::AggregatableResult status) {
  switch (status) {
    case AttributionTrigger::AggregatableResult::kSuccess:
      out << "success";
      break;
    case AttributionTrigger::AggregatableResult::kInternalError:
      out << "internalError";
      break;
    case AttributionTrigger::AggregatableResult::
        kNoCapacityForConversionDestination:
      out << "insufficientDestinationCapacity";
      break;
    case AttributionTrigger::AggregatableResult::kNoMatchingImpressions:
      out << "noMatchingSources";
      break;
    case AttributionTrigger::AggregatableResult::kExcessiveAttributions:
      out << "excessiveAttributions";
      break;
    case AttributionTrigger::AggregatableResult::kExcessiveReportingOrigins:
      out << "excessiveReportingOrigins";
      break;
    case AttributionTrigger::AggregatableResult::kNoHistograms:
      out << "noHistograms";
      break;
    case AttributionTrigger::AggregatableResult::kInsufficientBudget:
      out << "insufficientBudget";
      break;
    case AttributionTrigger::AggregatableResult::kNoMatchingSourceFilterData:
      out << "noMatchingSourceFilterData";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, DeactivatedSource::Reason reason) {
  switch (reason) {
    case DeactivatedSource::Reason::kReplacedByNewerSource:
      out << "kReplacedByNewerSource";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, RateLimitResult result) {
  switch (result) {
    case RateLimitResult::kAllowed:
      out << "kAllowed";
      break;
    case RateLimitResult::kNotAllowed:
      out << "kNotAllowed";
      break;
    case RateLimitResult::kError:
      out << "kError";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, AttributionSourceType source_type) {
  return out << AttributionSourceTypeToString(source_type);
}

std::ostream& operator<<(std::ostream& out,
                         StoredSource::AttributionLogic attribution_logic) {
  switch (attribution_logic) {
    case StoredSource::AttributionLogic::kNever:
      out << "kNever";
      break;
    case StoredSource::AttributionLogic::kTruthfully:
      out << "kTruthfully";
      break;
    case StoredSource::AttributionLogic::kFalsely:
      out << "kFalsely";
      break;
  }
  return out;
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionTrigger::EventTriggerData& event_trigger) {
  return out << "{data=" << event_trigger.data
             << ",priority=" << event_trigger.priority << ",dedup_key="
             << (event_trigger.dedup_key
                     ? base::NumberToString(*event_trigger.dedup_key)
                     : "null")
             << ",filters=" << event_trigger.filters
             << ",not_filters=" << event_trigger.not_filters << "}";
}

std::ostream& operator<<(std::ostream& out,
                         StoredSource::ActiveState active_state) {
  switch (active_state) {
    case StoredSource::ActiveState::kActive:
      out << "kActive";
      break;
    case StoredSource::ActiveState::kInactive:
      out << "kInactive";
      break;
    case StoredSource::ActiveState::kReachedEventLevelAttributionLimit:
      out << "kReachedEventLevelAttributionLimit";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion) {
  out << "{destination_origin=" << conversion.destination_origin()
      << ",reporting_origin=" << conversion.reporting_origin() << ",debug_key="
      << (conversion.debug_key() ? base::NumberToString(*conversion.debug_key())
                                 : "null")
      << "event_triggers=[";

  const char* separator = "";
  for (const auto& event_trigger : conversion.event_triggers()) {
    out << separator << event_trigger;
    separator = ", ";
  }

  return out << "]}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionFilterData& filter_data) {
  out << "{";

  const char* outer_separator = "";
  for (const auto& [filter, values] : filter_data.filter_values()) {
    out << outer_separator << filter << "=[";

    const char* inner_separator = "";
    for (const auto& value : values) {
      out << inner_separator << value;
      inner_separator = ", ";
    }

    out << "]";
    outer_separator = ", ";
  }

  return out << "}";
}

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source) {
  return out << "{source_event_id=" << source.source_event_id()
             << ",impression_origin=" << source.impression_origin()
             << ",conversion_origin=" << source.conversion_origin()
             << ",reporting_origin=" << source.reporting_origin()
             << ",impression_time=" << source.impression_time()
             << ",expiry_time=" << source.expiry_time()
             << ",source_type=" << source.source_type()
             << ",priority=" << source.priority()
             << ",filter_data=" << source.filter_data() << ",debug_key="
             << (source.debug_key() ? base::NumberToString(*source.debug_key())
                                    : "null")
             << ",aggregatable_source=" << source.aggregatable_source() << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionInfo& attribution_info) {
  return out << "{source=" << attribution_info.source
             << ",time=" << attribution_info.time << ",debug_key="
             << (attribution_info.debug_key
                     ? base::NumberToString(*attribution_info.debug_key)
                     : "null")
             << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionStorageDelegate::FakeReport& r) {
  return out << "{trigger_data=" << r.trigger_data
             << ",report_time=" << r.report_time << "}";
}

std::ostream& operator<<(std::ostream& out, const StorableSource& source) {
  return out << "{common_info=" << source.common_info() << "}";
}

std::ostream& operator<<(std::ostream& out, const StoredSource& source) {
  out << "{common_info=" << source.common_info()
      << ",attribution_logic=" << source.attribution_logic()
      << ",active_state=" << source.active_state()
      << ",source_id=" << *source.source_id() << ",dedup_keys=[";

  const char* separator = "";
  for (int64_t dedup_key : source.dedup_keys()) {
    out << separator << dedup_key;
    separator = ", ";
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
             << ",id=" << (data.id ? base::NumberToString(**data.id) : "null")
             << "}";
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

  return out << "],id=" << (data.id ? base::NumberToString(**data.id) : "null")
             << "}";
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
  out << "{attribution_info=" << report.attribution_info()
      << ",report_time=" << report.report_time()
      << ",external_report_id=" << report.external_report_id()
      << ",failed_send_attempts=" << report.failed_send_attempts()
      << ",data=" << report.data() << "}";
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         AttributionReport::ReportType report_type) {
  switch (report_type) {
    case AttributionReport::ReportType::kEventLevel:
      out << "kEventLevel";
      break;
    case AttributionReport::ReportType::kAggregatableAttribution:
      out << "kAggregatableAttribution";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, SendResult::Status status) {
  switch (status) {
    case SendResult::Status::kSent:
      out << "kSent";
      break;
    case SendResult::Status::kTransientFailure:
      out << "kTransientFailure";
      break;
    case SendResult::Status::kFailure:
      out << "kFailure";
      break;
    case SendResult::Status::kDropped:
      out << "kDropped";
      break;
    case SendResult::Status::kFailedToAssemble:
      out << "kFailedToAssemble";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const SendResult& info) {
  return out << "{status=" << info.status
             << ",http_response_code=" << info.http_response_code << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const DeactivatedSource& deactivated_source) {
  return out << "{source=" << deactivated_source.source
             << ",reason=" << deactivated_source.reason << "}";
}

std::ostream& operator<<(std::ostream& out, StorableSource::Result status) {
  switch (status) {
    case StorableSource::Result::kSuccess:
      return out << "success";
    case StorableSource::Result::kInternalError:
      return out << "internalError";
    case StorableSource::Result::kInsufficientSourceCapacity:
      return out << "insufficientSourceCapacity";
    case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      return out << "insufficientUniqueDestinationCapacity";
    case StorableSource::Result::kExcessiveReportingOrigins:
      return out << "excessiveReportingOrigins";
  }
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionAggregatableKey& key) {
  return out << "{high_bits=" << key.high_bits << ",low_bits=" << key.low_bits
             << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableTriggerData& trigger_data) {
  out << "{key=" << trigger_data.key() << ",source_keys=[";

  const char* separator = "";
  for (const auto& key : trigger_data.source_keys()) {
    out << separator << key;
    separator = ", ";
  }

  return out << "],filters=" << trigger_data.filters()
             << ",not_filters=" << trigger_data.not_filters() << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableTrigger& aggregatable_trigger) {
  out << "{trigger_data=[";

  const char* separator = "";
  for (const auto& trigger_data : aggregatable_trigger.trigger_data()) {
    out << separator << trigger_data;
    separator = ", ";
  }

  out << "],values=[";

  separator = "";
  for (const auto& [key, value] : aggregatable_trigger.values()) {
    out << separator << key << ":" << value;
    separator = ", ";
  }

  return out << "]}";
}

AttributionFilterSizeTestCase::Map AttributionFilterSizeTestCase::AsMap()
    const {
  Map map;

  for (size_t i = 0; i < filter_count; i++) {
    // Give each filter a unique name while respecting the desired size.
    std::string filter(filter_size, 'A' + i);
    std::vector<std::string> values(value_count, std::string(value_size, '*'));
    map.emplace(std::move(filter), std::move(values));
  }

  DCHECK_EQ(map.size(), filter_count);
  return map;
}

namespace proto {

bool operator==(const AttributionAggregatableKey& a,
                const AttributionAggregatableKey& b) {
  auto tie = [](const AttributionAggregatableKey& key) {
    return std::make_tuple(key.has_high_bits(), key.high_bits(),
                           key.has_low_bits(), key.low_bits());
  };
  return tie(a) == tie(b);
}

bool operator==(const AttributionAggregatableSource& a,
                const AttributionAggregatableSource& b) {
  if (a.keys().size() != b.keys().size())
    return false;

  return base::ranges::all_of(a.keys(), [&](const auto& key) {
    auto iter = b.keys().find(key.first);
    return iter != b.keys().end() && iter->second == key.second;
  });
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionAggregatableKey& key) {
  return out << "{high_bits="
             << (key.has_high_bits() ? base::NumberToString(key.high_bits())
                                     : "null")
             << ",low_bits="
             << (key.has_low_bits() ? base::NumberToString(key.low_bits())
                                    : "null")
             << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableSource& aggregatable_source) {
  out << "{keys=[";

  const char* separator = "";
  for (const auto& [key_id, key] : aggregatable_source.keys()) {
    out << separator << key_id << ":" << key;
    separator = ", ";
  }
  return out << "]}";
}

}  // namespace proto

bool operator==(const AttributionAggregatableSource& a,
                const AttributionAggregatableSource& b) {
  return a.proto() == b.proto();
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionAggregatableSource& aggregatable_source) {
  return out << "{proto=" << aggregatable_source.proto() << "}";
}

EventTriggerDataMatcherConfig::~EventTriggerDataMatcherConfig() = default;

::testing::Matcher<const AttributionTrigger::EventTriggerData&>
EventTriggerDataMatches(const EventTriggerDataMatcherConfig& cfg) {
  return AllOf(
      Field("data", &AttributionTrigger::EventTriggerData::data, cfg.data),
      Field("priority", &AttributionTrigger::EventTriggerData::priority,
            cfg.priority),
      Field("dedup_key", &AttributionTrigger::EventTriggerData::dedup_key,
            cfg.dedup_key),
      Field("filters", &AttributionTrigger::EventTriggerData::filters,
            cfg.filters),
      Field("not_filters", &AttributionTrigger::EventTriggerData::not_filters,
            cfg.not_filters));
}

AttributionTriggerMatcherConfig::~AttributionTriggerMatcherConfig() = default;

::testing::Matcher<AttributionTrigger> AttributionTriggerMatches(
    const AttributionTriggerMatcherConfig& cfg) {
  return AllOf(
      Property("destination_origin", &AttributionTrigger::destination_origin,
               cfg.destination_origin),
      Property("reporting_origin", &AttributionTrigger::reporting_origin,
               cfg.reporting_origin),
      Property("filters", &AttributionTrigger::filters, cfg.filters),
      Property("debug_key", &AttributionTrigger::debug_key, cfg.debug_key),
      Property("event_triggers", &AttributionTrigger::event_triggers,
               cfg.event_triggers));
}

std::vector<AttributionReport> GetAttributionReportsForTesting(
    AttributionManagerImpl* manager,
    base::Time max_report_time) {
  base::RunLoop run_loop;
  std::vector<AttributionReport> attribution_reports;
  manager->attribution_storage_
      .AsyncCall(&AttributionStorage::GetAttributionReports)
      .WithArgs(max_report_time, /*limit=*/-1,
                AttributionReport::ReportTypes{
                    AttributionReport::ReportType::kEventLevel,
                    AttributionReport::ReportType::kAggregatableAttribution})
      .Then(base::BindOnce(base::BindLambdaForTesting(
          [&](std::vector<AttributionReport> reports) {
            attribution_reports = std::move(reports);
            run_loop.Quit();
          })));
  run_loop.Run();
  return attribution_reports;
}

std::unique_ptr<MockDataHost> GetRegisteredDataHost(
    mojo::PendingReceiver<blink::mojom::AttributionDataHost> data_host) {
  return std::make_unique<MockDataHost>(std::move(data_host));
}

TestAggregatableSourceProvider::TestAggregatableSourceProvider(size_t size) {
  AggregatableSourceProtoBuilder source_builder;
  for (size_t i = 0; i < size; ++i) {
    source_builder.AddKey(
        base::NumberToString(i),
        AggregatableKeyProtoBuilder().SetHighBits(0).SetLowBits(i).Build());
  }

  auto source = AttributionAggregatableSource::Create(source_builder.Build());
  DCHECK(source.has_value());
  source_ = std::move(*source);
}

TestAggregatableSourceProvider::~TestAggregatableSourceProvider() = default;

SourceBuilder TestAggregatableSourceProvider::GetBuilder(
    base::Time source_time) const {
  return SourceBuilder(source_time).SetAggregatableSource(source_);
}

TriggerBuilder DefaultAggregatableTriggerBuilder(
    const std::vector<uint32_t>& histogram_values) {
  auto trigger_mojo = blink::mojom::AttributionAggregatableTrigger::New();

  for (size_t i = 0; i < histogram_values.size(); ++i) {
    std::string key_id = base::NumberToString(i);
    trigger_mojo->trigger_data.push_back(
        blink::mojom::AttributionAggregatableTriggerData::New(
            blink::mojom::AttributionAggregatableKey::New(/*high_bits=*/i,
                                                          /*low_bits=*/0),
            std::vector<std::string>{key_id},
            blink::mojom::AttributionFilterData::New(),
            blink::mojom::AttributionFilterData::New()));
    trigger_mojo->values.emplace(std::move(key_id), histogram_values[i]);
  }

  auto trigger =
      AttributionAggregatableTrigger::FromMojo(std::move(trigger_mojo));
  DCHECK(trigger.has_value());

  return TriggerBuilder().SetAggregatableTrigger(*trigger);
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
