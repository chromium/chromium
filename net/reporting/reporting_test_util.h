// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_TEST_UTIL_H_
#define NET_REPORTING_REPORTING_TEST_UTIL_H_

#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/unguessable_token.h"
#include "net/base/network_anonymization_key.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_service.h"
#include "net/reporting/reporting_target_type.h"
#include "net/reporting/reporting_uploader.h"
#include "net/test/test_with_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace base {
class MockOneShotTimer;
class SimpleTestClock;
class SimpleTestTickClock;
class Value;
}  // namespace base

namespace url {
class Origin;
}  // namespace url

namespace net {

class IsolationInfo;
struct ReportingEndpoint;
class ReportingGarbageCollector;

// A matcher for ReportingReports, which checks that the url of the report is
// the given url.
// Usage: EXPECT_THAT(report, ReportUrlIs(url));
// EXPECT_THAT(reports(),
//             testing::ElementsAre(ReportUrlIs(url1), ReportUrlIs(url2)));
MATCHER_P(ReportUrlIs, url, "") {
  return arg.url == url;
}

RandIntCallback TestReportingRandIntCallback();

// A test implementation of ReportingUploader that holds uploads for tests to
// examine and complete with a specified outcome.
class TestReportingUploader : public ReportingUploader {
 public:
  class PendingUpload {
   public:
    virtual ~PendingUpload();

    virtual const url::Origin& report_origin() const = 0;
    virtual const GURL& url() const = 0;
    virtual const std::string& json() const = 0;
    virtual std::optional<base::Value> GetValue() const = 0;

    virtual void Complete(Outcome outcome) = 0;

   protected:
    PendingUpload();
  };

  TestReportingUploader();

  TestReportingUploader(const TestReportingUploader&) = delete;
  TestReportingUploader& operator=(const TestReportingUploader&) = delete;

  ~TestReportingUploader() override;

  const std::vector<std::unique_ptr<PendingUpload>>& pending_uploads() const {
    return pending_uploads_;
  }

  // ReportingUploader implementation:

  void StartUpload(const url::Origin& report_origin,
                   const GURL& url,
                   const IsolationInfo& isolation_info,
                   const std::string& json,
                   int max_depth,
                   bool eligible_for_credentials,
                   UploadCallback callback) override;

  void OnShutdown() override;

  int GetPendingUploadCountForTesting() const override;

 private:
  std::vector<std::unique_ptr<PendingUpload>> pending_uploads_;
};

// Allows all permissions unless set_disallow_report_uploads is called; uses
// the real ReportingDelegate for JSON parsing to exercise depth and size
// limits.
class TestReportingDelegate : public ReportingDelegate {
 public:
  TestReportingDelegate();

  TestReportingDelegate(const TestReportingDelegate&) = delete;
  TestReportingDelegate& operator=(const TestReportingDelegate&) = delete;

  // ReportingDelegate implementation:

  ~TestReportingDelegate() override;

  void set_disallow_report_uploads(bool disallow_report_uploads) {
    disallow_report_uploads_ = disallow_report_uploads;
  }

  void set_pause_permissions_check(bool pause_permissions_check) {
    pause_permissions_check_ = pause_permissions_check;
  }

  bool CanQueueReport(const url::Origin& origin) const override;

  void CanSendReports(std::set<url::Origin> origins,
                      base::OnceCallback<void(std::set<url::Origin>)>
                          result_callback) const override;

  bool PermissionsCheckPaused() const;
  void ResumePermissionsCheck();

  bool CanSetClient(const url::Origin& origin,
                    const GURL& endpoint) const override;

  bool CanUseClient(const url::Origin& origin,
                    const GURL& endpoint) const override;

 private:
  bool disallow_report_uploads_ = false;
  bool pause_permissions_check_ = false;

  mutable std::set<url::Origin> saved_origins_;
  mutable base::OnceCallback<void(std::set<url::Origin>)>
      permissions_check_callback_;
};

// A test implementation of ReportingContext that uses test versions of
// Clock, TickClock, Timer, and ReportingUploader.
class TestReportingContext : public ReportingContext {
 public:
  TestReportingContext(
      base::Clock* clock,
      const base::TickClock* tick_clock,
      const ReportingPolicy& policy,
      ReportingCache::PersistentReportingStore* store = nullptr,
      const base::flat_map<std::string, GURL>& enterprise_reporting_endpoints =
          {});

