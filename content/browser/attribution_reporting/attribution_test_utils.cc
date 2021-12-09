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
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner_util.h"
#include "base/test/bind.h"
#include "url/gurl.h"

namespace content {

namespace {

using AttributionAllowedStatus =
    ::content::RateLimitTable::AttributionAllowedStatus;
using CreateReportStatus =
    ::content::AttributionStorage::CreateReportResult::Status;
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

base::GUID DefaultExternalReportID() {
  return base::GUID::ParseLowercase("21abd97f-73e8-4b88-9389-a9fee6abda5e");
}

ConfigurableStorageDelegate::ConfigurableStorageDelegate() = default;
ConfigurableStorageDelegate::~ConfigurableStorageDelegate() = default;

base::Time ConfigurableStorageDelegate::GetReportTime(
    const StorableSource& source,
    base::Time trigger_time) const {
  return source.impression_time() + base::Milliseconds(report_time_ms_);
}

int ConfigurableStorageDelegate::GetMaxAttributionsPerSource(
    StorableSource::SourceType source_type) const {
  return max_attributions_per_source_;
}

int ConfigurableStorageDelegate::GetMaxSourcesPerOrigin() const {
  return max_sources_per_origin_;
}

int ConfigurableStorageDelegate::GetMaxAttributionsPerOrigin() const {
  return max_attributions_per_origin_;
}

int ConfigurableStorageDelegate::GetMaxAttributionDestinationsPerEventSource()
    const {
  return max_attribution_destinations_per_event_source_;
}

AttributionStorage::Delegate::RateLimitConfig
ConfigurableStorageDelegate::GetRateLimits(
    AttributionStorage::AttributionType attribution_type) const {
  return rate_limits_;
}

uint64_t ConfigurableStorageDelegate::GetFakeEventSourceTriggerData() const {
  return fake_event_source_trigger_data_;
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

const AttributionPolicy& MockAttributionManager::GetAttributionPolicy() const {
  return policy_;
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

void MockAttributionManager::NotifyReportSent(const SentReport& info) {
  for (Observer& observer : observers_)
    observer.OnReportSent(info);
}

void MockAttributionManager::NotifyReportDropped(
    const AttributionStorage::CreateReportResult& result) {
  for (Observer& observer : observers_)
    observer.OnReportDropped(result);
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
    StorableSource::SourceType source_type) {
  source_type_ = source_type;
  return *this;
}

SourceBuilder& SourceBuilder::SetPriority(int64_t priority) {
  priority_ = priority;
  return *this;
}

SourceBuilder& SourceBuilder::SetAttributionLogic(
    StorableSource::AttributionLogic attribution_logic) {
  attribution_logic_ = attribution_logic;
  return *this;
}

SourceBuilder& SourceBuilder::SetImpressionId(
    absl::optional<StorableSource::Id> impression_id) {
  impression_id_ = impression_id;
  return *this;
}

SourceBuilder& SourceBuilder::SetDedupKeys(std::vector<int64_t> dedup_keys) {
  dedup_keys_ = std::move(dedup_keys);
  return *this;
}

StorableSource SourceBuilder::Build() const {
  StorableSource impression(
      source_event_id_, impression_origin_, conversion_origin_,
      reporting_origin_, impression_time_,
      /*expiry_time=*/impression_time_ + expiry_, source_type_, priority_,
      attribution_logic_, impression_id_);
  impression.SetDedupKeys(dedup_keys_);
  return impression;
}

StorableTrigger DefaultTrigger() {
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

TriggerBuilder& TriggerBuilder::SetDedupKey(absl::optional<int64_t> dedup_key) {
  dedup_key_ = dedup_key;
  return *this;
}

StorableTrigger TriggerBuilder::Build() const {
  return StorableTrigger(trigger_data_, conversion_destination_,
                         reporting_origin_, event_source_trigger_data_,
                         priority_, dedup_key_);
}

ReportBuilder::ReportBuilder(StorableSource source)
    : source_(std::move(source)),
      external_report_id_(DefaultExternalReportID()) {}

ReportBuilder::~ReportBuilder() = default;

ReportBuilder& ReportBuilder::SetTriggerData(uint64_t trigger_data) {
  trigger_data_ = trigger_data;
  return *this;
}

ReportBuilder& ReportBuilder::SetConversionTime(base::Time time) {
  conversion_time_ = time;
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
    absl::optional<AttributionReport::Id> id) {
  report_id_ = id;
  return *this;
}

AttributionReport ReportBuilder::Build() const {
  return AttributionReport(source_, trigger_data_, conversion_time_,
                           report_time_, priority_, external_report_id_,
                           report_id_);
}

// Custom comparator for `StorableSource` that does not take impression IDs
// into account.
bool operator==(const StorableSource& a, const StorableSource& b) {
  const auto tie = [](const StorableSource& impression) {
    return std::make_tuple(
        impression.source_event_id(), impression.impression_origin(),
        impression.conversion_origin(), impression.reporting_origin(),
        impression.impression_time(), impression.expiry_time(),
        impression.source_type(), impression.priority(),
        impression.attribution_logic(), impression.dedup_keys());
  };
  return tie(a) == tie(b);
}

// Custom comparator for comparing two vectors of conversion reports. Does not
// compare impression and conversion IDs as they are set by the underlying
// sqlite db and should not be tested.
bool operator==(const AttributionReport& a, const AttributionReport& b) {
  const auto tie = [](const AttributionReport& conversion) {
    return std::make_tuple(conversion.impression, conversion.trigger_data,
                           conversion.conversion_time, conversion.report_time,
                           conversion.priority, conversion.external_report_id,
                           conversion.failed_send_attempts);
  };
  return tie(a) == tie(b);
}

bool operator==(const SentReport& a, const SentReport& b) {
  const auto tie = [](const SentReport& info) {
    return std::make_tuple(info.report, info.status, info.http_response_code);
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

std::ostream& operator<<(std::ostream& out, CreateReportStatus status) {
  switch (status) {
    case CreateReportStatus::kSuccess:
      out << "kSuccess";
      break;
    case CreateReportStatus::kSuccessDroppedLowerPriority:
      out << "kSuccessDroppedLowerPriority";
      break;
    case CreateReportStatus::kInternalError:
      out << "kInternalError";
      break;
    case CreateReportStatus::kNoCapacityForConversionDestination:
      out << "kNoCapacityForConversionDestination";
      break;
    case CreateReportStatus::kNoMatchingImpressions:
      out << "kNoMatchingImpressions";
      break;
    case CreateReportStatus::kDeduplicated:
      out << "kDeduplicated";
      break;
    case CreateReportStatus::kRateLimited:
      out << "kRateLimited";
      break;
    case CreateReportStatus::kPriorityTooLow:
      out << "kPriorityTooLow";
      break;
    case CreateReportStatus::kDroppedForNoise:
      out << "kDroppedForNoise";
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

std::ostream& operator<<(std::ostream& out, AttributionAllowedStatus status) {
  switch (status) {
    case AttributionAllowedStatus::kAllowed:
      out << "kAllowed";
      break;
    case AttributionAllowedStatus::kNotAllowed:
      out << "kNotAllowed";
      break;
    case AttributionAllowedStatus::kError:
      out << "kError";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         StorableSource::SourceType source_type) {
  switch (source_type) {
    case StorableSource::SourceType::kNavigation:
      out << "kNavigation";
      break;
    case StorableSource::SourceType::kEvent:
      out << "kEvent";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out,
                         StorableSource::AttributionLogic attribution_logic) {
  switch (attribution_logic) {
    case StorableSource::AttributionLogic::kNever:
      out << "kNever";
      break;
    case StorableSource::AttributionLogic::kTruthfully:
      out << "kTruthfully";
      break;
    case StorableSource::AttributionLogic::kFalsely:
      out << "kFalsely";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const StorableTrigger& conversion) {
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
             << "}";
}

std::ostream& operator<<(std::ostream& out, const StorableSource& impression) {
  out << "{source_event_id=" << impression.source_event_id()
      << ",impression_origin=" << impression.impression_origin()
      << ",conversion_origin=" << impression.conversion_origin()
      << ",reporting_origin=" << impression.reporting_origin()
      << ",impression_time=" << impression.impression_time()
      << ",expiry_time=" << impression.expiry_time()
      << ",source_type=" << impression.source_type()
      << ",priority=" << impression.priority()
      << ",attribution_logic=" << impression.attribution_logic()
      << ",impression_id="
      << (impression.impression_id()
              ? base::NumberToString(**impression.impression_id())
              : "null")
      << ",dedup_keys=[";

  const char* separator = "";
  for (int64_t dedup_key : impression.dedup_keys()) {
    out << separator << dedup_key;
    separator = ", ";
  }

  return out << "]}";
}

std::ostream& operator<<(std::ostream& out, const AttributionReport& report) {
  return out << "{impression=" << report.impression
             << ",trigger_data=" << report.trigger_data
             << ",conversion_time=" << report.conversion_time
             << ",report_time=" << report.report_time
             << ",priority=" << report.priority
             << ",external_report_id=" << report.external_report_id
             << ",conversion_id="
             << (report.conversion_id
                     ? base::NumberToString(**report.conversion_id)
                     : "null")
             << ",failed_send_attempts=" << report.failed_send_attempts << "}";
}

std::ostream& operator<<(std::ostream& out, SentReport::Status status) {
  switch (status) {
    case SentReport::Status::kSent:
      out << "kSent";
      break;
    case SentReport::Status::kTransientFailure:
      out << "kTransientFailure";
      break;
    case SentReport::Status::kFailure:
      out << "kFailure";
      break;
    case SentReport::Status::kDropped:
      out << "kDropped";
      break;
    case SentReport::Status::kOffline:
      out << "kOffline";
      break;
    case SentReport::Status::kRemovedFromQueue:
      out << "kRemovedFromQueue";
      break;
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const SentReport& info) {
  return out << "{report=" << info.report << ",status=" << info.status
             << ",http_response_code=" << info.http_response_code << "}";
}

std::ostream& operator<<(std::ostream& out,
                         const DeactivatedSource& deactivated_source) {
  return out << "{source=" << deactivated_source.source
             << ",reason=" << deactivated_source.reason << "}";
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
