// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_test_util.h"

#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "net/base/network_isolation_key.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_delivery_agent.h"
#include "net/reporting/reporting_endpoint.h"
#include "net/reporting/reporting_garbage_collector.h"
#include "net/reporting/reporting_policy.h"
#include "net/reporting/reporting_uploader.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net {

namespace {

class PendingUploadImpl : public TestReportingUploader::PendingUpload {
 public:
  PendingUploadImpl(const url::Origin& report_origin,
                    const GURL& url,
                    const NetworkIsolationKey& network_isolation_key,
                    const std::string& json,
                    ReportingUploader::UploadCallback callback,
                    base::OnceCallback<void(PendingUpload*)> complete_callback)
      : report_origin_(report_origin),
        url_(url),
        network_isolation_key_(network_isolation_key),
        json_(json),
        callback_(std::move(callback)),
        complete_callback_(std::move(complete_callback)) {}

  ~PendingUploadImpl() override = default;

  // PendingUpload implementation:
  const url::Origin& report_origin() const override { return report_origin_; }
  const GURL& url() const override { return url_; }
  const std::string& json() const override { return json_; }
  std::unique_ptr<base::Value> GetValue() const override {
    return base::JSONReader::ReadDeprecated(json_);
  }

  void Complete(ReportingUploader::Outcome outcome) override {
    std::move(callback_).Run(outcome);
    // Deletes |this|.
    std::move(complete_callback_).Run(this);
  }

 private:
  url::Origin report_origin_;
  GURL url_;
  NetworkIsolationKey network_isolation_key_;
  std::string json_;
  ReportingUploader::UploadCallback callback_;
  base::OnceCallback<void(PendingUpload*)> complete_callback_;
};

void ErasePendingUpload(
    std::vector<std::unique_ptr<TestReportingUploader::PendingUpload>>* uploads,
    TestReportingUploader::PendingUpload* upload) {
  for (auto it = uploads->begin(); it != uploads->end(); ++it) {
    if (it->get() == upload) {
      uploads->erase(it);
      return;
    }
  }
  NOTREACHED();
}

}  // namespace

RandIntCallback TestReportingRandIntCallback() {
  return base::BindRepeating(
      [](int* rand_counter, int min, int max) {
        DCHECK_LE(min, max);
        return min + ((*rand_counter)++ % (max - min + 1));
      },
      base::Owned(std::make_unique<int>(0)));
}

TestReportingUploader::PendingUpload::~PendingUpload() = default;
TestReportingUploader::PendingUpload::PendingUpload() = default;

TestReportingUploader::TestReportingUploader() = default;
TestReportingUploader::~TestReportingUploader() = default;

void TestReportingUploader::StartUpload(
    const url::Origin& report_origin,
    const GURL& url,
    const NetworkIsolationKey& network_isolation_key,
    const std::string& json,
    int max_depth,
    UploadCallback callback) {
  pending_uploads_.push_back(std::make_unique<PendingUploadImpl>(
      report_origin, url, network_isolation_key, json, std::move(callback),
      base::BindOnce(&ErasePendingUpload, &pending_uploads_)));
}

void TestReportingUploader::OnShutdown() {
  pending_uploads_.clear();
}

int TestReportingUploader::GetPendingUploadCountForTesting() const {
  return pending_uploads_.size();
}

TestReportingDelegate::TestReportingDelegate() = default;

TestReportingDelegate::~TestReportingDelegate() = default;

bool TestReportingDelegate::CanQueueReport(const url::Origin& origin) const {
  return true;
}

void TestReportingDelegate::CanSendReports(
    std::set<url::Origin> origins,
    base::OnceCallback<void(std::set<url::Origin>)> result_callback) const {
  if (pause_permissions_check_) {
    saved_origins_ = std::move(origins);
    permissions_check_callback_ = std::move(result_callback);
    return;
  }

  if (disallow_report_uploads_)
    origins.clear();
  std::move(result_callback).Run(std::move(origins));
}

bool TestReportingDelegate::PermissionsCheckPaused() const {
  return !permissions_check_callback_.is_null();
}

void TestReportingDelegate::ResumePermissionsCheck() {
  if (disallow_report_uploads_)
    saved_origins_.clear();
  std::move(permissions_check_callback_).Run(std::move(saved_origins_));
}

bool TestReportingDelegate::CanSetClient(const url::Origin& origin,
                                         const GURL& endpoint) const {
  return true;
}

bool TestReportingDelegate::CanUseClient(const url::Origin& origin,
                                         const GURL& endpoint) const {
  return true;
}

TestReportingContext::TestReportingContext(
    base::Clock* clock,
    const base::TickClock* tick_clock,
    const ReportingPolicy& policy,
    ReportingCache::PersistentReportingStore* store)
    : ReportingContext(policy,
                       clock,
                       tick_clock,
                       TestReportingRandIntCallback(),
                       std::make_unique<TestReportingUploader>(),
                       std::make_unique<TestReportingDelegate>(),
                       store),
      delivery_timer_(new base::MockOneShotTimer()),
      garbage_collection_timer_(new base::MockOneShotTimer()) {
  garbage_collector()->SetTimerForTesting(
      base::WrapUnique(garbage_collection_timer_));
  delivery_agent()->SetTimerForTesting(base::WrapUnique(delivery_timer_));
}

TestReportingContext::~TestReportingContext() {
  delivery_timer_ = nullptr;
  garbage_collection_timer_ = nullptr;
}

