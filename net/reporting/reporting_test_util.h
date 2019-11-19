// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_REPORTING_REPORTING_TEST_UTIL_H_
#define NET_REPORTING_REPORTING_TEST_UTIL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/test/simple_test_clock.h"
#include "base/test/simple_test_tick_clock.h"
#include "net/base/rand_callback.h"
#include "net/reporting/reporting_cache.h"
#include "net/reporting/reporting_context.h"
#include "net/reporting/reporting_delegate.h"
#include "net/reporting/reporting_service.h"
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

class NetworkIsolationKey;
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
    virtual std::unique_ptr<base::Value> GetValue() const = 0;

    virtual void Complete(Outcome outcome) = 0;

   protected:
    PendingUpload();
  };

  TestReportingUploader();
  ~TestReportingUploader() override;

  const std::vector<std::unique_ptr<PendingUpload>>& pending_uploads() const {
    return pending_uploads_;
  }

  // ReportingUploader implementation:

  void StartUpload(const url::Origin& report_origin,
                   const GURL& url,
                   const NetworkIsolationKey& network_isolation_key,
                   const std::string& json,
                   int max_depth,
                   UploadCallback callback) override;

  void OnShutdown() override;

  int GetPendingUploadCountForTesting() const override;

 private:
  std::vector<std::unique_ptr<PendingUpload>> pending_uploads_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingUploader);
};

// Allows all permissions unless set_disallow_report_uploads is called; uses
// the real ReportingDelegate for JSON parsing to exercise depth and size
// limits.
class TestReportingDelegate : public ReportingDelegate {
 public:
  TestReportingDelegate();

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

  DISALLOW_COPY_AND_ASSIGN(TestReportingDelegate);
};

// A test implementation of ReportingContext that uses test versions of
// Clock, TickClock, Timer, and ReportingUploader.
class TestReportingContext : public ReportingContext {
 public:
  TestReportingContext(
      base::Clock* clock,
      const base::TickClock* tick_clock,
      const ReportingPolicy& policy,
      ReportingCache::PersistentReportingStore* store = nullptr);
  ~TestReportingContext();

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

  base::MockOneShotTimer* delivery_timer_;
  base::MockOneShotTimer* garbage_collection_timer_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingContext);
};

// A unit test base class that provides a TestReportingContext and shorthand
// getters.
class ReportingTestBase : public TestWithTaskEnvironment {
 protected:
  ReportingTestBase();
  ~ReportingTestBase() override;

  void UsePolicy(const ReportingPolicy& policy);
  void UseStore(ReportingCache::PersistentReportingStore* store);

  // Finds a particular endpoint (by origin, group, url) in the cache and
  // returns it (or an invalid ReportingEndpoint, if not found).
  const ReportingEndpoint FindEndpointInCache(const url::Origin& origin,
                                              const std::string& group_name,
                                              const GURL& url);

  // Sets an endpoint with the given properties in a group with the given
  // properties, bypassing header parsing. Note that the endpoint is not
  // guaranteed to exist in the cache after calling this function, if endpoint
  // eviction is triggered. Returns whether the endpoint was successfully set.
  bool SetEndpointInCache(
      const url::Origin& origin,
      const std::string& group_name,
      const GURL& url,
      base::Time expires,
      OriginSubdomains include_subdomains = OriginSubdomains::DEFAULT,
      int priority = ReportingEndpoint::EndpointInfo::kDefaultPriority,
      int weight = ReportingEndpoint::EndpointInfo::kDefaultWeight);

  // Returns whether an endpoint with the given properties exists in the cache.
  bool EndpointExistsInCache(const url::Origin& origin,
                             const std::string& group_name,
                             const GURL& url);

  // Gets the statistics for a given endpoint, if it exists.
  ReportingEndpoint::Statistics GetEndpointStatistics(
      const url::Origin& origin,
      const std::string& group_name,
      const GURL& url);

  // Returns whether an endpoint group with exactly the given properties exists
  // in the cache. |expires| can be omitted, in which case it will not be
  // checked.
  bool EndpointGroupExistsInCache(const url::Origin& origin,
                                  const std::string& group_name,
                                  OriginSubdomains include_subdomains,
                                  base::Time expires = base::Time());

  // Returns whether a client for the given origin exists in the cache.
  bool OriginClientExistsInCache(const url::Origin& origin);

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
  ReportingCache::PersistentReportingStore* store() { return store_; }

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
  std::unique_ptr<TestReportingContext> context_;
  ReportingCache::PersistentReportingStore* store_;

  DISALLOW_COPY_AND_ASSIGN(ReportingTestBase);
};

class TestReportingService : public ReportingService {
 public:
  struct Report {
    Report();

    Report(Report&& other);

    Report(const GURL& url,
           const std::string& user_agent,
           const std::string& group,
           const std::string& type,
           std::unique_ptr<const base::Value> body,
           int depth);

    ~Report();

    GURL url;
    std::string user_agent;
    std::string group;
    std::string type;
    std::unique_ptr<const base::Value> body;
    int depth;

   private:
    DISALLOW_COPY(Report);
  };

  TestReportingService();

  const std::vector<Report>& reports() const { return reports_; }

  // ReportingService implementation:

  ~TestReportingService() override;

  void QueueReport(const GURL& url,
                   const std::string& user_agent,
                   const std::string& group,
                   const std::string& type,
                   std::unique_ptr<const base::Value> body,
                   int depth) override;

  void ProcessHeader(const GURL& url, const std::string& header_value) override;

  void RemoveBrowsingData(
      int data_type_mask,
      const base::RepeatingCallback<bool(const GURL&)>& origin_filter) override;

  void RemoveAllBrowsingData(int data_type_mask) override;

  void OnShutdown() override;

  const ReportingPolicy& GetPolicy() const override;

  ReportingContext* GetContextForTesting() const override;

 private:
  std::vector<Report> reports_;
  ReportingPolicy dummy_policy_;

  DISALLOW_COPY_AND_ASSIGN(TestReportingService);
};

}  // namespace net

#endif  // NET_REPORTING_REPORTING_TEST_UTIL_H_
