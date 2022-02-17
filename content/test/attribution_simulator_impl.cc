// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <memory>
#include <sstream>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_manager.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_network_sender.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/attribution_test_utils.h"
#include "content/browser/attribution_reporting/attribution_trigger.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/attribution_simulator_input_parser.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {

namespace {

base::Time GetEventTime(const AttributionSimulationEventAndValue& event) {
  struct Visitor {
    base::Time operator()(const StorableSource& source) {
      return source.common_info().impression_time();
    }

    base::Time operator()(const AttributionTriggerAndTime& trigger) {
      return trigger.time;
    }
  };

  return absl::visit(Visitor{}, event.first);
}

// TODO(apaseltiner): Consider exposing other behaviors here.
class AlwaysSetCookieChecker : public AttributionCookieChecker {
 public:
  AlwaysSetCookieChecker() = default;

  ~AlwaysSetCookieChecker() override = default;

  AlwaysSetCookieChecker(const AlwaysSetCookieChecker&) = delete;
  AlwaysSetCookieChecker(AlwaysSetCookieChecker&&) = delete;

  AlwaysSetCookieChecker& operator=(const AlwaysSetCookieChecker&) = delete;
  AlwaysSetCookieChecker& operator=(AlwaysSetCookieChecker&&) = delete;

 private:
  // AttributionManagerImpl::CookieChecker:
  void IsDebugCookieSet(const url::Origin& origin,
                        base::OnceCallback<void(bool)> callback) override {
    std::move(callback).Run(true);
  }
};

class SentReportAccumulator : public AttributionNetworkSender {
 public:
  SentReportAccumulator(base::Value::ListStorage& reports,
                        bool remove_report_ids)
      : time_origin_(base::Time::Now()),
        remove_report_ids_(remove_report_ids),
        reports_(reports) {}

  ~SentReportAccumulator() override = default;

  SentReportAccumulator(const SentReportAccumulator&) = delete;
  SentReportAccumulator(SentReportAccumulator&&) = delete;

  SentReportAccumulator& operator=(const SentReportAccumulator&) = delete;
  SentReportAccumulator& operator=(SentReportAccumulator&&) = delete;

 private:
  // AttributionManagerImpl::NetworkSender:
  void SendReport(AttributionReport report,
                  ReportSentCallback sent_callback) override {
    base::Value report_body = report.ReportBody();
    if (remove_report_ids_)
      report_body.RemoveKey("report_id");

    base::DictionaryValue value;
    value.SetKey("report", std::move(report_body));
    value.SetStringKey("report_url", report.ReportURL().spec());
    value.SetIntKey("report_time",
                    (base::Time::Now() - time_origin_).InSeconds());

    base::DictionaryValue test_info;
    test_info.SetBoolKey("randomized_trigger",
                         report.attribution_info().source.attribution_logic() ==
                             StoredSource::AttributionLogic::kFalsely);
    value.SetKey("test_info", std::move(test_info));

    reports_.push_back(std::move(value));

    std::move(sent_callback)
        .Run(std::move(report), SendResult(SendResult::Status::kSent,
                                           /*http_response_code=*/200));
  }

  const base::Time time_origin_;
  const bool remove_report_ids_;
  base::Value::ListStorage& reports_;
};

// Registers sources and triggers in the `AttributionManagerImpl` and records
// rejected sources in a JSON list.
class AttributionEventHandler : public AttributionManager::Observer {
 public:
  AttributionEventHandler(AttributionManagerImpl* manager,
                          base::Value::ListStorage& rejected_sources,
                          base::Value::ListStorage& rejected_triggers)
      : manager_(manager),
        rejected_sources_(rejected_sources),
        rejected_triggers_(rejected_triggers) {
    observation_.Observe(manager);
  }

  ~AttributionEventHandler() override = default;

  void Handle(AttributionSimulationEventAndValue event) {
    // Sources and triggers are handled in order; this includes observer
    // invocations. Therefore, we can track the original `base::Value`
    // associated with the event using a queue.

    input_values_.push_back(std::move(event.second));
    absl::visit(*this, std::move(event.first));
  }

  // For use with `absl::visit()`.
  void operator()(StorableSource source) {
    manager_->MaybeEnqueueEventForTesting(std::move(source));
  }

  // For use with `absl::visit()`.
  void operator()(AttributionTriggerAndTime trigger) {
    manager_->MaybeEnqueueEventForTesting(std::move(trigger.trigger));
  }

 private:
  // AttributionManager::Observer:

