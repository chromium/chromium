// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/reporting/reporting_test_util.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/timer/mock_timer.h"
#include "net/base/isolation_info.h"
#include "net/base/network_anonymization_key.h"
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
                    const IsolationInfo& isolation_info,
                    const std::string& json,
                    ReportingUploader::UploadCallback callback,
                    base::OnceCallback<void(PendingUpload*)> complete_callback)
      : report_origin_(report_origin),
        url_(url),
        isolation_info_(isolation_info),
        json_(json),
        callback_(std::move(callback)),
        complete_callback_(std::move(complete_callback)) {}

  ~PendingUploadImpl() override = default;

  // PendingUpload implementation:
  const url::Origin& report_origin() const override { return report_origin_; }
  const GURL& url() const override { return url_; }
  const std::string& json() const override { return json_; }
  std::optional<base::Value> GetValue() const override {
    return base::JSONReader::Read(json_);
  }

  void Complete(ReportingUploader::Outcome outcome) override {
    std::move(callback_).Run(outcome);
    // Deletes |this|.
    std::move(complete_callback_).Run(this);
  }

 private:
  url::Origin report_origin_;
  GURL url_;
  IsolationInfo isolation_info_;
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
  NOTREACHED_IN_MIGRATION();
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

void TestReportingUploader::StartUpload(const url::Origin& report_origin,
                                        const GURL& url,
                                        const IsolationInfo& isolation_info,
                                        const std::string& json,
                                        int max_depth,
                                        bool eligible_for_credentials,
                                        UploadCallback callback) {
  pending_uploads_.push_back(std::make_unique<PendingUploadImpl>(
      report_origin, url, isolation_info, json, std::move(callback),
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
    ReportingCache::PersistentReportingStore* store,
    const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints)
    : ReportingContext(policy,
                       clock,
                       tick_clock,
                       TestReportingRandIntCallback(),
                       std::make_unique<TestReportingUploader>(),
                       std::make_unique<TestReportingDelegate>(),
                       store,
                       enterprise_reporting_endpoints) {
  auto delivery_timer = std::make_unique<base::MockOneShotTimer>();
  delivery_timer_ = delivery_timer.get();
  auto garbage_collection_timer = std::make_unique<base::MockOneShotTimer>();
  garbage_collection_timer_ = garbage_collection_timer.get();
  garbage_collector()->SetTimerForTesting(std::move(garbage_collection_timer));
  delivery_agent()->SetTimerForTesting(std::move(delivery_timer));
}

TestReportingContext::~TestReportingContext() {
  delivery_timer_ = nullptr;
  garbage_collection_timer_ = nullptr;
}

ReportingTestBase::ReportingTestBase() {
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
    std::unique_ptr<ReportingCache::PersistentReportingStore> store) {
  // Must destroy old context, if there is one, before destroying old store.
  // Need to copy policy first, since the context owns it.
  ReportingPolicy policy_copy = policy();
  context_.reset();
  store_ = std::move(store);
  CreateContext(policy_copy, clock()->Now(), tick_clock()->NowTicks());
}

const ReportingEndpoint ReportingTestBase::FindEndpointInCache(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  return cache()->GetEndpointForTesting(group_key, url);
}

bool ReportingTestBase::SetEndpointInCache(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url,
    base::Time expires,
    OriginSubdomains include_subdomains,
    int priority,
    int weight) {
  cache()->SetEndpointForTesting(group_key, url, include_subdomains, expires,
                                 priority, weight);
  const ReportingEndpoint endpoint = FindEndpointInCache(group_key, url);
  return endpoint.is_valid();
}

void ReportingTestBase::SetV1EndpointInCache(
    const ReportingEndpointGroupKey& group_key,
    const base::UnguessableToken& reporting_source,
    const IsolationInfo& isolation_info,
    const GURL& url) {
  cache()->SetV1EndpointForTesting(group_key, reporting_source, isolation_info,
                                   url);
}

void ReportingTestBase::SetEnterpriseEndpointInCache(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  cache()->SetEnterpriseEndpointForTesting(group_key, url);
}

bool ReportingTestBase::EndpointExistsInCache(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  ReportingEndpoint endpoint = cache()->GetEndpointForTesting(group_key, url);
  return endpoint.is_valid();
}

ReportingEndpoint::Statistics ReportingTestBase::GetEndpointStatistics(
    const ReportingEndpointGroupKey& group_key,
    const GURL& url) {
  ReportingEndpoint endpoint;
  if (group_key.IsDocumentEndpoint()) {
    endpoint = cache()->GetV1EndpointForTesting(
        group_key.reporting_source.value(), group_key.group_name);
  } else {
    endpoint = cache()->GetEndpointForTesting(group_key, url);
  }
  if (endpoint)
    return endpoint.stats;
  return ReportingEndpoint::Statistics();
}

bool ReportingTestBase::EndpointGroupExistsInCache(
    const ReportingEndpointGroupKey& group_key,
    OriginSubdomains include_subdomains,
    base::Time expires) {
  return cache()->EndpointGroupExistsForTesting(group_key, include_subdomains,
                                                expires);
}

bool ReportingTestBase::ClientExistsInCacheForOrigin(
    const url::Origin& origin) {
  std::set<url::Origin> all_origins = cache()->GetAllOrigins();
  return all_origins.find(origin) != all_origins.end();
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
                                                    policy, store_.get());
  clock()->SetNow(now);
  tick_clock()->SetNowTicks(now_ticks);
}

base::TimeTicks ReportingTestBase::yesterday() {
  return tick_clock()->NowTicks() - base::Days(1);
}

base::TimeTicks ReportingTestBase::now() {
  return tick_clock()->NowTicks();
}

base::TimeTicks ReportingTestBase::tomorrow() {
  return tick_clock()->NowTicks() + base::Days(1);
}

TestReportingService::Report::Report() = default;

TestReportingService::Report::Report(Report&& other) = default;

TestReportingService::Report::Report(
    const GURL& url,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& user_agent,
    const std::string& group,
    const std::string& type,
    std::unique_ptr<const base::Value> body,
    int depth)
    : url(url),
      network_anonymization_key(network_anonymization_key),
      user_agent(user_agent),
      group(group),
      type(type),
      body(std::move(body)),
      depth(depth) {}

TestReportingService::Report::~Report() = default;

TestReportingService::TestReportingService() = default;

TestReportingService::~TestReportingService() = default;

void TestReportingService::QueueReport(
    const GURL& url,
    const std::optional<base::UnguessableToken>& reporting_source,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& user_agent,
    const std::string& group,
    const std::string& type,
    base::Value::Dict body,
    int depth,
    ReportingTargetType target_type) {
  reports_.emplace_back(
      Report(url, network_anonymization_key, user_agent, group, type,
             std::make_unique<base::Value>(std::move(body)), depth));
}

void TestReportingService::ProcessReportToHeader(
    const url::Origin& origin,
    const NetworkAnonymizationKey& network_anonymization_key,
    const std::string& header_value) {
  NOTREACHED_IN_MIGRATION();
}

void TestReportingService::RemoveBrowsingData(
    uint64_t data_type_mask,
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter) {
  NOTREACHED_IN_MIGRATION();
}

void TestReportingService::RemoveAllBrowsingData(uint64_t data_type_mask) {
  NOTREACHED_IN_MIGRATION();
}

void TestReportingService::OnShutdown() {}

const ReportingPolicy& TestReportingService::GetPolicy() const {
  NOTREACHED_IN_MIGRATION();
  return dummy_policy_;
}

ReportingContext* TestReportingService::GetContextForTesting() const {
  NOTREACHED_IN_MIGRATION();
  return nullptr;
}

std::vector<raw_ptr<const ReportingReport, VectorExperimental>>
TestReportingService::GetReports() const {
  NOTREACHED_IN_MIGRATION();
  return std::vector<raw_ptr<const ReportingReport, VectorExperimental>>();
}

base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
TestReportingService::GetV1ReportingEndpointsByOrigin() const {
  NOTREACHED_IN_MIGRATION();
  return base::flat_map<url::Origin, std::vector<ReportingEndpoint>>();
}

void TestReportingService::AddReportingCacheObserver(
    ReportingCacheObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

void TestReportingService::RemoveReportingCacheObserver(
    ReportingCacheObserver* observer) {
  NOTREACHED_IN_MIGRATION();
}

}  // namespace net