ReportingTestBase::ReportingTestBase() : store_(nullptr) {
  // For tests, disable jitter.
  ReportingPolicy policy;
  policy.endpoint_backoff_policy.jitter_factor = 0.0;

  CreateContext(policy, base::Time::Now(), base::TimeTicks::Now());
}

ReportingTestBase::~ReportingTestBase() = default;

void ReportingTestBase::UsePolicy(const ReportingPolicy& new_policy) {
  CreateContext(new_policy, clock()->Now(), tick_clock()->NowTicks());
}

void ReportingTestBase::UseStore(
    ReportingCache::PersistentReportingStore* store) {
  store_ = store;
  CreateContext(policy(), clock()->Now(), tick_clock()->NowTicks());
}

const ReportingEndpoint ReportingTestBase::FindEndpointInCache(
    const url::Origin& origin,
    const std::string& group_name,
    const GURL& url) {
  return cache()->GetEndpointForTesting(origin, group_name, url);
}

bool ReportingTestBase::SetEndpointInCache(const url::Origin& origin,
                                           const std::string& group_name,
                                           const GURL& url,
                                           base::Time expires,
                                           OriginSubdomains include_subdomains,
                                           int priority,
                                           int weight) {
  cache()->SetEndpointForTesting(origin, group_name, url, include_subdomains,
                                 expires, priority, weight);
  const ReportingEndpoint endpoint =
      FindEndpointInCache(origin, group_name, url);
  return endpoint.is_valid();
}

bool ReportingTestBase::EndpointExistsInCache(const url::Origin& origin,
                                              const std::string& group_name,
                                              const GURL& url) {
  ReportingEndpoint endpoint =
      cache()->GetEndpointForTesting(origin, group_name, url);
  return endpoint.is_valid();
}

ReportingEndpoint::Statistics ReportingTestBase::GetEndpointStatistics(
    const url::Origin& origin,
    const std::string& group_name,
    const GURL& url) {
  ReportingEndpoint endpoint =
      cache()->GetEndpointForTesting(origin, group_name, url);
  if (endpoint)
    return endpoint.stats;
  return ReportingEndpoint::Statistics();
}

bool ReportingTestBase::EndpointGroupExistsInCache(
    const url::Origin& origin,
    const std::string& group_name,
    OriginSubdomains include_subdomains,
    base::Time expires) {
  return cache()->EndpointGroupExistsForTesting(origin, group_name,
                                                include_subdomains, expires);
}

bool ReportingTestBase::OriginClientExistsInCache(const url::Origin& origin) {
  std::vector<url::Origin> all_origins = cache()->GetAllOrigins();
  for (const url::Origin& cur_origin : all_origins) {
    if (cur_origin == origin)
      return true;
  }
  return false;
}

GURL ReportingTestBase::MakeURL(size_t index) {
  return GURL(base::StringPrintf("https://example%zd.test", index));
}

void ReportingTestBase::SimulateRestart(base::TimeDelta delta,
                                        base::TimeDelta delta_ticks) {
  CreateContext(policy(), clock()->Now() + delta,
                tick_clock()->NowTicks() + delta_ticks);
}

void ReportingTestBase::CreateContext(const ReportingPolicy& policy,
                                      base::Time now,
                                      base::TimeTicks now_ticks) {
  context_ = std::make_unique<TestReportingContext>(&clock_, &tick_clock_,
                                                    policy, store_);
  clock()->SetNow(now);
  tick_clock()->SetNowTicks(now_ticks);
}

base::TimeTicks ReportingTestBase::yesterday() {
  return tick_clock()->NowTicks() - base::TimeDelta::FromDays(1);
}

base::TimeTicks ReportingTestBase::now() {
  return tick_clock()->NowTicks();
}

base::TimeTicks ReportingTestBase::tomorrow() {
  return tick_clock()->NowTicks() + base::TimeDelta::FromDays(1);
}

TestReportingService::Report::Report() = default;

TestReportingService::Report::Report(Report&& other)
    : url(other.url),
      user_agent(other.user_agent),
      group(other.group),
      type(other.type),
      body(std::move(other.body)),
      depth(other.depth) {}

TestReportingService::Report::Report(const GURL& url,
                                     const std::string& user_agent,
                                     const std::string& group,
                                     const std::string& type,
                                     std::unique_ptr<const base::Value> body,
                                     int depth)
    : url(url),
      user_agent(user_agent),
      group(group),
      type(type),
      body(std::move(body)),
      depth(depth) {}

TestReportingService::Report::~Report() = default;

TestReportingService::TestReportingService() = default;

TestReportingService::~TestReportingService() = default;

void TestReportingService::QueueReport(const GURL& url,
                                       const std::string& user_agent,
                                       const std::string& group,
                                       const std::string& type,
                                       std::unique_ptr<const base::Value> body,
                                       int depth) {
  reports_.push_back(
      Report(url, user_agent, group, type, std::move(body), depth));
}

void TestReportingService::ProcessHeader(const GURL& url,
                                         const std::string& header_value) {
  NOTREACHED();
}

void TestReportingService::RemoveBrowsingData(
    int data_type_mask,
    const base::RepeatingCallback<bool(const GURL&)>& origin_filter) {
  NOTREACHED();
}

void TestReportingService::RemoveAllBrowsingData(int data_type_mask) {
  NOTREACHED();
}

void TestReportingService::OnShutdown() {}

const ReportingPolicy& TestReportingService::GetPolicy() const {
  NOTREACHED();
  return dummy_policy_;
}

ReportingContext* TestReportingService::GetContextForTesting() const {
  NOTREACHED();
  return nullptr;
}

}  // namespace net