  TestReportingContext(const TestReportingContext&) = delete;
  TestReportingContext& operator=(const TestReportingContext&) = delete;

  ~TestReportingContext() override;

  base::MockOneShotTimer* test_delivery_timer() { return delivery_timer_; }
  base::MockOneShotTimer* test_garbage_collection_timer() {
    return garbage_collection_timer_;
  }
  TestReportingUploader* test_uploader() {
    return reinterpret_cast<TestReportingUploader*>(uploader());
  }
  TestReportingDelegate* test_delegate() {
    return reinterpret_cast<TestReportingDelegate*>(delegate());
  }

 private:
  // Owned by the DeliveryAgent and GarbageCollector, respectively, but
  // referenced here to preserve type:

  raw_ptr<base::MockOneShotTimer> delivery_timer_;
  raw_ptr<base::MockOneShotTimer> garbage_collection_timer_;
};

// A unit test base class that provides a TestReportingContext and shorthand
// getters.
class ReportingTestBase : public TestWithTaskEnvironment {
 public:
  ReportingTestBase(const ReportingTestBase&) = delete;
  ReportingTestBase& operator=(const ReportingTestBase&) = delete;

 protected:
  ReportingTestBase();
  ~ReportingTestBase() override;

  void UsePolicy(const ReportingPolicy& policy);
  void UseStore(
      std::unique_ptr<ReportingCache::PersistentReportingStore> store);

  // Finds a particular endpoint in the cache and returns it (or an invalid
  // ReportingEndpoint, if not found).
  const ReportingEndpoint FindEndpointInCache(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url);

  // Sets an endpoint with the given properties in a group with the given
  // properties, bypassing header parsing. Note that the endpoint is not
  // guaranteed to exist in the cache after calling this function, if endpoint
  // eviction is triggered. Returns whether the endpoint was successfully set.
  bool SetEndpointInCache(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url,
      base::Time expires,
      OriginSubdomains include_subdomains = OriginSubdomains::DEFAULT,
      int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority,
      int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight);

  // Sets an endpoint with the given group_key and url as origin in the document
  // endpoints map using |reporting_source| as key.
  void SetV1EndpointInCache(const ReportingEndpointGroupKey& group_key,
                            const base::UnguessableToken& reporting_source,
                            const IsolationInfo& isolation_info,
                            const GURL& url);

  // Sets an enterprise endpoint with the given group_key and url as origin in
  // the enterprise endpoints vector.
  void SetEnterpriseEndpointInCache(const ReportingEndpointGroupKey& group_key,
                                    const GURL& url);

  // Returns whether an endpoint with the given properties exists in the cache.
  bool EndpointExistsInCache(const ReportingEndpointGroupKey& group_key,
                             const GURL& url);

  // Gets the statistics for a given endpoint, if it exists.
  ReportingEndpoint::Statistics GetEndpointStatistics(
      const ReportingEndpointGroupKey& group_key,
      const GURL& url);

  // Returns whether an endpoint group with exactly the given properties exists
  // in the cache. |expires| can be omitted, in which case it will not be
  // checked.
  bool EndpointGroupExistsInCache(const ReportingEndpointGroupKey& group_key,
                                  OriginSubdomains include_subdomains,
                                  base::Time expires = base::Time());

  // Returns whether a client for the given origin exists in the cache.
  bool ClientExistsInCacheForOrigin(const url::Origin& origin);

  // Makes a unique URL with the provided index.
  GURL MakeURL(size_t index);

  // Simulates an embedder restart, preserving the ReportingPolicy.
  //
  // Advances the Clock by |delta|, and the TickClock by |delta_ticks|. Both can
  // be zero or negative.
  void SimulateRestart(base::TimeDelta delta, base::TimeDelta delta_ticks);

  TestReportingContext* context() { return context_.get(); }