  void OnSourceHandled(const StorableSource& source,
                       StorableSource::Result result) override {
    DCHECK(!input_values_.empty());
    base::Value input_value = std::move(input_values_.front());
    input_values_.pop_front();

    const char* reason;
    switch (result) {
      case StorableSource::Result::kSuccess:
        return;
      case StorableSource::Result::kInternalError:
        reason = "internalError";
        break;
      case StorableSource::Result::kInsufficientSourceCapacity:
        reason = "insufficientSourceCapacity";
        break;
      case StorableSource::Result::kInsufficientUniqueDestinationCapacity:
        reason = "insufficientUniqueDestinationCapacity";
        break;
      case StorableSource::Result::kExcessiveReportingOrigins:
        reason = "excessiveReportingOrigins";
        break;
    }

    base::DictionaryValue dict;
    dict.SetStringKey("reason", reason);
    dict.SetKey("source", std::move(input_value));

    rejected_sources_.push_back(std::move(dict));
  }

  void OnTriggerHandled(
      const AttributionStorage::CreateReportResult& result) override {
    DCHECK(!input_values_.empty());
    base::Value input_value = std::move(input_values_.front());
    input_values_.pop_front();

    std::stringstream reason;
    switch (result.status()) {
      case AttributionTrigger::Result::kSuccess:
      case AttributionTrigger::Result::kSuccessDroppedLowerPriority:
        // TODO(apaseltiner): Consider surfacing reports dropped due to
        // prioritization.
        return;
      case AttributionTrigger::Result::kInternalError:
      case AttributionTrigger::Result::kNoCapacityForConversionDestination:
      case AttributionTrigger::Result::kNoMatchingImpressions:
      case AttributionTrigger::Result::kDeduplicated:
      case AttributionTrigger::Result::kExcessiveAttributions:
      case AttributionTrigger::Result::kPriorityTooLow:
      case AttributionTrigger::Result::kDroppedForNoise:
      case AttributionTrigger::Result::kExcessiveReportingOrigins:
        reason << result.status();
        break;
    }

    base::DictionaryValue dict;
    dict.SetStringKey("reason", reason.str());
    dict.SetKey("trigger", std::move(input_value));

    rejected_triggers_.push_back(std::move(dict));
  }

  base::ScopedObservation<AttributionManager, AttributionManager::Observer>
      observation_{this};

  base::raw_ptr<AttributionManagerImpl> manager_;

  base::Value::ListStorage& rejected_sources_;
  base::Value::ListStorage& rejected_triggers_;

  base::circular_deque<base::Value> input_values_;
};

}  // namespace

base::Value RunAttributionSimulationOrExit(
    base::Value input,
    const AttributionSimulationOptions& options) {
  // Prerequisites for using an environment with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::vector<AttributionSimulationEventAndValue> events =
      ParseAttributionSimulationInputOrExit(std::move(input),
                                            base::Time::Now());
  base::ranges::stable_sort(events, /*comp=*/{}, &GetEventTime);

  // Avoid creating an on-disk sqlite DB.
  content::AttributionManagerImpl::RunInMemoryForTesting();

  auto always_allow_reports_callback =
      base::BindRepeating([](const AttributionReport&) { return true; });

  // This isn't needed because the DB is completely in memory for testing.
  const base::FilePath user_data_directory;

  base::Value::ListStorage reports;
  auto manager = AttributionManagerImpl::CreateForTesting(
      std::move(always_allow_reports_callback), user_data_directory,
      /*special_storage_policy=*/nullptr,
      std::make_unique<AttributionStorageDelegateImpl>(options.noise_mode,
                                                       options.delay_mode),
      std::make_unique<AlwaysSetCookieChecker>(),
      /*network_sender=*/
      std::make_unique<SentReportAccumulator>(reports,
                                              options.remove_report_ids));

  base::Value::ListStorage rejected_sources;
  base::Value::ListStorage rejected_triggers;
  AttributionEventHandler handler(manager.get(), rejected_sources,
                                  rejected_triggers);

  for (auto& event : events) {
    task_environment.FastForwardBy(GetEventTime(event) - base::Time::Now());
    handler.Handle(std::move(event));
  }

  absl::optional<base::Time> last_report_time;

  base::RunLoop loop;
  manager->GetPendingReportsForInternalUse(
      base::BindLambdaForTesting([&](std::vector<AttributionReport> reports) {
        if (!reports.empty()) {
          last_report_time = base::ranges::max(reports, /*comp=*/{},
                                               &AttributionReport::report_time)
                                 .report_time();
        }

        loop.Quit();
      }));

  loop.Run();
  if (last_report_time.has_value())
    task_environment.FastForwardBy(*last_report_time - base::Time::Now());

  base::Value output(base::Value::Type::DICTIONARY);
  output.SetKey("reports", base::Value(std::move(reports)));

  if (!rejected_sources.empty())
    output.SetKey("rejected_sources", base::Value(std::move(rejected_sources)));

  if (!rejected_triggers.empty()) {
    output.SetKey("rejected_triggers",
                  base::Value(std::move(rejected_triggers)));
  }

  return output;
}

}  // namespace content
