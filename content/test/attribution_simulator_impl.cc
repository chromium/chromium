// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/attribution_simulator.h"

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "content/browser/attribution_reporting/attribution_cookie_checker.h"
#include "content/browser/attribution_reporting/attribution_manager_impl.h"
#include "content/browser/attribution_reporting/attribution_network_sender.h"
#include "content/browser/attribution_reporting/attribution_report.h"
#include "content/browser/attribution_reporting/attribution_storage_delegate_impl.h"
#include "content/browser/attribution_reporting/common_source_info.h"
#include "content/browser/attribution_reporting/send_result.h"
#include "content/browser/attribution_reporting/stored_source.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/test/browser_task_environment.h"
#include "content/test/attribution_simulator_input_parser.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "url/gurl.h"

namespace content {

namespace {

base::Time GetEventTime(const AttributionSimulationEvent& event) {
  struct Visitor {
    base::Time operator()(const StorableSource& source) {
      return source.common_info().impression_time();
    }

    base::Time operator()(const AttributionTriggerAndTime& trigger) {
      return trigger.time;
    }
  };

  return absl::visit(Visitor{}, event);
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
                         report.source().attribution_logic() ==
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

struct EventHandler {
  base::raw_ptr<AttributionManagerImpl> manager;

  void operator()(StorableSource source) {
    manager->MaybeEnqueueEventForTesting(std::move(source));
  }

  void operator()(AttributionTriggerAndTime trigger) {
    manager->MaybeEnqueueEventForTesting(std::move(trigger.trigger));
  }
};

}  // namespace

base::Value RunAttributionSimulationOrExit(
    const base::Value& input,
    const AttributionSimulationOptions& options) {
  // Prerequisites for using an environment with mock time.
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  std::vector<AttributionSimulationEvent> events =
      ParseAttributionSimulationInputOrExit(input, base::Time::Now());
  base::ranges::stable_sort(events, /*comp=*/{}, &GetEventTime);

  // Avoid creating an on-disk sqlite DB.
  content::AttributionManagerImpl::RunInMemoryForTesting();

  // Ensure that the manager always thinks the browser is online.
  auto network_connection_tracker =
      network::TestNetworkConnectionTracker::CreateInstance();
  content::SetNetworkConnectionTrackerForTesting(
      network_connection_tracker.get());

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

  // TODO(apaseltiner): Add an `AttributionManager::Observer` to `manager` so we
  // can record dropped reports in the output.

  EventHandler handler{.manager = manager.get()};

  for (auto& event : events) {
    task_environment.FastForwardBy(GetEventTime(event) - base::Time::Now());
    absl::visit(handler, std::move(event));
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
  return output;
}

}  // namespace content
