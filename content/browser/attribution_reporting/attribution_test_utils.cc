// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/attribution_reporting/attribution_test_utils.h"

#include <limits.h>
#include <algorithm>

#include <tuple>

#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner_util.h"
#include "base/test/bind.h"
#include "url/gurl.h"

namespace content {

namespace {

using DeactivatedSource = ::content::AttributionStorage::DeactivatedSource;

const char kDefaultImpressionOrigin[] = "https://impression.test/";
const char kDefaultTriggerOrigin[] = "https://sub.conversion.test/";
const char kDefaultTriggerDestination[] = "https://conversion.test/";
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

void MockDataHost::SourceDataAvailable(
    blink::mojom::AttributionSourceDataPtr data) {
  source_data_.push_back(std::move(data));
  if (source_data_.size() < min_source_data_count_) {
    return;
  }
  wait_loop_.Quit();
}

MockDataHostManager::MockDataHostManager() = default;

MockDataHostManager::~MockDataHostManager() = default;

base::GUID DefaultExternalReportID() {
  return base::GUID::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
}

ConfigurableStorageDelegate::ConfigurableStorageDelegate() = default;
ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

base::Time ConfigurableStorageDelegate::GetReportTime(
    const CommonSourceInfo& source,
    base::Time trigger_time) const {
  return source.impression_time() + report_delay_;
}

int ConfigurableStorageDelegate::GetMaxAttributionsPerSource(
    CommonSourceInfo::SourceType source_type) const {
  return max_attributions_per_source_;
}

int ConfigurableStorageDelegate::GetMaxSourcesPerOrigin() const {
  return max_sources_per_origin_;
}

int ConfigurableStorageDelegate::GetMaxAttributionsPerOrigin() const {
  return max_attributions_per_origin_;
}

int ConfigurableStorageDelegate::
    GetMaxDestinationsPerSourceSiteReportingOrigin() const {
  return max_destinations_per_source_site_reporting_origin_;
}

AttributionStorageDelegate::RateLimitConfig
ConfigurableStorageDelegate::GetRateLimits() const {
  return rate_limits_;
}

base::TimeDelta ConfigurableStorageDelegate::GetDeleteExpiredSourcesFrequency()
    const {
  return delete_expired_sources_frequency_;
}

base::TimeDelta
ConfigurableStorageDelegate::GetDeleteExpiredRateLimitsFrequency() const {
  return delete_expired_rate_limits_frequency_;
}

base::GUID ConfigurableStorageDelegate::NewReportID() const {
  return DefaultExternalReportID();
}

absl::optional<AttributionStorageDelegate::OfflineReportDelayConfig>
ConfigurableStorageDelegate::GetOfflineReportDelayConfig() const {
  return offline_report_delay_config_;
}

void ConfigurableStorageDelegate::ShuffleReports(
    std::vector<AttributionReport>& reports) const {
  if (reverse_reports_on_shuffle_)
    base::ranges::reverse(reports);
}

AttributionStorageDelegate::RandomizedResponse
ConfigurableStorageDelegate::GetRandomizedResponse(
    const CommonSourceInfo& source) const {
  return randomized_response_;
}

AttributionManager* TestManagerProvider::GetManager(
    WebContents* web_contents) const {
  return manager_;
}

MockAttributionManager::MockAttributionManager() = default;

MockAttributionManager::~MockAttributionManager() = default;

void MockAttributionManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void MockAttributionManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

AttributionDataHostManager* MockAttributionManager::GetDataHostManager() {
  return data_host_manager_.get();
}

void MockAttributionManager::NotifySourcesChanged() {
  for (Observer& observer : observers_)
    observer.OnSourcesChanged();
}

void MockAttributionManager::NotifyReportsChanged() {
  for (Observer& observer : observers_)
    observer.OnReportsChanged();
}

void MockAttributionManager::NotifySourceDeactivated(
    const DeactivatedSource& source) {
  for (Observer& observer : observers_)
    observer.OnSourceDeactivated(source);
}

void MockAttributionManager::NotifySourceHandled(
    const StorableSource& source,
    StorableSource::Result result) {
  for (Observer& observer : observers_)
    observer.OnSourceHandled(source, result);
}

void MockAttributionManager::NotifyReportSent(const AttributionReport& report,
                                              const SendResult& info) {
  for (Observer& observer : observers_)
    observer.OnReportSent(report, info);
}

void MockAttributionManager::NotifyTriggerHandled(
    const AttributionStorage::CreateReportResult& result) {
  for (Observer& observer : observers_)
    observer.OnTriggerHandled(result);
}

void MockAttributionManager::SetDataHostManager(
    std::unique_ptr<AttributionDataHostManager> manager) {
  data_host_manager_ = std::move(manager);
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

SourceBuilder& SourceBuilder::SetSourceType(
    CommonSourceInfo::SourceType source_type) {
  source_type_ = source_type;
  return *this;
}

SourceBuilder& SourceBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
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

SourceBuilder& SourceBuilder::SetSourceId(StoredSource::Id source_id) {
  source_id_ = source_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetDedupKeys(std::vector<uint64_t> dedup_keys) {
  dedup_keys_ = std::move(dedup_keys);
  return *this;
}

CommonSourceInfo SourceBuilder::BuildCommonInfo() const {
  return CommonSourceInfo(source_event_id_, impression_origin_,
                          conversion_origin_, reporting_origin_,
                          impression_time_,
                          /*expiry_time=*/impression_time_ + expiry_,
                          source_type_, priority_, debug_key_);
}

StorableSource SourceBuilder::Build() const {
  return StorableSource(BuildCommonInfo());
}

StoredSource SourceBuilder::BuildStored() const {
  StoredSource source(BuildCommonInfo(), attribution_logic_, source_id_);
  source.SetDedupKeys(dedup_keys_);
  return source;
}

AttributionTrigger DefaultTrigger() {
  return TriggerBuilder().Build();
}

TriggerBuilder::TriggerBuilder()
    : conversion_destination_(
          net::SchemefulSite(GURL(kDefaultTriggerDestination))),
      reporting_origin_(url::Origin::Create(GURL(kDefaultReportOrigin))) {}

TriggerBuilder::~TriggerBuilder() = default;

TriggerBuilder& TriggerBuilder::SetTriggerData(uint64_t trigger_data) {
  trigger_data_ = trigger_data;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetEventSourceTriggerData(
    uint64_t event_source_trigger_data) {
  event_source_trigger_data_ = event_source_trigger_data;
  return *this;
}

TriggerBuilder& TriggerBuilder::SetConversionDestination(
    net::SchemefulSite conversion_destination) {
  conversion_destination_ = std::move(conversion_destination);
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

AttributionTrigger TriggerBuilder::Build() const {
  return AttributionTrigger(trigger_data_, conversion_destination_,
                            reporting_origin_, event_source_trigger_data_,
                            priority_, dedup_key_, debug_key_);
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

ReportBuilder& ReportBuilder::SetReportId(
    absl::optional<AttributionReport::EventLevelData::Id> id) {
  report_id_ = id;
  return *this;
}

AttributionReport ReportBuilder::Build() const {
  return AttributionReport(
      attribution_info_, report_time_, external_report_id_,
      AttributionReport::EventLevelData(trigger_data_, priority_, report_id_));
}

bool operator==(const AttributionTrigger& a, const AttributionTrigger& b) {
  const auto tie = [](const AttributionTrigger& t) {
    return std::make_tuple(t.trigger_data(), t.conversion_destination(),
                           t.reporting_origin(), t.event_source_trigger_data(),
                           t.priority(), t.dedup_key());
  };
  return tie(a) == tie(b);
}

bool operator==(const CommonSourceInfo& a, const CommonSourceInfo& b) {
  const auto tie = [](const CommonSourceInfo& source) {
    return std::make_tuple(source.source_event_id(), source.impression_origin(),
                           source.conversion_origin(),
                           source.reporting_origin(), source.impression_time(),
                           source.expiry_time(), source.source_type(),
                           source.priority(), source.debug_key());
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
                           source.dedup_keys());
  };
  return tie(a) == tie(b);
}

bool operator==(const HistogramContribution& a,
                const HistogramContribution& b) {
  const auto tie = [](const HistogramContribution& contribution) {
    return std::make_tuple(contribution.bucket(), contribution.value());
  };
  return tie(a) == tie(b);
}

bool operator==(const AggregatableAttribution& a, AggregatableAttribution& b) {
  const auto tie = [](const AggregatableAttribution& aggregatable_attribution) {
    return std::make_tuple(aggregatable_attribution.source_id,
                           aggregatable_attribution.trigger_time,
                           aggregatable_attribution.report_time,
                           aggregatable_attribution.contributions);
  };
  return tie(a) == tie(b);
}

// Does not compare ID as it is set by the underlying sqlite db and
// should not be tested.
bool operator==(const AttributionReport::EventLevelData& a,
                const AttributionReport::EventLevelData& b) {
  const auto tie = [](const AttributionReport::EventLevelData& data) {
    return std::make_tuple(data.trigger_data, data.priority);
  };
  return tie(a) == tie(b);
}

// Does not compare ID as it is set by the underlying sqlite db and
// should not be tested.
bool operator==(const AttributionReport::AggregatableContributionData& a,
                const AttributionReport::AggregatableContributionData& b) {
  return a.contribution == b.contribution;
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

std::ostream& operator<<(std::ostream& out, AttributionTrigger::Result status) {
  switch (status) {
    case AttributionTrigger::Result::kSuccess:
      out << "success";
      break;
    case AttributionTrigger::Result::kSuccessDroppedLowerPriority:
      out << "successDroppedLowerPriority";
      break;
    case AttributionTrigger::Result::kInternalError:
      out << "internalError";
      break;
    case AttributionTrigger::Result::kNoCapacityForConversionDestination:
      out << "insufficientDestinationCapacity";
      break;
    case AttributionTrigger::Result::kNoMatchingImpressions:
      out << "noMatchingSources";
      break;
    case AttributionTrigger::Result::kDeduplicated:
      out << "deduplicated";
      break;
    case AttributionTrigger::Result::kExcessiveAttributions:
      out << "excessiveAttributions";
      break;
    case AttributionTrigger::Result::kPriorityTooLow:
      out << "priorityTooLow";
      break;
    case AttributionTrigger::Result::kDroppedForNoise:
      out << "noised";
      break;
    case AttributionTrigger::Result::kExcessiveReportingOrigins:
      out << "excessiveReportingOrigins";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, DeactivatedSource::Reason reason) {
  switch (reason) {
    case DeactivatedSource::Reason::kReplacedByNewerSource:
      out << "kReplacedByNewerSource";
      break;
    case DeactivatedSource::Reason::kReachedAttributionLimit:
      out << "kReachedAttributionLimit";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, RateLimitTable::Result result) {
  switch (result) {
    case RateLimitTable::Result::kAllowed:
      out << "kAllowed";
      break;
    case RateLimitTable::Result::kNotAllowed:
      out << "kNotAllowed";
      break;
    case RateLimitTable::Result::kError:
      out << "kError";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         CommonSourceInfo::SourceType source_type) {
  switch (source_type) {
    case CommonSourceInfo::SourceType::kNavigation:
      out << "kNavigation";
      break;
    case CommonSourceInfo::SourceType::kEvent:
      out << "kEvent";
      break;
  }
  return out;
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

std::ostream& operator<<(std::ostream& out,
                         const AttributionTrigger& conversion) {
  return out << "{trigger_data=" << conversion.trigger_data()
             << ",conversion_destination="
             << conversion.conversion_destination().Serialize()
             << ",reporting_origin=" << conversion.reporting_origin()
             << ",event_source_trigger_data="
             << conversion.event_source_trigger_data()
             << ",priority=" << conversion.priority() << ",dedup_key="
             << (conversion.dedup_key()
                     ? base::NumberToString(*conversion.dedup_key())
                     : "null")
             << ",debug_key="
             << (conversion.debug_key()
                     ? base::NumberToString(*conversion.debug_key())
                     : "null")
             << "}";
}

std::ostream& operator<<(std::ostream& out, const CommonSourceInfo& source) {
  return out << "{source_event_id=" << source.source_event_id()
             << ",impression_origin=" << source.impression_origin()
             << ",conversion_origin=" << source.conversion_origin()
             << ",reporting_origin=" << source.reporting_origin()
             << ",impression_time=" << source.impression_time()
             << ",expiry_time=" << source.expiry_time()
             << ",source_type=" << source.source_type()
             << ",priority=" << source.priority() << ",debug_key="
             << (source.debug_key() ? base::NumberToString(*source.debug_key())
                                    : "null")
             << "}";
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
      << ",source_id=" << *source.source_id() << ",dedup_keys=[";

  const char* separator = "";
  for (int64_t dedup_key : source.dedup_keys()) {
    out << separator << dedup_key;
    separator = ", ";
  }

  return out << "]}";
}

std::ostream& operator<<(std::ostream& out,
                         const HistogramContribution& contribution) {
  return out << "{bucket=" << contribution.bucket()
             << ",value=" << contribution.value() << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AggregatableAttribution& aggregatable_attribution) {
  out << "{source_id=" << aggregatable_attribution.source_id
      << ",trigger_time=" << aggregatable_attribution.trigger_time
      << ",report_time=" << aggregatable_attribution.report_time
      << ",contributions=[";

  const char* separator = "";
  for (const HistogramContribution& contribution :
       aggregatable_attribution.contributions) {
    out << separator << contribution;
    separator = ", ";
  }

  return out << "]}";
}

std::ostream& operator<<(std::ostream& out,
                         const AttributionReport::EventLevelData& data) {
  return out << "{trigger_data=" << data.trigger_data
             << ",priority=" << data.priority
             << ",id=" << (data.id ? base::NumberToString(**data.id) : "null")
             << "}";
}

std::ostream& operator<<(
    std::ostream& out,
    const AttributionReport::AggregatableContributionData& data) {
  return out << "{contribution=" << data.contribution
             << ",id=" << (data.id ? base::NumberToString(**data.id) : "null")
             << "}";
}

namespace {
std::ostream& operator<<(
    std::ostream& out,
    const absl::variant<AttributionReport::EventLevelData,
                        AttributionReport::AggregatableContributionData>&
        data) {
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
      return out << "kSuccess";
    case StorableSource::Result::kInternalError:
      return out << "kInternalError";
    case StorableSource::Result::kInsufficientSourceCapacity:
      return out << "kInsufficientSourceCapacity";
    case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
      return out << "kInsufficientUniqueDestinationCapacity";
    case StorableSource::Result::kExcessiveReportingOrigins:
      return out << "kExcessiveReportingOrigins";
  }
}

std::vector<AttributionReport> GetAttributionsToReportForTesting(
    AttributionManagerImpl* manager,
    base::Time max_report_time) {
  base::RunLoop run_loop;
  std::vector<AttributionReport> attribution_reports;
  manager->attribution_storage_
      .AsyncCall(&AttributionStorage::GetAttributionsToReport)
      .WithArgs(max_report_time, /*limit=*/-1)
      .Then(base::BindOnce(base::BindLambdaForTesting(
          [&](std::vector<AttributionReport> reports) {
            attribution_reports = std::move(reports);
            run_loop.Quit();
          })));
  run_loop.Run();
  return attribution_reports;
}

}  // namespace content