  const ReportingPolicy& policy() { return context_->policy(); }

  base::SimpleTestClock* clock() { return &clock_; }
  base::SimpleTestTickClock* tick_clock() { return &tick_clock_; }
  base::MockOneShotTimer* delivery_timer() {
    return context_->test_delivery_timer();
  }
  base::MockOneShotTimer* garbage_collection_timer() {
    return context_->test_garbage_collection_timer();
  }
  TestReportingUploader* uploader() { return context_->test_uploader(); }

  ReportingCache* cache() { return context_->cache(); }
  ReportingDeliveryAgent* delivery_agent() {
    return context_->delivery_agent();
  }
  ReportingGarbageCollector* garbage_collector() {
    return context_->garbage_collector();
  }
  ReportingCache::PersistentReportingStore* store() { return store_.get(); }

  base::TimeTicks yesterday();
  base::TimeTicks now();
  base::TimeTicks tomorrow();

  const std::vector<std::unique_ptr<TestReportingUploader::PendingUpload>>&
  pending_uploads() {
    return uploader()->pending_uploads();
  }

 private:
  void CreateContext(const ReportingPolicy& policy,
                     base::Time now,
                     base::TimeTicks now_ticks);

  base::SimpleTestClock clock_;
  base::SimpleTestTickClock tick_clock_;
  std::unique_ptr<ReportingCache::PersistentReportingStore> store_;
  std::unique_ptr<TestReportingContext> context_;
};

class TestReportingService : public ReportingService {
 public:
  struct Report {
    Report();

    Report(const Report&) = delete;

    Report(Report&& other);

    Report(const GURL& url,
           const NetworkAnonymizationKey& network_anonymization_key,
           const std::string& user_agent,
           const std::string& group,
           const std::string& type,
           std::unique_ptr<const base::Value> body,
           int depth);

    ~Report();

    GURL url;
    NetworkAnonymizationKey network_anonymization_key;
    std::string user_agent;
    std::string group;
    std::string type;
    std::unique_ptr<const base::Value> body;
    int depth;
  };

  TestReportingService();

  TestReportingService(const TestReportingService&) = delete;
  TestReportingService& operator=(const TestReportingService&) = delete;

  const std::vector<Report>& reports() const { return reports_; }

  // ReportingService implementation:

  ~TestReportingService() override;

  void SetDocumentReportingEndpoints(
      const base::UnguessableToken& reporting_source,
      const url::Origin& origin,
      const IsolationInfo& isolation_info,
      const base::flat_map<std::string, std::string>& endpoints) override {}

  void SetEnterpriseReportingEndpoints(
      const base::flat_map<std::string, GURL>& endpoints) override {}

  void SendReportsAndRemoveSource(
      const base::UnguessableToken& reporting_source) override {}

  void QueueReport(
      const GURL& url,
      const std::optional<base::UnguessableToken>& reporting_source,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& user_agent,
      const std::string& group,
      const std::string& type,
      base::Value::Dict body,
      int depth,
      ReportingTargetType target_type) override;

  void ProcessReportToHeader(
      const url::Origin& url,
      const NetworkAnonymizationKey& network_anonymization_key,
      const std::string& header_value) override;

  void RemoveBrowsingData(
      uint64_t data_type_mask,
      const base::RepeatingCallback<bool(const url::Origin&)>& origin_filter)
      override;

  void RemoveAllBrowsingData(uint64_t data_type_mask) override;

  void OnShutdown() override;

  const ReportingPolicy& GetPolicy() const override;

  ReportingContext* GetContextForTesting() const override;

  std::vector<raw_ptr<const ReportingReport, VectorExperimental>> GetReports()
      const override;
  base::flat_map<url::Origin, std::vector<ReportingEndpoint>>
  GetV1ReportingEndpointsByOrigin() const override;
  void AddReportingCacheObserver(ReportingCacheObserver* observer) override;
  void RemoveReportingCacheObserver(ReportingCacheObserver* observer) override;

 private:
  std::vector<Report> reports_;
  ReportingPolicy dummy_policy_;
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_TEST_UTIL_H_
