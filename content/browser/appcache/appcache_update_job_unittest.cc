// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/appcache/appcache_update_job.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/appcache/appcache_cache_test_helper.h"
#include "content/browser/appcache/appcache_disk_cache_ops.h"
#include "content/browser/appcache/appcache_group.h"
#include "content/browser/appcache/appcache_host.h"
#include "content/browser/appcache/appcache_response_info.h"
#include "content/browser/appcache/appcache_update_url_loader_request.h"
#include "content/browser/appcache/mock_appcache_service.h"
#include "content/browser/appcache/test_origin_trial_policy.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/url_loader_factory_getter.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/origin_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/url_loader_interceptor.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_util.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/origin_trials/origin_trial_policy.h"
#include "third_party/blink/public/common/origin_trials/trial_token.h"
#include "third_party/blink/public/common/origin_trials/trial_token_validator.h"
#include "third_party/blink/public/mojom/appcache/appcache.mojom.h"
#include "third_party/blink/public/mojom/appcache/appcache_info.mojom.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"

namespace {
// tools/origin_trials/generate_token.py http://mockhost AppCache
// --expire-days=2000
#define APPCACHE_ORIGIN_TRIAL_TOKEN                                            \
  "AhiiB7vi3JiEO1/"                                                            \
  "RQIytQslLSN3WYVu3Xd32abYhTia+91ladjnXSClfU981x+"                            \
  "aoPimEqYVy6tWoeMZZYTpqlggAAABNeyJvcmlnaW4iOiAiaHR0cDovL21vY2tob3N0OjgwIiwg" \
  "ImZlYXR1cmUiOiAiQXBwQ2FjaGUiLCAiZXhwaXJ5IjogMTc2MTE2NjQxOH0="

const char* kTestAppCacheOriginTrialToken = APPCACHE_ORIGIN_TRIAL_TOKEN;

}  // namespace

namespace content {
static TestOriginTrialPolicy g_origin_trial_policy;

namespace appcache_update_job_unittest {

class AppCacheUpdateJobTest;

// Values should match values used in appcache_update_job.cc.
const base::TimeDelta kFullUpdateInterval = base::TimeDelta::FromHours(24);
const base::TimeDelta kMaxEvictableErrorDuration =
    base::TimeDelta::FromDays(14);
const base::TimeDelta kOneHour = base::TimeDelta::FromHours(1);
const base::TimeDelta kOneYear = base::TimeDelta::FromDays(365);

const char kManifest1Contents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "NETWORK:\n"
    "*\n";

const char kManifest1OriginTrialContents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "ORIGIN-TRIAL:\n" APPCACHE_ORIGIN_TRIAL_TOKEN
    "\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "NETWORK:\n"
    "*\n";

const char kManifest1WithNotModifiedContents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "NETWORK:\n"
    "*\n"
    "CACHE:\n"
    "notmodified\n";

const char kManifest1WithMaybeModifiedContents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "NETWORK:\n"
    "*\n"
    "CACHE:\n"
    "maybemodified\n";

const char kExplicit1Contents[] = "explicit1";

// By default, kManifest2Contents is served from a path in /files/, so any
// resource listing in it that references outside of that path will require a
// scope override to be stored.
const char kManifest2Contents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "CHROMIUM-INTERCEPT:\n"
    "intercept1 return intercept1a\n"
    "/bar/intercept2 return intercept2a\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "/bar/fallback2 fallback2a\n"
    "NETWORK:\n"
    "*\n";

const char kManifest2OriginTrialContents[] =
    "CACHE MANIFEST\n"
    "explicit1\n"
    "CHROMIUM-INTERCEPT:\n"
    "intercept1 return intercept1a\n"
    "/bar/intercept2 return intercept2a\n"
    "ORIGIN-TRIAL:\n" APPCACHE_ORIGIN_TRIAL_TOKEN
    "\n"
    "FALLBACK:\n"
    "fallback1 fallback1a\n"
    "/bar/fallback2 fallback2a\n"
    "NETWORK:\n"
    "*\n";

const char kScopeManifestContents[] =
    "CACHE MANIFEST\n"
    "CHROMIUM-INTERCEPT:\n"
    "/bar/foo return /intercept-newbar/foo\n"
    "/bar/baz/foo return /intercept-newbar/newbaz/foo\n"
    "/other/foo return /intercept-newother/foo\n"
    "/ return /intercept-newroot/foo\n"
    "FALLBACK:\n"
    "/bar/foo /fallback-newbar/foo\n"
    "/bar/baz/foo /fallback-newbar/newbaz/foo\n"
    "/other/foo /fallback-newother/foo\n"
    "/ /fallback-newroot/foo\n";

// There are a handful of http accessible resources that we need to conduct
// these tests. Instead of running a separate server to host these resources,
// we mock them up.
class MockHttpServer {
 public:
  static url::Origin GetOrigin() {
    return url::Origin::Create(GURL("http://mockhost"));
  }

  static GURL GetMockUrl(const std::string& path) {
    return GURL("http://mockhost/" + path);
  }

  static GURL GetMockHttpsUrl(const std::string& path) {
    return GURL("https://mockhost/" + path);
  }

  static GURL GetMockCrossOriginHttpsUrl(const std::string& path) {
    return GURL("https://cross_origin_host/" + path);
  }

  static std::string GetManifestHeaders(const bool ok,
                                        const std::string& scope) {
    std::string headers;
    if (ok)
      headers += "HTTP/1.1 200 OK\n";
    else
      headers += "HTTP/1.1 304 NOT MODIFIED\n";
    headers += "Content-type: text/cache-manifest\n";
    if (!scope.empty())
      headers += "X-AppCache-Allowed: " + scope + "\n";
    headers += "\n";
    return headers;
  }

  static void GetMockResponse(const std::string& path,
                              std::string* headers,
                              std::string* body) {
    const char ok_headers[] =
        "HTTP/1.1 200 OK\n"
        "\n";
    const char error_headers[] =
        "HTTP/1.1 500 BOO HOO\n"
        "\n";
    const char manifest_headers[] =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/cache-manifest\n"
        "\n";
    const char not_modified_headers[] =
        "HTTP/1.1 304 NOT MODIFIED\n"
        "X-AppCache-Allowed: /\n"
        "\n";
    const char gone_headers[] =
        "HTTP/1.1 410 GONE\n"
        "\n";
    const char not_found_headers[] =
        "HTTP/1.1 404 NOT FOUND\n"
        "\n";
    const char no_store_headers[] =
        "HTTP/1.1 200 OK\n"
        "Cache-Control: no-store\n"
        "\n";

    if (path == "/files/missing-mime-manifest") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "CACHE MANIFEST\n";
    } else if (path == "/files/bad-manifest") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = "BAD CACHE MANIFEST";
    } else if (path == "/files/empty1") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "";
    } else if (path == "/files/empty-file-manifest") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "empty1\n";
    } else if (path == "/files/empty-manifest") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = "CACHE MANIFEST\n";
    } else if (path == "/files/explicit1") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = kExplicit1Contents;
    } else if (path == "/files/explicit2") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "explicit2";
    } else if (path == "/files/fallback1a") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "fallback1a";
    } else if (path == "/files/fallback2a") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "fallback2a";
    } else if (path == "/files/intercept1a") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "intercept1a";
    } else if (path == "/files/intercept2a") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "intercept2a";
    } else if (path == "/files/gone") {
      (*headers) = std::string(gone_headers, base::size(gone_headers));
      (*body) = "";
    } else if (path == "/files/manifest1") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = kManifest1Contents;
    } else if (path == "/files/manifest2") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = kManifest2Contents;
    } else if (path == "/files/manifest2-origin-trial") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = kManifest2OriginTrialContents;
    } else if (path == "/files/manifest2-with-root-override") {
      (*headers) = GetManifestHeaders(/*ok=*/true, /*scope=*/"/");
      (*body) = kManifest2Contents;
    } else if (path == "/files/manifest1-with-notmodified") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = kManifest1WithNotModifiedContents;
    } else if (path == "/files/manifest1-with-maybemodified") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) = kManifest1WithMaybeModifiedContents;
    } else if (path == "/files/manifest-fb-404") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "explicit1\n"
          "FALLBACK:\n"
          "fallback1 fallback1a\n"
          "fallback404 fallback-404\n"
          "NETWORK:\n"
          "online1\n";
    } else if (path == "/files/manifest-merged-types") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "explicit1\n"
          "# manifest is also an explicit entry\n"
          "manifest-merged-types\n"
          "FALLBACK:\n"
          "# fallback is also explicit entry\n"
          "fallback1 explicit1\n"
          "NETWORK:\n"
          "online1\n";
    } else if (path == "/files/manifest-with-404") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "explicit-404\n"
          "explicit1\n"
          "explicit2\n"
          "explicit3\n"
          "FALLBACK:\n"
          "fallback1 fallback1a\n"
          "NETWORK:\n"
          "online1\n";
    } else if (path == "/files/manifest-with-intercept") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "CHROMIUM-INTERCEPT:\n"
          "intercept1 return intercept1a\n";
    } else if (path == "/files/maybemodified") {
      (*headers) =
          std::string(not_modified_headers, base::size(not_modified_headers));
      (*body) = "maybemodified";
    } else if (path == "/files/modified") {
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "modified";
    } else if (path == "/files/notmodified") {
      (*headers) =
          std::string(not_modified_headers, base::size(not_modified_headers));
      (*body) = "";
    } else if (path == "/files/servererror") {
      (*headers) = std::string(error_headers, base::size(error_headers));
      (*body) = "error";
    } else if (path == "/files/valid_cross_origin_https_manifest") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "https://cross_origin_host/files/explicit1\n";
    } else if (path == "/files/invalid_cross_origin_https_manifest") {
      (*headers) = std::string(manifest_headers, base::size(manifest_headers));
      (*body) =
          "CACHE MANIFEST\n"
          "https://cross_origin_host/files/no-store-headers\n";
    } else if (path == "/files/no-store-headers") {
      (*headers) = std::string(no_store_headers, base::size(no_store_headers));
      (*body) = "no-store";
    } else if (path == "/intercept-newbar/foo" ||
               path == "/intercept-newbar/newbaz/foo" ||
               path == "/intercept-newother/foo" ||
               path == "/intercept-newroot/foo" ||
               path == "/fallback-newbar/foo" ||
               path == "/fallback-newbar/newbaz/foo" ||
               path == "/fallback-newother/foo" ||
               path == "/fallback-newroot/foo") {
      // Store mock responses for fallback resources needed by
      // kScopeManifestContents.
      (*headers) = std::string(ok_headers, base::size(ok_headers));
      (*body) = "intercept/fallback contents";
    } else if (path == "/manifest-no-override" ||
               path == "/bar/manifest-no-override") {
      (*headers) = GetManifestHeaders(/*ok=*/true, /*scope=*/"");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-root-override" ||
               path == "/bar/manifest-root-override") {
      (*headers) = GetManifestHeaders(/*ok=*/true, /*scope=*/"/");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-bar-override" ||
               path == "/bar/manifest-bar-override") {
      (*headers) = GetManifestHeaders(/*ok=*/true, /*scope=*/"/bar/");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-other-override" ||
               path == "/bar/manifest-other-override") {
      (*headers) = GetManifestHeaders(/*ok=*/true, /*scope=*/"/other/");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-304-no-override" ||
               path == "/bar/manifest-304-no-override") {
      (*headers) = GetManifestHeaders(/*ok=*/false, /*scope=*/"");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-304-root-override" ||
               path == "/bar/manifest-304-root-override") {
      (*headers) = GetManifestHeaders(/*ok=*/false, /*scope=*/"/");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-304-bar-override" ||
               path == "/bar/manifest-304-bar-override") {
      (*headers) = GetManifestHeaders(/*ok=*/false, /*scope=*/"/bar/");
      (*body) = kScopeManifestContents;
    } else if (path == "/manifest-304-other-override" ||
               path == "/bar/manifest-304-other-override") {
      (*headers) = GetManifestHeaders(/*ok=*/false, /*scope=*/"/other/");
      (*body) = kScopeManifestContents;
    } else {
      (*headers) =
          std::string(not_found_headers, base::size(not_found_headers));
      (*body) = "";
    }
  }
};

inline bool operator==(const AppCacheNamespace& lhs,
                       const AppCacheNamespace& rhs) {
  return lhs.type == rhs.type && lhs.namespace_url == rhs.namespace_url &&
         lhs.target_url == rhs.target_url;
}

class MockFrontend : public blink::mojom::AppCacheFrontend {
 public:
  MockFrontend()
      : ignore_progress_events_(false),
        verify_progress_events_(false),
        last_progress_total_(-1),
        last_progress_complete_(-1),
        start_update_trigger_(
            blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT),
        update_(nullptr) {}

  void CacheSelected(blink::mojom::AppCacheInfoPtr info) override {}

  void EventRaised(blink::mojom::AppCacheEventID event_id) override {
    raised_events_.push_back(event_id);

    // Trigger additional updates if requested.
    if (event_id == start_update_trigger_ && update_) {
      for (AppCacheHost* host : update_hosts_) {
        update_->StartUpdate(
            host, (host ? host->pending_master_entry_url() : GURL()));
      }
      update_hosts_.clear();  // only trigger once
    }
  }

  void ErrorEventRaised(
      blink::mojom::AppCacheErrorDetailsPtr details) override {
    error_message_ = details->message;
    EventRaised(blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);
  }

  void ProgressEventRaised(const GURL& url,
                           int32_t num_total,
                           int32_t num_complete) override {
    if (!ignore_progress_events_)
      EventRaised(blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);

    if (verify_progress_events_) {
      EXPECT_GE(num_total, num_complete);
      EXPECT_GE(num_complete, 0);

      if (last_progress_total_ == -1) {
        // Should start at zero.
        EXPECT_EQ(0, num_complete);
      } else {
        // Total should be stable and complete should bump up by one at a time.
        EXPECT_EQ(last_progress_total_, num_total);
        EXPECT_EQ(last_progress_complete_ + 1, num_complete);
      }

      // Url should be valid for all except the 'final' event.
      if (num_total == num_complete)
        EXPECT_TRUE(url.is_empty());
      else
        EXPECT_TRUE(url.is_valid());

      last_progress_total_ = num_total;
      last_progress_complete_ = num_complete;
    }
  }

  void LogMessage(blink::mojom::ConsoleMessageLevel log_level,
                  const std::string& message) override {}

  void SetSubresourceFactory(
      mojo::PendingRemote<network::mojom::URLLoaderFactory> url_loader_factory)
      override {}

  void AddExpectedEvent(blink::mojom::AppCacheEventID event_id) {
    DCHECK(!ignore_progress_events_ ||
           event_id != blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    expected_events_.push_back(event_id);
  }

  void SetIgnoreProgressEvents(bool ignore) {
    // Some tests involve joining new hosts to an already running update job
    // or intentionally failing. The timing and sequencing of the progress
    // events generated by an update job are dependent on the behavior of
    // an external HTTP server. For jobs that do not run fully till completion,
    // due to either joining late or early exit, we skip monitoring the
    // progress events to avoid flakiness.
    ignore_progress_events_ = ignore;
  }

  void SetVerifyProgressEvents(bool verify) {
    verify_progress_events_ = verify;
  }

  void TriggerAdditionalUpdates(blink::mojom::AppCacheEventID trigger_event,
                                AppCacheUpdateJob* update) {
    start_update_trigger_ = trigger_event;
    update_ = update;
  }

  void AdditionalUpdateHost(AppCacheHost* host) {
    update_hosts_.push_back(host);
  }

  using RaisedEvents = std::vector<blink::mojom::AppCacheEventID>;
  RaisedEvents raised_events_;
  std::string error_message_;

  // Set the expected events if verification needs to happen asynchronously.
  RaisedEvents expected_events_;
  std::string expected_error_message_;

  bool ignore_progress_events_;

  bool verify_progress_events_;
  int last_progress_total_;
  int last_progress_complete_;

  // Add ability for frontend to add master entries to an inprogress update.
  blink::mojom::AppCacheEventID start_update_trigger_;
  AppCacheUpdateJob* update_;
  std::vector<AppCacheHost*> update_hosts_;
};

// Helper class to simulate a URL that returns retry or success.
class RetryRequestTestJob {
 public:
  enum RetryHeader {
    NO_RETRY_AFTER,
    NONZERO_RETRY_AFTER,
    RETRY_AFTER_0,
  };

  static constexpr char kRetryUrl[] = "http://retry/";

  // Call this at the start of each retry test.
  static void Initialize(int num_retry_responses,
                         RetryHeader header,
                         int expected_requests) {
    num_requests_ = 0;
    num_retries_ = num_retry_responses;
    retry_after_ = header;
    expected_requests_ = expected_requests;
  }

  // Verifies results at end of test and resets counters.
  static void Verify() {
    EXPECT_EQ(expected_requests_, num_requests_);
    num_requests_ = 0;
    expected_requests_ = 0;
  }

  static void GetResponseForURL(const GURL& url,
                                std::string* headers,
                                std::string* data) {
    ++num_requests_;
    if (num_retries_ > 0 && IsRetryUrl(url)) {
      --num_retries_;
      *headers = RetryRequestTestJob::retry_headers();
    } else {
      *headers = RetryRequestTestJob::manifest_headers();
    }
    if (data)
      *data = RetryRequestTestJob::data();
  }

  static bool IsRetryUrl(const GURL& url) { return url.spec() == kRetryUrl; }

 private:
  static std::string retry_headers() {
    const char no_retry_after[] =
        "HTTP/1.1 503 BOO HOO\n"
        "\n";
    const char nonzero[] =
        "HTTP/1.1 503 BOO HOO\n"
        "Retry-After: 60\n"
        "\n";
    const char retry_after_0[] =
        "HTTP/1.1 503 BOO HOO\n"
        "Retry-After: 0\n"
        "\n";

    switch (retry_after_) {
      case NO_RETRY_AFTER:
        return std::string(no_retry_after, base::size(no_retry_after));
      case NONZERO_RETRY_AFTER:
        return std::string(nonzero, base::size(nonzero));
      case RETRY_AFTER_0:
      default:
        return std::string(retry_after_0, base::size(retry_after_0));
    }
  }

  static std::string manifest_headers() {
    const char headers[] =
        "HTTP/1.1 200 OK\n"
        "Content-type: text/cache-manifest\n"
        "\n";
    return std::string(headers, base::size(headers));
  }

  static std::string data() {
    return base::StrCat({"CACHE MANIFEST\r", kRetryUrl, "\r"});
  }

  static int num_requests_;
  static int num_retries_;
  static RetryHeader retry_after_;
  static int expected_requests_;
};

// static
constexpr char RetryRequestTestJob::kRetryUrl[];
int RetryRequestTestJob::num_requests_ = 0;
int RetryRequestTestJob::num_retries_;
RetryRequestTestJob::RetryHeader RetryRequestTestJob::retry_after_;
int RetryRequestTestJob::expected_requests_ = 0;

// Helper class to check for certain HTTP headers.
class HttpHeadersRequestTestJob {
 public:
  HttpHeadersRequestTestJob() {
    saw_if_modified_since_ = false;
    saw_if_none_match_ = false;
    headers_allowed_ = true;
    saw_headers_ = false;
    already_checked_ = false;
  }

  HttpHeadersRequestTestJob(const std::string& expect_if_modified_since,
                            const std::string& expect_if_none_match,
                            bool headers_allowed = true)
      : expect_if_modified_since_(expect_if_modified_since),
        saw_if_modified_since_(false),
        expect_if_none_match_(expect_if_none_match),
        saw_if_none_match_(false),
        headers_allowed_(headers_allowed),
        saw_headers_(false),
        already_checked_(false),
        verify_called_(false) {}

  ~HttpHeadersRequestTestJob() { DCHECK(verify_called_); }

  // Verifies results at end of test.
  void Verify(const GURL& url) {
    verify_called_ = true;
    if (!expect_if_modified_since_.empty())
      EXPECT_TRUE(saw_if_modified_since_) << "URL " << url.spec() << " failed";
    if (!expect_if_none_match_.empty())
      EXPECT_TRUE(saw_if_none_match_) << "URL " << url.spec() << " failed";
    if (!headers_allowed_)
      EXPECT_FALSE(saw_headers_) << "URL " << url.spec() << " failed";
  }

  void ValidateExtraHeaders(const net::HttpRequestHeaders& extra_headers) {
    if (already_checked_)
      return;

    already_checked_ = true;  // only check once for a test

    std::string header_value;

    if (extra_headers.GetHeader(net::HttpRequestHeaders::kIfModifiedSince,
                                &header_value)) {
      saw_headers_ = true;
      saw_if_modified_since_ = header_value == expect_if_modified_since_;
    }

    if (extra_headers.GetHeader(net::HttpRequestHeaders::kIfNoneMatch,
                                &header_value)) {
      saw_headers_ = true;
      saw_if_none_match_ = header_value == expect_if_none_match_;
    }
  }

 private:
  std::string expect_if_modified_since_;
  bool saw_if_modified_since_;
  std::string expect_if_none_match_;
  bool saw_if_none_match_;
  bool headers_allowed_;
  bool saw_headers_;
  bool already_checked_;
  bool verify_called_;
};

class AppCacheUpdateJobTest : public testing::Test,
                              public AppCacheGroup::UpdateObserver {
 public:
  AppCacheUpdateJobTest()
      : do_checks_after_update_finished_(false),
        expect_group_obsolete_(false),
        expect_group_has_cache_(false),
        expect_group_is_being_deleted_(false),
        expect_evictable_error_(false),
        expect_eviction_(false),
        expect_old_cache_(nullptr),
        expect_newest_cache_(nullptr),
        expect_non_null_update_time_(false),
        tested_manifest_(NONE),
        tested_manifest_path_override_(nullptr),
        interceptor_(
            base::BindRepeating(&AppCacheUpdateJobTest::InterceptRequest,
                                base::Unretained(this))),
        process_id_(123) {
    appcache_require_origin_trial_feature_.InitAndDisableFeature(
        blink::features::kAppCacheRequireOriginTrial);
  }

  // TODO(ananta/michaeln): Remove dependencies on URLRequest based
  // classes by refactoring the response headers/data into a common class.
  bool InterceptRequest(URLLoaderInterceptor::RequestParams* params) {
    const auto& url_request = params->url_request;
    if (url_request.url.host() == "failme" ||
        url_request.url.host() == "testme") {
      params->client->OnComplete(network::URLLoaderCompletionStatus(-100));
      return true;
    }

    auto it = http_headers_request_test_jobs_.find(url_request.url);
    if (it != http_headers_request_test_jobs_.end())
      it->second->ValidateExtraHeaders(url_request.headers);

    // Copy the URL so we can optionally override it under certain conditions.
    GURL requested_url = url_request.url;

    // If "files/maybemodified" is requested without a conditional header,
    // treat the request as if it's a request for "files/modified".  The
    // result will be a 200 OK response (rather than maybemodified's normal
    // 304 response).
    if (requested_url.path() == "/files/maybemodified" &&
        !url_request.headers.HasHeader("If-Modified-Since")) {
      requested_url = GURL("http://mockhost/files/modified");
    }

    std::string headers;
    std::string body;
    if (RetryRequestTestJob::IsRetryUrl(requested_url)) {
      RetryRequestTestJob::GetResponseForURL(requested_url, &headers, &body);
    } else {
      MockHttpServer::GetMockResponse(requested_url.path(), &headers, &body);
    }

    net::HttpResponseInfo info;
    info.headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        net::HttpUtil::AssembleRawHeaders(headers));

    auto response = network::mojom::URLResponseHead::New();
    response->headers = info.headers;
    response->headers->GetMimeType(&response->mime_type);

    // Provide valid request and response times to
    // UpdateUrlLoaderRequest.  It's still up to that class's
    // |OnReceiveResponse| to capture these values in the net::HttpResponseInfo.
    response->request_time = base::Time::Now();
    response->response_time = base::Time::Now();

    params->client->OnReceiveResponse(std::move(response));

    mojo::ScopedDataPipeProducerHandle producer_handle;
    mojo::ScopedDataPipeConsumerHandle consumer_handle;
    mojo::CreateDataPipe(nullptr, &producer_handle, &consumer_handle);

    uint32_t bytes_written = body.size();
    producer_handle->WriteData(body.data(), &bytes_written,
                               MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    params->client->OnStartLoadingResponseBody(std::move(consumer_handle));
    return true;
  }

  void SetUp() override {
    browser_context_ = std::make_unique<content::TestBrowserContext>();
    weak_partition_factory_ =
        std::make_unique<base::WeakPtrFactory<StoragePartitionImpl>>(
            static_cast<StoragePartitionImpl*>(
                BrowserContext::GetDefaultStoragePartition(
                    browser_context_.get())));

    ChildProcessSecurityPolicyImpl::GetInstance()->AddForTesting(
        process_id_, browser_context_.get());
    blink::TrialTokenValidator::SetOriginTrialPolicyGetter(base::BindRepeating(
        []() -> blink::OriginTrialPolicy* { return &g_origin_trial_policy; }));
  }

  void TearDown() override {
    weak_partition_factory_.reset();
    browser_context_.reset();

    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(process_id_);
  }
  // Use a separate IO thread to run a test. Thread will be destroyed
  // when it goes out of scope.
  template <class Method>
  void RunTestOnUIThread(Method method) {
    base::RunLoop run_loop;
    test_completed_cb_ = run_loop.QuitClosure();
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(method, base::Unretained(this)));
    run_loop.Run();
  }

  void StartCacheAttemptTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://failme"),
        service_->storage()->NewGroupId());

    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend mock_frontend;
    const int kMockProcessId1 = 1;
    AppCacheHost host(
        /*host_id=*/base::UnguessableToken::Create(), kMockProcessId1,
        /*render_frame_id=*/1,
        ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
            kMockProcessId1),
        mojo::NullRemote(), service_.get());
    host.set_frontend_for_testing(&mock_frontend);

    update->StartUpdate(&host, GURL());

    // Verify state.
    EXPECT_EQ(AppCacheUpdateJob::CACHE_ATTEMPT, update->update_type_);
    EXPECT_EQ(AppCacheUpdateJobState::FETCH_MANIFEST, update->internal_state_);
    EXPECT_EQ(AppCacheGroup::CHECKING, group_->update_status());
    EXPECT_TRUE(update->doing_full_update_check_);

    // Verify notifications.
    MockFrontend::RaisedEvents& events = mock_frontend.raised_events_;
    ASSERT_EQ(1u, events.size());
    EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT,
              events[0]);

    // Abort as we're not testing actual URL fetches in this test.
    delete update;
    UpdateFinished();
  }

  void StartUpgradeAttemptTest() {
    {
      MakeService();
      group_ = base::MakeRefCounted<AppCacheGroup>(
          service_->storage(), GURL("http://failme"),
          service_->storage()->NewGroupId());

      // Give the group some existing caches.
      AppCache* cache1 = MakeCacheForGroup(1, 111);
      AppCache* cache2 = MakeCacheForGroup(2, 222);

      // Associate some hosts with caches in the group.
      MockFrontend mock_frontend1;
      MockFrontend mock_frontend2;
      MockFrontend mock_frontend3;
      MockFrontend mock_frontend4;

      const int kMockProcessId1 = 1;
      const int kMockProcessId2 = 2;
      const int kMockProcessId3 = 3;
      const int kMockProcessId4 = 4;
      AppCacheHost host1(
          /*host_id=*/base::UnguessableToken::Create(), kMockProcessId1,
          /*render_frame_id=*/1,
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              kMockProcessId1),
          mojo::NullRemote(), service_.get());
      host1.set_frontend_for_testing(&mock_frontend1);
      host1.AssociateCompleteCache(cache1);

      AppCacheHost host2(
          /*host_id=*/base::UnguessableToken::Create(), kMockProcessId2,
          /*render_frame_id=*/2,
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              kMockProcessId2),
          mojo::NullRemote(), service_.get());
      host2.set_frontend_for_testing(&mock_frontend2);
      host2.AssociateCompleteCache(cache2);

      AppCacheHost host3(
          /*host_id=*/base::UnguessableToken::Create(), kMockProcessId3,
          /*render_frame_id=*/3,
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              kMockProcessId3),
          mojo::NullRemote(), service_.get());
      host3.set_frontend_for_testing(&mock_frontend3);
      host3.AssociateCompleteCache(cache1);

      AppCacheHost host4(
          /*host_id=*/base::UnguessableToken::Create(), kMockProcessId4,
          /*render_frame_id=*/4,
          ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
              kMockProcessId4),
          mojo::NullRemote(), service_.get());
      host4.set_frontend_for_testing(&mock_frontend4);

      AppCacheUpdateJob* update =
          new AppCacheUpdateJob(service_.get(), group_.get());
      group_->update_job_ = update;
      update->StartUpdate(&host4, GURL());

      // Verify state after starting an update.
      EXPECT_EQ(AppCacheUpdateJob::UPGRADE_ATTEMPT, update->update_type_);
      EXPECT_EQ(AppCacheUpdateJobState::FETCH_MANIFEST,
                update->internal_state_);
      EXPECT_EQ(AppCacheGroup::CHECKING, group_->update_status());
      EXPECT_FALSE(update->doing_full_update_check_);

      // Verify notifications.
      MockFrontend::RaisedEvents events = mock_frontend1.raised_events_;
      ASSERT_EQ(1u, events.size());
      EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT,
                events[0]);

      events = mock_frontend2.raised_events_;
      ASSERT_EQ(1u, events.size());
      EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT,
                events[0]);

      events = mock_frontend3.raised_events_;
      ASSERT_EQ(1u, events.size());
      EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT,
                events[0]);

      events = mock_frontend4.raised_events_;
      EXPECT_TRUE(events.empty());

      // Abort as we're not testing actual URL fetches in this test.
      delete update;
    }
    UpdateFinished();
  }

  void CacheAttemptFetchManifestFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://failme"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFetchManifestFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/servererror"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    group_->set_last_full_update_check_time(base::Time::Now() -
                                            kFullUpdateInterval - kOneHour);
    update->StartUpdate(nullptr, GURL());
    EXPECT_TRUE(update->doing_full_update_check_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_evictable_error_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    expect_full_update_time_equal_to_ = group_->last_full_update_check_time();
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestRedirectTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://testme"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // redirect is like a failed request
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestMissingMimeTypeTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/missing-mime-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 33);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    frontend->SetVerifyProgressEvents(true);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = EMPTY_MANIFEST;
    tested_manifest_path_override_ = "files/missing-mime-manifest";
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestNotFoundTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/nosuchfile"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = true;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestGoneFetchTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/gone"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void ManifestGoneUpgradeTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/gone"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);

    AppCache* cache = MakeCacheForGroup(1, 111);
    host->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = true;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_OBSOLETE_EVENT);

    WaitForUpdateToFinish();
  }

  void CacheAttemptNotModifiedTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // treated like cache failure
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeNotModifiedTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    group_->set_last_full_update_check_time(base::Time::Now() -
                                            kFullUpdateInterval - kOneHour);
    group_->set_first_evictable_error_time(base::Time::Now());
    update->StartUpdate(nullptr, GURL());
    EXPECT_TRUE(update->doing_full_update_check_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;     // newest cache unaffected by update
    expect_evictable_error_ = false;  // should be reset
    expect_full_update_time_newer_than_ = group_->last_full_update_check_time();
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeNotModifiedVersion1Test() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    cache->set_manifest_parser_version(1);
    EXPECT_EQ(cache->token_expires(), base::Time());

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_path_override_ = "files/notmodified";
    tested_manifest_ = MANIFEST1;

    // Get the expected expiration date of the test token.
    {
      blink::TrialTokenValidator validator;
      blink::TrialTokenResult result = validator.ValidateToken(
          kTestAppCacheOriginTrialToken, MockHttpServer::GetOrigin(),
          base::Time::Now());
      expect_token_expires_ = result.expiry_time;
      ASSERT_EQ(result.status, blink::OriginTrialTokenStatus::kSuccess);
      EXPECT_EQ(GetAppCacheOriginTrialNameForTesting(), result.feature_name);
      EXPECT_NE(base::Time(), expect_token_expires_);
    }

    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed the response_info working set with canned data so that an existing
    // manifest is reused by the update job.
    const char kData[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    const std::string kRawHeaders(kData, base::size(kData));
    MakeAppCacheResponseInfo(group_->manifest_url(),
                             response_writer_->response_id(), kRawHeaders);

    // Last data parsed pretends to be version 1, and doesn't know about
    // the origin trial (verified above).  Even with a 304, this should
    // refresh the token expires field.
    const std::string seed_data(kManifest1OriginTrialContents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeManifestDataChangedScopeUnchangedTest() {
    MakeService();
    // URL path /files/manifest2-with-root-override has a cached scope of "/".
    // The path has a scope override of "/", so the fetched scope will be "/"
    // and no scope change will occur.
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest2-with-root-override"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST2_WITH_ROOT_SCOPE;

    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with manifest1 contents.  The result should be that the
    // manifest2 fetch results in different byte-for-byte data.
    const std::string seed_data(kManifest1Contents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeManifestDataChangedScopeChangedTest() {
    MakeService();
    // URL path /files/manifest2 has a cached scope of "/".  The path has no
    // scope override, so the fetched scope will be "/files/" and a scope change
    // will occur.
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest2"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST2_WITH_FILES_SCOPE;

    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with manifest1 contents.  The result should be that the
    // manifest2 fetch results in different byte-for-byte data.
    const std::string seed_data(kManifest1Contents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeManifestDataUnchangedScopeUnchangedTest() {
    MakeService();
    // URL path /files/manifest2-with-root-override has a cached scope of "/".
    // The path has a scope override of "/", so the fetched scope will be "/"
    // and no scope change will occur.
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest2-with-root-override"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    // Seed storage with expected manifest data.
    const std::string seed_data(kManifest2Contents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeManifestDataUnchangedScopeChangedTest() {
    MakeService();
    // URL path /files/manifest2 has a cached scope of "/".  The path has no
    // scope override, so the fetched scope will be "/files/" and a scope change
    // will occur.
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest2"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST2_WITH_FILES_SCOPE;

    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected manifest data.
    const std::string seed_data(kManifest2Contents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  // See http://code.google.com/p/chromium/issues/detail?id=95101
  void Bug95101Test() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/empty-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create a malformed cache with a missing manifest entry.
    GURL wrong_manifest_url =
        MockHttpServer::GetMockUrl("files/missing-mime-manifest");
    AppCache* cache = MakeCacheForGroup(1, wrong_manifest_url, 111);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_is_being_deleted_ = true;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);
    frontend->expected_error_message_ =
        "Manifest entry not found in existing cache";
    WaitForUpdateToFinish();
  }

  void StartUpdateAfterSeedingStorageData(int result) {
    ASSERT_GT(result, 0);
    response_writer_.reset();

    AppCacheUpdateJob* update = group_->update_job_;
    update->StartUpdate(nullptr, GURL());

    WaitForUpdateToFinish();
  }

  void BasicCacheAttemptSuccessTest() {
    GURL manifest_url = MockHttpServer::GetMockUrl("files/manifest1");

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    ASSERT_TRUE(group_->last_full_update_check_time().is_null());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_full_update_time_newer_than_ = base::Time::Now() - kOneHour;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  // Tests that 200 OK responses with varying scopes result in the expected
  // AppCache data stored in the cache.
  void ScopeManifestRootWithNoOverrideTest() {
    ScopeTest("manifest-no-override", SCOPE_MANIFEST_ROOT);
  }

  void ScopeManifestBarWithNoOverrideTest() {
    ScopeTest("bar/manifest-no-override", SCOPE_MANIFEST_BAR);
  }

  void ScopeManifestRootWithRootOverrideTest() {
    ScopeTest("manifest-root-override", SCOPE_MANIFEST_ROOT);
  }

  void ScopeManifestBarWithRootOverrideTest() {
    ScopeTest("bar/manifest-root-override", SCOPE_MANIFEST_ROOT);
  }

  void ScopeManifestRootWithBarOverrideTest() {
    ScopeTest("manifest-bar-override", SCOPE_MANIFEST_BAR);
  }

  void ScopeManifestBarWithBarOverrideTest() {
    ScopeTest("bar/manifest-bar-override", SCOPE_MANIFEST_BAR);
  }

  void ScopeManifestRootWithOtherOverrideTest() {
    ScopeTest("manifest-other-override", SCOPE_MANIFEST_OTHER);
  }

  void ScopeManifestBarWithOtherOverrideTest() {
    ScopeTest("bar/manifest-other-override", SCOPE_MANIFEST_OTHER);
  }

  // Tests that 304 NOT MODIFIED responses with varying scopes result in the
  // expected AppCache data stored in the cache.
  void ScopeManifest304RootWithNoOverrideTest() {
    Scope304Test("manifest-304-no-override", /*previous_scope=*/"/bar/",
                 SCOPE_MANIFEST_ROOT);
  }

  void ScopeManifest304BarWithNoOverrideTest() {
    Scope304Test("bar/manifest-304-no-override", /*previous_scope=*/"/baz/",
                 SCOPE_MANIFEST_BAR);
  }

  void ScopeManifest304RootWithRootOverrideTest() {
    Scope304Test("manifest-304-root-override", /*previous_scope=*/"/bar/",
                 SCOPE_MANIFEST_ROOT);
  }

  void ScopeManifest304BarWithRootOverrideTest() {
    Scope304Test("bar/manifest-304-root-override", /*previous_scope=*/"/baz/",
                 SCOPE_MANIFEST_ROOT);
  }

  void ScopeManifest304RootWithBarOverrideTest() {
    Scope304Test("manifest-304-bar-override", /*previous_scope=*/"/baz/",
                 SCOPE_MANIFEST_BAR);
  }

  void ScopeManifest304BarWithBarOverrideTest() {
    Scope304Test("bar/manifest-304-bar-override", /*previous_scope=*/"/baz/",
                 SCOPE_MANIFEST_BAR);
  }

  void ScopeManifest304RootWithOtherOverrideTest() {
    Scope304Test("manifest-304-other-override", /*previous_scope=*/"/bar/",
                 SCOPE_MANIFEST_OTHER);
  }

  void ScopeManifest304BarWithOtherOverrideTest() {
    Scope304Test("bar/manifest-304-other-override", /*previous_scope=*/"/baz/",
                 SCOPE_MANIFEST_OTHER);
  }

  void DownloadInterceptEntriesTest() {
    // Ensures we download intercept entries too.
    GURL manifest_url =
        MockHttpServer::GetMockUrl("files/manifest-with-intercept");
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST_WITH_INTERCEPT;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void BasicUpgradeSuccessTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create a response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);
    frontend1->SetVerifyProgressEvents(true);
    frontend2->SetVerifyProgressEvents(true);
    group_->set_last_full_update_check_time(base::Time::Now() -
                                            kFullUpdateInterval - kOneHour);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_full_update_time_newer_than_ = group_->last_full_update_check_time();
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected manifest data different from manifest1.
    const std::string seed_data("different");
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeLoadFromNewestCacheTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    expect_response_ids_.insert(std::map<GURL, int64_t>::value_type(
        MockHttpServer::GetMockUrl("files/explicit1"),
        response_writer_->response_id()));
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected http response info for entry. Allow reuse.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Cache-Control: max-age=8675309\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeNoLoadFromNewestCacheTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected http response info for entry. Do NOT
    // allow reuse by setting an expires header in the past.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Expires: Thu, 01 Dec 1994 16:00:00 GMT\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeLoadFromNewestCacheVaryHeaderTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected http response info for entry: a vary header.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Cache-Control: max-age=8675309\0"
        "Vary: blah\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeLoadFromNewestCacheReuseVaryHeaderTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Give the newest cache an entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT,
                                  response_writer_->response_id()));

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    expect_response_ids_.insert(std::map<GURL, int64_t>::value_type(
        MockHttpServer::GetMockUrl("files/explicit1"),
        response_writer_->response_id()));
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected http response info for an entry
    // with a vary header for which we allow reuse.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Cache-Control: max-age=8675309\0"
        "Vary: origin, accept-encoding\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->request_time = base::Time::Now();
    response_info->response_time = base::Time::Now();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void UpgradeSuccessMergedTypesTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest-merged-types"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Give the newest cache a master entry that is also one of the explicit
    // entries in the manifest.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit1"),
                    AppCacheEntry(AppCacheEntry::MASTER, 111));

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST_MERGED_TYPES;
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // explicit1
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // manifest
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void CacheAttemptFailUrlFetchTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest-with-404"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // 404 explicit url is cache failure
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailUrlFetchTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest-fb-404"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 99);
    group_->set_first_evictable_error_time(
        base::Time::Now() - kMaxEvictableErrorDuration - kOneHour);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    frontend1->SetIgnoreProgressEvents(true);
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffectd by failed update
    expect_eviction_ = true;
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailMasterUrlFetchTest() {
    tested_manifest_path_override_ = "files/manifest1-with-notmodified";

    MakeService();
    const GURL kManifestUrl =
        MockHttpServer::GetMockUrl(tested_manifest_path_override_);
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), kManifestUrl, service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 25);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Give the newest cache some existing entries; one will fail with a 404.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/notfound"),
                    AppCacheEntry(AppCacheEntry::MASTER, 222));
    cache->AddEntry(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER | AppCacheEntry::FOREIGN, 333));
    cache->AddEntry(MockHttpServer::GetMockUrl("files/servererror"),
                    AppCacheEntry(AppCacheEntry::MASTER, 444));
    cache->AddEntry(MockHttpServer::GetMockUrl("files/notmodified"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 555));

    // Seed the response_info working set with canned data for
    // files/servererror and for files/notmodified to test that the
    // existing entries for those resource are reused by the update job.
    const char kData[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    const std::string kRawHeaders(kData, base::size(kData));
    MakeAppCacheResponseInfo(kManifestUrl, 444, kRawHeaders);
    MakeAppCacheResponseInfo(kManifestUrl, 555, kRawHeaders);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));  // foreign flag is dropped
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/servererror"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/notmodified"),
        AppCacheEntry(AppCacheEntry::EXPLICIT)));
    expect_response_ids_.insert(std::map<GURL, int64_t>::value_type(
        MockHttpServer::GetMockUrl("files/servererror"), 444));  // copied
    expect_response_ids_.insert(std::map<GURL, int64_t>::value_type(
        MockHttpServer::GetMockUrl("files/notmodified"), 555));  // copied
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // explicit1
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // fallback1a
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // notfound
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // explicit2
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // servererror
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // notmodified
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // explicit1
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // fallback1a
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // notfound
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // explicit2
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // servererror
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // notmodified
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void EmptyManifestTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/empty-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 33);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    frontend1->SetVerifyProgressEvents(true);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = EMPTY_MANIFEST;
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void EmptyFileTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/empty-file-manifest"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 22);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);
    frontend->SetVerifyProgressEvents(true);

    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = EMPTY_FILE_MANIFEST;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void RetryRetryAfterTest() {
    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 3 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::RETRY_AFTER_0, 4);
    RetryRequestTest(false);
  }

  void RetryNoRetryAfterTest() {
    // Set some large number of times to return retry.
    // Expect 1 manifest fetch and 0 retries.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::NO_RETRY_AFTER, 1);
    RetryRequestTest(false);
  }

  void RetryNonzeroRetryAfterTest() {
    // Set some large number of times to return retry.
    // Expect 1 request and 0 retry attempts.
    RetryRequestTestJob::Initialize(5, RetryRequestTestJob::NONZERO_RETRY_AFTER,
                                    1);
    RetryRequestTest(false);
  }

  void RetrySuccessTest() {
    // Set 2 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 2 retries, 1 url fetch, 1 manifest refetch.
    RetryRequestTestJob::Initialize(2, RetryRequestTestJob::RETRY_AFTER_0, 5);
    RetryRequestTest(true);
  }

  void RetryUrlTest() {
    // Set 1 as the retry limit (does not exceed the max).
    // Expect 1 manifest fetch, 1 url fetch, 1 url retry, 1 manifest refetch.
    RetryRequestTestJob::Initialize(1, RetryRequestTestJob::RETRY_AFTER_0, 4);
    RetryRequestTest(true);
  }

  void FailStoreNewestCacheTest() {
    MakeService();
    MockAppCacheStorage* storage =
        static_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // storage failed
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeFailStoreNewestCacheTest() {
    MakeService();
    MockAppCacheStorage* storage =
        static_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 11);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // unchanged
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFailStoreNewestCacheTest() {
    MakeService();
    MockAppCacheStorage* storage =
        static_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateStoreGroupAndNewestCacheFailure();

    const GURL kManifestUrl = MockHttpServer::GetMockUrl("files/notmodified");
    const int64_t kManifestResponseId = 11;

    // Seed the response_info working set with canned data for
    // files/servererror and for files/notmodified to test that the
    // existing entries for those resource are reused by the update job.
    const char kData[] =
        "HTTP/1.1 200 OK\0"
        "Content-type: text/cache-manifest\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    const std::string kRawHeaders(kData, base::size(kData));
    MakeAppCacheResponseInfo(kManifestUrl, kManifestResponseId, kRawHeaders);

    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), kManifestUrl, service_->storage()->NewGroupId());
    scoped_refptr<AppCache> cache(MakeCacheForGroup(
        service_->storage()->NewCacheId(), kManifestResponseId));

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->SetSiteForCookiesForTesting(
        net::SiteForCookies::FromUrl(kManifestUrl));
    host->SelectCache(MockHttpServer::GetMockUrl("files/empty1"),
                      blink::mojom::kAppCacheNoCacheId, kManifestUrl);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    tested_manifest_ = EMPTY_MANIFEST;
    tested_manifest_path_override_ = "files/notmodified";
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache.get();  // unchanged
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);
    frontend->expected_error_message_ = "Failed to commit new cache to storage";

    WaitForUpdateToFinish();
  }

  void UpgradeFailMakeGroupObsoleteTest() {
    MakeService();
    MockAppCacheStorage* storage =
        static_cast<MockAppCacheStorage*>(service_->storage());
    storage->SimulateMakeGroupObsoleteFailure();

    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/nosuchfile"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFetchManifestFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(service_->storage(),
                                                 GURL("http://failme"), 111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->new_master_entry_url_ = GURL("http://failme/blah");
    update->StartUpdate(host, host->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryBadManifestTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/bad-manifest"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->new_master_entry_url_ = MockHttpServer::GetMockUrl("files/blah");
    update->StartUpdate(host, host->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryManifestNotFoundTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/nosuchfile"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->new_master_entry_url_ = MockHttpServer::GetMockUrl("files/blah");

    update->StartUpdate(host, host->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryFailUrlFetchTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest-fb-404"), 111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    frontend->SetIgnoreProgressEvents(true);
    AppCacheHost* host = MakeHost(frontend);
    host->new_master_entry_url_ = MockHttpServer::GetMockUrl("files/explicit1");

    update->StartUpdate(host, host->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // 404 fallback url is cache failure
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryAllFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    frontend1->SetIgnoreProgressEvents(true);
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/servererror");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;  // all pending masters failed
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeMasterEntryAllFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->AssociateCompleteCache(cache);

    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    frontend3->SetIgnoreProgressEvents(true);
    AppCacheHost* host3 = MakeHost(frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/servererror");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntrySomeFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    frontend1->SetIgnoreProgressEvents(true);
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;  // as long as one pending master succeeds
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);

    WaitForUpdateToFinish();
  }

  void UpgradeMasterEntrySomeFailTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->AssociateCompleteCache(cache);

    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void MasterEntryNoUpdateTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/notmodified"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->AssociateCompleteCache(cache);

    // Give cache an existing entry that can also be fetched.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit2"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 222));

    // Reset the update time to null so we can verify it gets
    // modified in this test case by the UpdateJob.
    cache->set_update_time(base::Time());

    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host3, host3->new_master_entry_url_);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache still the same cache
    expect_non_null_update_time_ = true;
    tested_manifest_ = PENDING_MASTER_NO_UPDATE;
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidCacheAttemptTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host1, host1->new_master_entry_url_);

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend2 = MakeMockFrontend();
    frontend2->SetIgnoreProgressEvents(true);
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(frontend4);
    host4->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(frontend5);  // no master entry url

    frontend1->TriggerAdditionalUpdates(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT, update);
    frontend1->AdditionalUpdateHost(host2);    // fetch will fail
    frontend1->AdditionalUpdateHost(host3);    // same as an explicit entry
    frontend1->AdditionalUpdateHost(host4);    // same as another master entry
    frontend1->AdditionalUpdateHost(nullptr);  // no host
    frontend1->AdditionalUpdateHost(host5);    // no master entry url

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);

    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);

    // Host 5 is not associated with cache so no progress/cached events.
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidNoUpdateTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/notmodified"),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(1, 111);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->AssociateCompleteCache(cache);

    // Give cache an existing entry.
    cache->AddEntry(MockHttpServer::GetMockUrl("files/explicit2"),
                    AppCacheEntry(AppCacheEntry::EXPLICIT, 222));

    // Start update with a pending master entry that will fail to give us an
    // event to trigger other updates.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/nosuchfile");
    update->StartUpdate(host2, host2->new_master_entry_url_);

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(frontend4);  // no master entry url

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(frontend5);
    host5->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");  // existing entry

    MockFrontend* frontend6 = MakeMockFrontend();
    AppCacheHost* host6 = MakeHost(frontend6);
    host6->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    frontend2->TriggerAdditionalUpdates(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT, update);
    frontend2->AdditionalUpdateHost(host3);
    frontend2->AdditionalUpdateHost(nullptr);  // no host
    frontend2->AdditionalUpdateHost(host4);    // no master entry url
    frontend2->AdditionalUpdateHost(host5);    // same as existing cache entry
    frontend2->AdditionalUpdateHost(host6);    // same as another master entry

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_newest_cache_ = cache;  // newest cache unaffected by update
    tested_manifest_ = PENDING_MASTER_NO_UPDATE;
    // prior associated host
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_ERROR_EVENT);

    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    // unassociated w/cache
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    frontend6->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend6->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_NO_UPDATE_EVENT);

    WaitForUpdateToFinish();
  }

  void StartUpdateMidDownloadTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), 42);
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->AssociateCompleteCache(cache);

    update->StartUpdate(nullptr, GURL());

    // Set up additional updates to be started while update is in progress.
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host2 = MakeHost(frontend2);
    host2->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit1");

    MockFrontend* frontend3 = MakeMockFrontend();
    AppCacheHost* host3 = MakeHost(frontend3);
    host3->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");

    MockFrontend* frontend4 = MakeMockFrontend();
    AppCacheHost* host4 = MakeHost(frontend4);  // no master entry url

    MockFrontend* frontend5 = MakeMockFrontend();
    AppCacheHost* host5 = MakeHost(frontend5);
    host5->new_master_entry_url_ =
        MockHttpServer::GetMockUrl("files/explicit2");

    frontend1->TriggerAdditionalUpdates(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT, update);
    frontend1->AdditionalUpdateHost(host2);    // same as entry in manifest
    frontend1->AdditionalUpdateHost(nullptr);  // no host
    frontend1->AdditionalUpdateHost(host3);    // new master entry
    frontend1->AdditionalUpdateHost(host4);    // no master entry url
    frontend1->AdditionalUpdateHost(host5);    // same as another master entry

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        MockHttpServer::GetMockUrl("files/explicit2"),
        AppCacheEntry(AppCacheEntry::MASTER)));
    // prior associated host
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend3->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // unassociated w/cache
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend4->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);

    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend5->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    WaitForUpdateToFinish();
  }

  void QueueMasterEntryTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Pretend update job has been running and is about to terminate.
    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->internal_state_ = AppCacheUpdateJobState::REFETCH_MANIFEST;
    EXPECT_TRUE(update->IsTerminating());

    // Start an update. Should be queued.
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->new_master_entry_url_ = MockHttpServer::GetMockUrl("files/explicit2");
    update->StartUpdate(host, host->new_master_entry_url_);
    EXPECT_TRUE(update->pending_master_entries_.empty());
    EXPECT_FALSE(group_->queued_updates_.empty());

    // Delete update, causing it to finish, which should trigger a new update
    // for the queued host and master entry after a delay.
    delete update;
    EXPECT_FALSE(group_->restart_update_task_.IsCancelled());

    // Set up checks for when queued update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1;
    expect_extra_entries_.insert(AppCache::EntryMap::value_type(
        host->new_master_entry_url_, AppCacheEntry(AppCacheEntry::MASTER)));
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CACHED_EVENT);

    // Group status will be blink::mojom::AppCacheStatus::APPCACHE_STATUS_IDLE
    // so cannot call WaitForUpdateToFinish.
    group_->AddUpdateObserver(this);
  }

  void VerifyHeadersAndDeleteUpdate(AppCacheUpdateJob* update) {
    for (const auto& kvp : http_headers_request_test_jobs_)
      kvp.second->Verify(kvp.first);
    delete update;
  }

  void IfModifiedSinceTestCache() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://headertest"), 111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // First test against a cache attempt. Will start manifest fetch
    // synchronously.
    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(), std::make_unique<HttpHeadersRequestTestJob>(
                                    std::string(), std::string()));
    MockFrontend mock_frontend;
    const int kMockProcessId1 = 1;
    AppCacheHost host(
        /*host_id=*/base::UnguessableToken::Create(), kMockProcessId1,
        /*render_frame_id=*/1,
        ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
            kMockProcessId1),
        mojo::NullRemote(), service_.get());
    host.set_frontend_for_testing(&mock_frontend);
    update->StartUpdate(&host, GURL());

    // We need to wait for the URL load requests to make it to the
    // URLLoaderInterceptor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheUpdateJobTest::VerifyHeadersAndDeleteUpdate,
                       base::Unretained(this), update));

    TriggerTestComplete();
  }

  void IfModifiedTestRefetch() {
    // Now simulate a refetch manifest request. Will start fetch request
    // synchronously.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "\0";
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(data, base::size(data)));

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://headertest"), 111);

    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(), std::make_unique<HttpHeadersRequestTestJob>(
                                    std::string(), std::string()));

    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;
    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_ = std::move(response_info);
    update->internal_state_ = AppCacheUpdateJobState::REFETCH_MANIFEST;
    update->RefetchManifest();

    // We need to wait for the URL load requests to make it to the
    // URLLoaderInterceptor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheUpdateJobTest::VerifyHeadersAndDeleteUpdate,
                       base::Unretained(this), update));

    TriggerTestComplete();
  }

  void IfModifiedTestLastModified() {
    // Change the headers to include a Last-Modified header. Manifest refetch
    // should include If-Modified-Since header.
    const char data2[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(data2, base::size(data2)));

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://headertest"), 111);

    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(),
        std::make_unique<HttpHeadersRequestTestJob>(
            "Sat, 29 Oct 1994 19:43:31 GMT", std::string()));
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;
    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_ = std::move(response_info);
    update->internal_state_ = AppCacheUpdateJobState::REFETCH_MANIFEST;
    update->RefetchManifest();

    // We need to wait for the URL load requests to make it to the
    // URLLoaderInterceptor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheUpdateJobTest::VerifyHeadersAndDeleteUpdate,
                       base::Unretained(this), update));

    TriggerTestComplete();
  }

  // AppCaches built with manifest parser version 0 should update without
  // conditional request headers to force the server to send a 200 response
  // back to the client rather than 304.  This test ensures that, when the cache
  // has a response info with cached Last-Modified headers, the request does not
  // include an If-Modified-Since conditioanl header.
  void IfModifiedSinceUpgradeParserVersion0Test() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(), std::make_unique<HttpHeadersRequestTestJob>(
                                    std::string(), std::string(),
                                    /*headers_allowed=*/false));
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Give the newest cache a manifest entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    cache->set_manifest_parser_version(0);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void IfModifiedSinceUpgradeParserVersion1Test() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create a cache without a manifest entry.  The manifest entry will be
    // added later.
    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), -1);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    AppCacheCacheTestHelper::CacheEntries cache_entries;

    // Add cache entry for manifest.
    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->headers = std::move(headers);
    AppCacheCacheTestHelper::AddCacheEntry(
        &cache_entries, group_->manifest_url(), AppCacheEntry::EXPLICIT,
        /*expect_if_modified_since=*/"Sat, 29 Oct 1994 19:43:31 GMT",
        /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
        std::move(response_info), kManifest1Contents);

    // Add all header checks from |cache_entries|.
    for (const auto& kvp : cache_entries) {
      const GURL& cache_entry_url = kvp.first;
      const AppCacheCacheTestHelper::CacheEntry* cache_entry = kvp.second.get();
      http_headers_request_test_jobs_.emplace(
          cache_entry_url,
          std::make_unique<HttpHeadersRequestTestJob>(
              cache_entry->expect_if_modified_since,
              cache_entry->expect_if_none_match, cache_entry->headers_allowed));
    }

    cache_helper_ = std::make_unique<AppCacheCacheTestHelper>(
        service_.get(), group_->manifest_url(), cache, std::move(cache_entries),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));
    cache_helper_->Write();

    // Start update after data write completes asynchronously.
  }

  // AppCaches built with manifest parser version 0 should update without
  // conditional request headers to force the server to send a 200 response
  // back to the client rather than 304.  This test ensures that, when the cache
  // has a response info with cached ETag headers, the request does not include
  // an If-None-Match conditioanl header.
  void IfNoneMatchUpgradeParserVersion0Test() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(), std::make_unique<HttpHeadersRequestTestJob>(
                                    std::string(), std::string(),
                                    /*headers_allowed=*/false));
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Give the newest cache a manifest entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    cache->set_manifest_parser_version(0);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected manifest response info that will cause
    // an If-None-Match header to be put in the manifest fetch request.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void IfNoneMatchUpgradeParserVersion1Test() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(), std::make_unique<HttpHeadersRequestTestJob>(
                                    std::string(), "\"LadeDade\""));
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Give the newest cache a manifest entry that is in storage.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST1;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with expected manifest response info that will cause
    // an If-None-Match header to be put in the manifest fetch request.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    scoped_refptr<net::HttpResponseHeaders> headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(
            std::string(data, base::size(data)));
    std::unique_ptr<net::HttpResponseInfo> response_info =
        std::make_unique<net::HttpResponseInfo>();
    response_info->headers = std::move(headers);
    scoped_refptr<HttpResponseInfoIOBuffer> io_buffer =
        base::MakeRefCounted<HttpResponseInfoIOBuffer>(
            std::move(response_info));
    response_writer_->WriteInfo(
        io_buffer.get(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void RequestResponseTimesAreSetTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), MockHttpServer::GetMockUrl("files/manifest1"),
        111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create a cache without a manifest entry.  The manifest entry will be
    // added later.
    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), -1);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = false;

    AppCacheCacheTestHelper::CacheEntries cache_entries;

    // Add cache entry for manifest.
    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    {
      const char data[] =
          "HTTP/1.1 200 OK\0"
          "Last-Modified: Sat, 29 Oct 2019 19:43:31 GMT\0";
      scoped_refptr<net::HttpResponseHeaders> headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              std::string(data, base::size(data)));
      std::unique_ptr<net::HttpResponseInfo> response_info =
          std::make_unique<net::HttpResponseInfo>();
      response_info->headers = std::move(headers);
      response_info->request_time = base::Time::Now() - kOneYear;
      response_info->response_time = base::Time::Now() - kOneYear;
      AppCacheCacheTestHelper::AddCacheEntry(
          &cache_entries, group_->manifest_url(), AppCacheEntry::EXPLICIT,
          /*expect_if_modified_since=*/std::string(),
          /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
          std::move(response_info), kManifest1Contents);
    }

    cache_helper_ = std::make_unique<AppCacheCacheTestHelper>(
        service_.get(), group_->manifest_url(), cache, std::move(cache_entries),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));
    cache_helper_->Write();

    post_update_finished_cb_ = base::BindOnce(
        &AppCacheUpdateJobTest::RequestResponseTimesAreSetUpdateFinished,
        base::Unretained(this));

    // Start update after data write completes asynchronously.
    // After update is finished, continues async in
    // |RequestResponseTimesAreSetUpdateFinished|.
  }

  void RequestResponseTimesAreSetUpdateFinished() {
    ASSERT_NE(group_->newest_complete_cache(), cache_helper_->write_cache());
    ASSERT_NE(group_->newest_complete_cache(), nullptr);
    cache_helper_->PrepareForRead(
        group_->newest_complete_cache(),
        base::BindOnce(
            &AppCacheUpdateJobTest::RequestResponseTimesAreSetReadFinished,
            base::Unretained(this)));
    cache_helper_->Read();
    // Continues async in |RequestResponseTimesAreSetReadFinished|.
  }

  void RequestResponseTimesAreSetReadFinished() {
    auto it = cache_helper_->read_cache_entries().find(
        MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_NE(it, cache_helper_->read_cache_entries().end());
    CHECK_GT(it->second->response_info->request_time,
             base::Time::Now() - kOneHour);
    CHECK_GT(it->second->response_info->response_time,
             base::Time::Now() - kOneHour);
    TriggerTestComplete();
    // Continues async in |TestComplete|.
  }

  void RequestResponseTimesAreModifiedTest() {
    RequestResponseTimesModified(/*feature_enabled=*/true);
  }

  void RequestResponseTimesAreNotModifiedTest() {
    RequestResponseTimesModified(/*feature_enabled=*/false);
  }

  void RequestResponseTimesModified(bool feature_enabled) {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest1-with-notmodified"), 111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create a cache without a manifest entry.  The manifest entry will be
    // added later.
    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), -1);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1_WITH_NOTMODIFIED;
    if (feature_enabled) {
      expect_old_cache_ = cache;
    }
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    AppCacheCacheTestHelper::CacheEntries cache_entries;

    // Add cache entry for manifest.
    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    {
      const char data[] =
          "HTTP/1.1 200 OK\0"
          "Last-Modified: Sat, 29 Oct 2019 19:43:31 GMT\0";
      scoped_refptr<net::HttpResponseHeaders> headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              std::string(data, base::size(data)));
      std::unique_ptr<net::HttpResponseInfo> response_info =
          std::make_unique<net::HttpResponseInfo>();
      response_info->headers = std::move(headers);
      response_info->request_time = base::Time::Now() - kOneYear;
      response_info->response_time = base::Time::Now() - kOneYear;
      AppCacheCacheTestHelper::AddCacheEntry(
          &cache_entries, group_->manifest_url(), AppCacheEntry::EXPLICIT,
          /*expect_if_modified_since=*/std::string(),
          /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
          std::move(response_info), kManifest1WithNotModifiedContents);
    }

    // Add cache entry for notmodified.
    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    {
      const char data[] =
          "HTTP/1.1 200 OK\0"
          "Last-Modified: Sat, 29 Oct 2019 19:43:31 GMT\0";
      scoped_refptr<net::HttpResponseHeaders> headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              std::string(data, base::size(data)));
      std::unique_ptr<net::HttpResponseInfo> response_info =
          std::make_unique<net::HttpResponseInfo>();
      response_info->headers = std::move(headers);
      // Leave the request and response time fields unset to match corrupt
      // cases in the field.
      CHECK_EQ(response_info->request_time, base::Time());
      CHECK_EQ(response_info->response_time, base::Time());
      AppCacheCacheTestHelper::AddCacheEntry(
          &cache_entries, MockHttpServer::GetMockUrl("files/notmodified"),
          AppCacheEntry::EXPLICIT,
          /*expect_if_modified_since=*/"Sat, 29 Oct 2019 19:43:31 GMT",
          /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
          std::move(response_info), /*body=*/"laughing-giraffe");
    }

    // Add all header checks from |cache_entries|.
    for (auto& it : cache_entries) {
      http_headers_request_test_jobs_.emplace(
          it.first,
          std::make_unique<HttpHeadersRequestTestJob>(
              it.second->expect_if_modified_since,
              it.second->expect_if_none_match, it.second->headers_allowed));
    }

    cache_helper_ = std::make_unique<AppCacheCacheTestHelper>(
        service_.get(), group_->manifest_url(), cache, std::move(cache_entries),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));
    cache_helper_->Write();

    post_update_finished_cb_ = base::BindOnce(
        &AppCacheUpdateJobTest::RequestResponseTimesModifiedUpdateFinished,
        base::Unretained(this), feature_enabled);

    // Start update after data write completes asynchronously.
    // After update is finished, continues async in
    // |RequestResponseTimesModifiedUpdateFinished|.
  }

  void RequestResponseTimesModifiedUpdateFinished(bool feature_enabled) {
    ASSERT_NE(group_->newest_complete_cache(), cache_helper_->write_cache());
    ASSERT_NE(group_->newest_complete_cache(), nullptr);
    cache_helper_->PrepareForRead(
        group_->newest_complete_cache(),
        base::BindOnce(
            &AppCacheUpdateJobTest::RequestResponseTimesModifiedReadFinished,
            base::Unretained(this), feature_enabled));
    cache_helper_->Read();
    // Continues async in |RequestResponseTimesModifiedReadFinished|.
  }

  void RequestResponseTimesModifiedReadFinished(bool feature_enabled) {
    auto it = cache_helper_->read_cache_entries().find(
        MockHttpServer::GetMockUrl("files/notmodified"));
    ASSERT_NE(it, cache_helper_->read_cache_entries().end());
    // Verify that the cache body on the entry matches the originally written
    // cache body.
    CHECK_EQ(it->second->body, "laughing-giraffe");
    if (feature_enabled) {
      CHECK_GT(it->second->response_info->request_time,
               base::Time::Now() - kOneHour);
      CHECK_GT(it->second->response_info->response_time,
               base::Time::Now() - kOneHour);
    } else {
      CHECK_EQ(it->second->response_info->request_time, base::Time());
      CHECK_EQ(it->second->response_info->response_time, base::Time());
    }
    TriggerTestComplete();
    // Continues async in |TestComplete|.
  }

  void RequestResponseTimesCorruptionFixedTest() {
    RequestResponseTimesCorruption(/*expect_modified=*/true);
  }

  void RequestResponseTimesCorruptionNotFixedTest() {
    RequestResponseTimesCorruption(/*expect_modified=*/false);
  }

  void RequestResponseTimesCorruption(bool expect_modified) {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl("files/manifest1-with-maybemodified"), 111);
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create a cache without a manifest entry.  The manifest entry will be
    // added later.
    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(), -1);
    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    host->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = MANIFEST1_WITH_MAYBEMODIFIED;
    if (!expect_modified) {
      expect_old_cache_ = cache;
    }
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    AppCacheCacheTestHelper::CacheEntries cache_entries;

    // Add cache entry for manifest.
    // Seed storage with expected manifest response info that will cause
    // an If-Modified-Since header to be put in the manifest fetch request.
    {
      const char data[] =
          "HTTP/1.1 200 OK\0"
          "Last-Modified: Sat, 29 Oct 2019 19:43:31 GMT\0";
      scoped_refptr<net::HttpResponseHeaders> headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              std::string(data, base::size(data)));
      std::unique_ptr<net::HttpResponseInfo> response_info =
          std::make_unique<net::HttpResponseInfo>();
      response_info->headers = std::move(headers);
      response_info->request_time = base::Time::Now() - kOneYear;
      response_info->response_time = base::Time::Now() - kOneYear;
      AppCacheCacheTestHelper::AddCacheEntry(
          &cache_entries, group_->manifest_url(), AppCacheEntry::EXPLICIT,
          /*expect_if_modified_since=*/std::string(),
          /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
          std::move(response_info), kManifest1WithMaybeModifiedContents);
    }

    // Add cache entry for maybemodified.
    // Seed storage with expected cache response info that will cause
    // an If-Modified-Since header to be put in the fetch request.
    {
      const char data[] =
          "HTTP/1.1 200 OK\0"
          "Last-Modified: Sat, 29 Oct 2019 19:43:31 GMT\0";
      scoped_refptr<net::HttpResponseHeaders> headers =
          base::MakeRefCounted<net::HttpResponseHeaders>(
              std::string(data, base::size(data)));
      std::unique_ptr<net::HttpResponseInfo> response_info =
          std::make_unique<net::HttpResponseInfo>();
      response_info->headers = std::move(headers);
      CHECK_EQ(response_info->request_time, base::Time());
      CHECK_EQ(response_info->response_time, base::Time());
      std::string expect_if_modified_since;
      if (!expect_modified) {
        expect_if_modified_since = "Sat, 29 Oct 2019 19:43:31 GMT";
      }
      AppCacheCacheTestHelper::AddCacheEntry(
          &cache_entries, MockHttpServer::GetMockUrl("files/maybemodified"),
          AppCacheEntry::EXPLICIT,
          /*expect_if_modified_since=*/expect_if_modified_since,
          /*expect_if_none_match=*/std::string(), /*headers_allowed=*/true,
          std::move(response_info), /*body=*/"cached-maybemodified");
    }

    // Add all header checks from |cache_entries|.
    for (auto& it : cache_entries) {
      http_headers_request_test_jobs_.emplace(
          it.first,
          std::make_unique<HttpHeadersRequestTestJob>(
              it.second->expect_if_modified_since,
              it.second->expect_if_none_match, it.second->headers_allowed));
    }

    cache_helper_ = std::make_unique<AppCacheCacheTestHelper>(
        service_.get(), group_->manifest_url(), cache, std::move(cache_entries),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));
    cache_helper_->Write();

    post_update_finished_cb_ = base::BindOnce(
        &AppCacheUpdateJobTest::RequestResponseTimesCorruptionUpdateFinished,
        base::Unretained(this), expect_modified);

    // Start update after data write completes asynchronously.
    // After update is finished, continues async in
    // |RequestResponseTimesCorruptionUpdateFinished|.
  }

  void RequestResponseTimesCorruptionUpdateFinished(bool expect_modified) {
    // After cache write was complete, we note that we expect an item to be
    // copied for the not modified case.
    if (!expect_modified) {
      expect_response_ids_.insert(std::map<GURL, int64_t>::value_type(
          MockHttpServer::GetMockUrl("files/maybemodified"),
          expect_old_cache_
              ->GetEntry(MockHttpServer::GetMockUrl("files/maybemodified"))
              ->response_id()));  // copied
    }
    ASSERT_NE(group_->newest_complete_cache(), cache_helper_->write_cache());
    ASSERT_NE(group_->newest_complete_cache(), nullptr);
    cache_helper_->PrepareForRead(
        group_->newest_complete_cache(),
        base::BindOnce(
            &AppCacheUpdateJobTest::RequestResponseTimesCorruptionReadFinished,
            base::Unretained(this), expect_modified));
    cache_helper_->Read();
    // Continues async in |RequestResponseTimesCorruptionReadFinished|.
  }

  void RequestResponseTimesCorruptionReadFinished(bool expect_modified) {
    std::string resource_name;
    resource_name = "files/maybemodified";
    auto it = cache_helper_->read_cache_entries().find(
        MockHttpServer::GetMockUrl(resource_name));
    ASSERT_NE(it, cache_helper_->read_cache_entries().end());
    if (expect_modified) {
      // Verify that the cache body on the entry matches the expected mock
      // return body.
      CHECK_EQ(it->second->body, "modified");
      CHECK_GT(it->second->response_info->request_time,
               base::Time::Now() - kOneHour);
      CHECK_GT(it->second->response_info->response_time,
               base::Time::Now() - kOneHour);
    } else {
      CHECK_EQ(it->second->body, "cached-maybemodified");
      CHECK_EQ(it->second->response_info->request_time, base::Time());
      CHECK_EQ(it->second->response_info->response_time, base::Time());
    }
    TriggerTestComplete();
    // Continues async in |TestComplete|.
  }

  void IfNoneMatchRefetchTest() {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://headertest"), 111);
    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(), std::make_unique<HttpHeadersRequestTestJob>(
                                    std::string(), "\"LadeDade\""));
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Simulate a refetch manifest request that uses an ETag header.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(data, base::size(data)));

    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_ = std::move(response_info);
    update->internal_state_ = AppCacheUpdateJobState::REFETCH_MANIFEST;
    update->RefetchManifest();

    // We need to wait for the URL load requests to make it to the
    // URLLoaderInterceptor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheUpdateJobTest::VerifyHeadersAndDeleteUpdate,
                       base::Unretained(this), update));

    TriggerTestComplete();
  }

  void MultipleHeadersRefetchTest() {
    // Verify that code is correct when building multiple extra headers.
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL("http://headertest"), 111);
    http_headers_request_test_jobs_.emplace(
        group_->manifest_url(),
        std::make_unique<HttpHeadersRequestTestJob>(
            "Sat, 29 Oct 1994 19:43:31 GMT", "\"LadeDade\""));
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Simulate a refetch manifest request that uses an ETag header.
    const char data[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "ETag: \"LadeDade\"\0"
        "\0";
    auto response_info = std::make_unique<net::HttpResponseInfo>();
    response_info->headers = base::MakeRefCounted<net::HttpResponseHeaders>(
        std::string(data, base::size(data)));

    group_->update_status_ = AppCacheGroup::DOWNLOADING;
    update->manifest_response_info_ = std::move(response_info);
    update->internal_state_ = AppCacheUpdateJobState::REFETCH_MANIFEST;
    update->RefetchManifest();

    // We need to wait for the URL load requests to make it to the
    // URLLoaderInterceptor.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&AppCacheUpdateJobTest::VerifyHeadersAndDeleteUpdate,
                       base::Unretained(this), update));

    TriggerTestComplete();
  }

  void CrossOriginHttpsSuccessTest() {
    GURL manifest_url = MockHttpServer::GetMockHttpsUrl(
        "files/valid_cross_origin_https_manifest");

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    tested_manifest_ = NONE;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void CrossOriginHttpsDeniedTest() {
    GURL manifest_url = MockHttpServer::GetMockHttpsUrl(
        "files/invalid_cross_origin_https_manifest");

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    tested_manifest_ = NONE;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void OriginTrialUpdateTest() {
    // Get the expected expiration date of the test token.
    {
      blink::TrialTokenValidator validator;
      blink::TrialTokenResult result = validator.ValidateToken(
          kTestAppCacheOriginTrialToken, MockHttpServer::GetOrigin(),
          base::Time::Now());
      expect_token_expires_ = result.expiry_time;
      ASSERT_EQ(result.status, blink::OriginTrialTokenStatus::kSuccess);
      EXPECT_EQ(GetAppCacheOriginTrialNameForTesting(), result.feature_name);
      EXPECT_NE(base::Time(), expect_token_expires_);
    }

    MakeService();
    tested_manifest_path_override_ = "files/manifest2-origin-trial";
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(),
        MockHttpServer::GetMockUrl(tested_manifest_path_override_),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        response_writer_->response_id());
    MockFrontend* frontend1 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    host1->AssociateCompleteCache(cache);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = MANIFEST2_WITH_FILES_SCOPE;

    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);  // final
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with identical manifest2 contents
    const std::string seed_data(kManifest2Contents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void OriginTrialRequiredNoTokenTest() {
    GURL manifest_url = MockHttpServer::GetMockUrl("files/manifest1");

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    ASSERT_TRUE(group_->last_full_update_check_time().is_null());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = false;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  void WaitForUpdateToFinish() {
    if (group_->update_status() == AppCacheGroup::IDLE)
      UpdateFinished();
    else
      group_->AddUpdateObserver(this);
  }

  void OnUpdateComplete(AppCacheGroup* group) override {
    ASSERT_EQ(group_.get(), group);
    protect_newest_cache_ = group->newest_complete_cache();
    UpdateFinished();
  }

  void UpdateFinished() {
    if (post_update_finished_cb_) {
      std::move(post_update_finished_cb_).Run();
      return;
    }

    TriggerTestComplete();
  }

  void TriggerTestComplete() {
    // We unwind the stack prior to finishing up to let stack-based objects
    // get deleted.
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&AppCacheUpdateJobTest::TestComplete,
                                  base::Unretained(this)));
  }

  void TestComplete() {
    EXPECT_EQ(AppCacheGroup::IDLE, group_->update_status());
    EXPECT_TRUE(group_->update_job() == nullptr);
    if (do_checks_after_update_finished_)
      VerifyExpectations();

    // Clean up everything that was created on the IO thread.
    cache_helper_.reset();
    protect_newest_cache_ = nullptr;
    group_ = nullptr;
    hosts_.clear();
    frontends_.clear();
    response_infos_.clear();
    service_.reset(nullptr);

    std::move(test_completed_cb_).Run();
  }

  void MakeService() {
    service_ = std::make_unique<MockAppCacheService>(
        weak_partition_factory_->GetWeakPtr());
  }

  AppCache* MakeCacheForGroup(int64_t cache_id, int64_t manifest_response_id) {
    return MakeCacheForGroup(cache_id, group_->manifest_url(),
                             manifest_response_id);
  }

  AppCache* MakeCacheForGroup(int64_t cache_id,
                              const GURL& manifest_entry_url,
                              int64_t manifest_response_id) {
    AppCache* cache = new AppCache(service_->storage(), cache_id);
    cache->set_complete(true);
    cache->set_manifest_parser_version(2);
    cache->set_manifest_scope("/");
    cache->set_update_time(base::Time::Now() - kOneHour);
    group_->AddCache(cache);
    group_->set_last_full_update_check_time(cache->update_time());

    // Add manifest entry to cache.
    if (manifest_response_id >= 0) {
      cache->AddEntry(manifest_entry_url, AppCacheEntry(AppCacheEntry::MANIFEST,
                                                        manifest_response_id));
    }

    // Specific tests that expect a newer time should set
    // expect_full_update_time_newer_than_ which causes this
    // equality expectation to be ignored.
    expect_full_update_time_equal_to_ = cache->update_time();

    return cache;
  }

  AppCacheHost* MakeHost(blink::mojom::AppCacheFrontend* frontend) {
    constexpr int kRenderFrameIdForTests = 456;
    hosts_.push_back(std::make_unique<AppCacheHost>(
        base::UnguessableToken::Create(), process_id_, kRenderFrameIdForTests,
        ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
            process_id_),
        mojo::NullRemote(), service_.get()));
    hosts_.back()->set_frontend_for_testing(frontend);
    return hosts_.back().get();
  }

  AppCacheResponseInfo* MakeAppCacheResponseInfo(
      const GURL& manifest_url,
      int64_t response_id,
      const std::string& raw_headers) {
    std::unique_ptr<net::HttpResponseInfo> http_info =
        std::make_unique<net::HttpResponseInfo>();
    http_info->headers =
        base::MakeRefCounted<net::HttpResponseHeaders>(raw_headers);
    auto info = base::MakeRefCounted<AppCacheResponseInfo>(
        service_->storage()->GetWeakPtr(), manifest_url, response_id,
        std::move(http_info), 0);
    response_infos_.emplace_back(info);
    return info.get();
  }

  MockFrontend* MakeMockFrontend() {
    frontends_.push_back(std::make_unique<MockFrontend>());
    return frontends_.back().get();
  }

  // Verifies conditions about the group and notifications after an update
  // has finished. Cannot verify update job internals as update is deleted.
  void VerifyExpectations() {
    RetryRequestTestJob::Verify();
    for (auto& kvp : http_headers_request_test_jobs_) {
      const GURL& test_job_url = kvp.first;
      HttpHeadersRequestTestJob* test_job = kvp.second.get();
      test_job->Verify(test_job_url);
    }

    EXPECT_EQ(expect_group_obsolete_, group_->is_obsolete());
    EXPECT_EQ(expect_group_is_being_deleted_ || expect_eviction_,
              group_->is_being_deleted());

    if (!expect_eviction_) {
      EXPECT_EQ(expect_evictable_error_,
                !group_->first_evictable_error_time().is_null());
      if (expect_evictable_error_) {
        MockAppCacheStorage* storage =
            static_cast<MockAppCacheStorage*>(service_->storage());
        EXPECT_EQ(group_->first_evictable_error_time(),
                  storage->stored_eviction_times_[group_->group_id()].second);
      }
    }

    if (!expect_full_update_time_newer_than_.is_null()) {
      EXPECT_LT(expect_full_update_time_newer_than_,
                group_->last_full_update_check_time());
    } else if (!expect_full_update_time_equal_to_.is_null()) {
      EXPECT_EQ(expect_full_update_time_equal_to_,
                group_->last_full_update_check_time());
    }

    if (expect_group_has_cache_) {
      ASSERT_TRUE(group_->newest_complete_cache() != nullptr);
      EXPECT_EQ(group_->newest_complete_cache()->manifest_parser_version(), 2);

      if (expect_non_null_update_time_)
        EXPECT_TRUE(!group_->newest_complete_cache()->update_time().is_null());

      if (expect_old_cache_) {
        EXPECT_NE(expect_old_cache_, group_->newest_complete_cache());
        EXPECT_TRUE(base::Contains(group_->old_caches(), expect_old_cache_));
      }
      if (expect_newest_cache_) {
        EXPECT_EQ(expect_newest_cache_, group_->newest_complete_cache());
        EXPECT_FALSE(
            base::Contains(group_->old_caches(), expect_newest_cache_));
      } else {
        // Tests that don't know which newest cache to expect contain updates
        // that succeed (because the update creates a new cache whose pointer
        // is unknown to the test). Check group and newest cache were stored
        // when update succeeds.
        MockAppCacheStorage* storage =
            static_cast<MockAppCacheStorage*>(service_->storage());
        EXPECT_TRUE(storage->IsGroupStored(group_.get()));
        EXPECT_TRUE(storage->IsCacheStored(group_->newest_complete_cache()));

        // Check that all entries in the newest cache were stored.
        for (const auto& pair : group_->newest_complete_cache()->entries()) {
          EXPECT_NE(blink::mojom::kAppCacheNoResponseId,
                    pair.second.response_id());

          // Check that any copied entries have the expected response id
          // and that entries that are not copied have a different response id.
          auto found = expect_response_ids_.find(pair.first);
          if (found != expect_response_ids_.end()) {
            EXPECT_EQ(found->second, pair.second.response_id());
          } else if (expect_old_cache_) {
            AppCacheEntry* old_entry = expect_old_cache_->GetEntry(pair.first);
            if (old_entry)
              EXPECT_NE(old_entry->response_id(), pair.second.response_id());
          }
        }
      }
    } else {
      EXPECT_TRUE(group_->newest_complete_cache() == nullptr);
    }

    // Verify token_expires times on cache.
    if (expect_group_has_cache_) {
      AppCache* cache = group_->newest_complete_cache();
      ASSERT_TRUE(cache);
      EXPECT_EQ(cache->token_expires(), expect_token_expires_);
    }

    // Check expected events.
    for (const std::unique_ptr<MockFrontend>& frontend : frontends_) {
      MockFrontend::RaisedEvents& expected_events = frontend->expected_events_;
      MockFrontend::RaisedEvents& actual_events = frontend->raised_events_;
      ASSERT_EQ(expected_events.size(), actual_events.size());

      // Check each expected event.
      for (size_t j = 0; j < expected_events.size() && j < actual_events.size();
           ++j) {
        EXPECT_EQ(expected_events[j], actual_events[j]);
      }

      if (!frontend->expected_error_message_.empty()) {
        EXPECT_EQ(frontend->expected_error_message_, frontend->error_message_);
      }
    }

    // Verify expected cache contents last as some checks are asserts
    // and will abort the test if they fail.
    if (tested_manifest_) {
      AppCache* cache = group_->newest_complete_cache();
      ASSERT_TRUE(cache != nullptr);
      EXPECT_EQ(group_.get(), cache->owning_group());
      EXPECT_TRUE(cache->is_complete());

      switch (tested_manifest_) {
        case MANIFEST1:
          VerifyManifest1(cache);
          break;
        case MANIFEST1_WITH_NOTMODIFIED:
          VerifyManifest1WithNotModified(cache);
          break;
        case MANIFEST1_WITH_MAYBEMODIFIED:
          VerifyManifest1WithMaybeModified(cache);
          break;
        case MANIFEST2_WITH_ROOT_SCOPE:
          VerifyManifest2WithRootScope(cache);
          break;
        case MANIFEST2_WITH_FILES_SCOPE:
          VerifyManifest2WithFilesScope(cache);
          break;
        case MANIFEST_MERGED_TYPES:
          VerifyManifestMergedTypes(cache);
          break;
        case EMPTY_MANIFEST:
          VerifyEmptyManifest(cache);
          break;
        case EMPTY_FILE_MANIFEST:
          VerifyEmptyFileManifest(cache);
          break;
        case PENDING_MASTER_NO_UPDATE:
          VerifyMasterEntryNoUpdate(cache);
          break;
        case MANIFEST_WITH_INTERCEPT:
          VerifyManifestWithIntercept(cache);
          break;
        case SCOPE_MANIFEST_ROOT:
          VerifyScopeManifest(cache);
          break;
        case SCOPE_MANIFEST_BAR:
          VerifyScopeManifest(cache);
          break;
        case SCOPE_MANIFEST_OTHER:
          VerifyScopeManifest(cache);
          break;
        case NONE:
        default:
          break;
      }
    }
  }

  void VerifyManifest1(AppCache* cache) {
    size_t expected = 3 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    const char* kManifestPath = tested_manifest_path_override_
                                    ? tested_manifest_path_override_
                                    : "files/manifest1";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::FALLBACK, entry->types());

    for (const auto& pair : expect_extra_entries_) {
      entry = cache->GetEntry(pair.first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(pair.second.types(), entry->types());
    }

    expected = 1;
    ASSERT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(
        cache->fallback_namespaces_[0] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("files/fallback1"),
                          MockHttpServer::GetMockUrl("files/fallback1a")));

    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifest1WithNotModified(AppCache* cache) {
    size_t expected = 4 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    const char* kManifestPath = tested_manifest_path_override_
                                    ? tested_manifest_path_override_
                                    : "files/manifest1-with-notmodified";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/notmodified"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::FALLBACK, entry->types());

    for (const auto& pair : expect_extra_entries_) {
      entry = cache->GetEntry(pair.first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(pair.second.types(), entry->types());
    }

    expected = 1;
    ASSERT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(
        cache->fallback_namespaces_[0] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("files/fallback1"),
                          MockHttpServer::GetMockUrl("files/fallback1a")));

    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifest1WithMaybeModified(AppCache* cache) {
    size_t expected = 4 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    const char* kManifestPath = tested_manifest_path_override_
                                    ? tested_manifest_path_override_
                                    : "files/manifest1-with-maybemodified";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/maybemodified"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::FALLBACK, entry->types());

    for (const auto& pair : expect_extra_entries_) {
      entry = cache->GetEntry(pair.first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(pair.second.types(), entry->types());
    }

    expected = 1;
    ASSERT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(
        cache->fallback_namespaces_[0] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("files/fallback1"),
                          MockHttpServer::GetMockUrl("files/fallback1a")));

    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifest2WithRootScope(AppCache* cache) {
    size_t expected = 6 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    const char* kManifestPath = tested_manifest_path_override_
                                    ? tested_manifest_path_override_
                                    : "files/manifest2-with-root-override";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsManifest());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsFallback());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/fallback2a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsFallback());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/intercept1a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsIntercept());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/intercept2a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsIntercept());

    for (const auto& pair : expect_extra_entries_) {
      entry = cache->GetEntry(pair.first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(pair.second.types(), entry->types());
    }

    expected = 2;
    ASSERT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(
        cache->fallback_namespaces_[0] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("files/fallback1"),
                          MockHttpServer::GetMockUrl("files/fallback1a")));
    EXPECT_TRUE(
        cache->fallback_namespaces_[1] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("bar/fallback2"),
                          MockHttpServer::GetMockUrl("files/fallback2a")));

    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifest2WithFilesScope(AppCache* cache) {
    size_t expected = 4 + expect_extra_entries_.size();
    EXPECT_EQ(expected, cache->entries().size());
    const char* kManifestPath = tested_manifest_path_override_
                                    ? tested_manifest_path_override_
                                    : "files/manifest2";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    EXPECT_TRUE(entry->IsManifest());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsExplicit());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/intercept1a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsIntercept());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/fallback1a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsFallback());

    for (const auto& pair : expect_extra_entries_) {
      entry = cache->GetEntry(pair.first);
      ASSERT_TRUE(entry);
      EXPECT_EQ(pair.second.types(), entry->types());
    }

    expected = 1;
    ASSERT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(
        cache->fallback_namespaces_[0] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("files/fallback1"),
                          MockHttpServer::GetMockUrl("files/fallback1a")));

    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifestMergedTypes(AppCache* cache) {
    size_t expected = 2;
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/manifest-merged-types"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::MANIFEST,
              entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::FALLBACK |
                  AppCacheEntry::MASTER,
              entry->types());

    expected = 1;
    ASSERT_EQ(expected, cache->fallback_namespaces_.size());
    EXPECT_TRUE(
        cache->fallback_namespaces_[0] ==
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("files/fallback1"),
                          MockHttpServer::GetMockUrl("files/explicit1")));

    EXPECT_EQ(expected, cache->online_safelist_namespaces_.size());
    EXPECT_TRUE(cache->online_safelist_namespaces_[0] ==
                AppCacheNamespace(APPCACHE_NETWORK_NAMESPACE,
                                  MockHttpServer::GetMockUrl("files/online1"),
                                  GURL()));
    EXPECT_FALSE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyEmptyManifest(AppCache* cache) {
    const char* kManifestPath = tested_manifest_path_override_
                                    ? tested_manifest_path_override_
                                    : "files/empty-manifest";
    size_t expected = 1;
    EXPECT_EQ(expected, cache->entries().size());
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyEmptyFileManifest(AppCache* cache) {
    EXPECT_EQ(size_t(2), cache->entries().size());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl("files/empty-file-manifest"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/empty1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyMasterEntryNoUpdate(AppCache* cache) {
    EXPECT_EQ(size_t(3), cache->entries().size());
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl("files/notmodified"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit1"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MASTER, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/explicit2"));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::EXPLICIT | AppCacheEntry::MASTER, entry->types());
    EXPECT_TRUE(entry->has_response_id());

    EXPECT_TRUE(cache->fallback_namespaces_.empty());
    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_safelist_all_);

    EXPECT_TRUE(cache->update_time_ > base::Time());
  }

  void VerifyManifestWithIntercept(AppCache* cache) {
    EXPECT_EQ(2u, cache->entries().size());
    const char* kManifestPath = "files/manifest-with-intercept";
    AppCacheEntry* entry =
        cache->GetEntry(MockHttpServer::GetMockUrl(kManifestPath));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());
    entry = cache->GetEntry(MockHttpServer::GetMockUrl("files/intercept1a"));
    ASSERT_TRUE(entry);
    EXPECT_TRUE(entry->IsIntercept());
  }

  bool CheckNamespaceExists(const std::vector<AppCacheNamespace>& namespaces,
                            const AppCacheNamespace& namespace_entry) {
    for (const AppCacheNamespace& appcache_namespace : namespaces) {
      if (appcache_namespace == namespace_entry)
        return true;
    }
    return false;
  }

  std::vector<AppCacheNamespace> GetExpectedScopeIntercepts() {
    std::vector<AppCacheNamespace> expected;

    // Set up the expected intercepts.
    std::vector<AppCacheNamespace> expected_root = {
        AppCacheNamespace(APPCACHE_INTERCEPT_NAMESPACE,
                          MockHttpServer::GetMockUrl(""),
                          MockHttpServer::GetMockUrl("intercept-newroot/foo")),
    };
    std::vector<AppCacheNamespace> expected_bar = {
        AppCacheNamespace(APPCACHE_INTERCEPT_NAMESPACE,
                          MockHttpServer::GetMockUrl("bar/foo"),
                          MockHttpServer::GetMockUrl("intercept-newbar/foo")),
        AppCacheNamespace(
            APPCACHE_INTERCEPT_NAMESPACE,
            MockHttpServer::GetMockUrl("bar/baz/foo"),
            MockHttpServer::GetMockUrl("intercept-newbar/newbaz/foo")),
    };
    std::vector<AppCacheNamespace> expected_other = {
        AppCacheNamespace(APPCACHE_INTERCEPT_NAMESPACE,
                          MockHttpServer::GetMockUrl("other/foo"),
                          MockHttpServer::GetMockUrl("intercept-newother/foo")),
    };

    switch (tested_manifest_) {
      case SCOPE_MANIFEST_ROOT:
        expected.insert(expected.end(), std::begin(expected_root),
                        std::end(expected_root));
        expected.insert(expected.end(), std::begin(expected_bar),
                        std::end(expected_bar));
        expected.insert(expected.end(), std::begin(expected_other),
                        std::end(expected_other));
        break;
      case SCOPE_MANIFEST_BAR:
        expected.insert(expected.end(), std::begin(expected_bar),
                        std::end(expected_bar));
        break;
      case SCOPE_MANIFEST_OTHER:
        expected.insert(expected.end(), std::begin(expected_other),
                        std::end(expected_other));
        break;
      default:
        NOTREACHED();
    }

    return expected;
  }

  std::vector<AppCacheNamespace> GetExpectedScopeFallbacks() {
    std::vector<AppCacheNamespace> expected;

    // Set up the expected fallbacks.
    std::vector<AppCacheNamespace> expected_root = {
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl(""),
                          MockHttpServer::GetMockUrl("fallback-newroot/foo")),
    };
    std::vector<AppCacheNamespace> expected_bar = {
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("bar/foo"),
                          MockHttpServer::GetMockUrl("fallback-newbar/foo")),
        AppCacheNamespace(
            APPCACHE_FALLBACK_NAMESPACE,
            MockHttpServer::GetMockUrl("bar/baz/foo"),
            MockHttpServer::GetMockUrl("fallback-newbar/newbaz/foo")),
    };
    std::vector<AppCacheNamespace> expected_other = {
        AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE,
                          MockHttpServer::GetMockUrl("other/foo"),
                          MockHttpServer::GetMockUrl("fallback-newother/foo")),
    };

    switch (tested_manifest_) {
      case SCOPE_MANIFEST_ROOT:
        expected.insert(expected.end(), std::begin(expected_root),
                        std::end(expected_root));
        expected.insert(expected.end(), std::begin(expected_bar),
                        std::end(expected_bar));
        expected.insert(expected.end(), std::begin(expected_other),
                        std::end(expected_other));
        break;
      case SCOPE_MANIFEST_BAR:
        expected.insert(expected.end(), std::begin(expected_bar),
                        std::end(expected_bar));
        break;
      case SCOPE_MANIFEST_OTHER:
        expected.insert(expected.end(), std::begin(expected_other),
                        std::end(expected_other));
        break;
      default:
        NOTREACHED();
    }

    return expected;
  }

  void VerifyScopeManifest(AppCache* cache) {
    std::vector<AppCacheNamespace> expected_intercepts =
        GetExpectedScopeIntercepts();
    std::vector<AppCacheNamespace> expected_fallbacks =
        GetExpectedScopeFallbacks();

    // Verify the number of cache entries.  There should be cache entries for
    // the manifest, all of the expected intercepts (because each defines a
    // separate target), and all of the expected fallbacks (because each also
    // defines a separate target).
    size_t expected_size =
        1 + expected_intercepts.size() + expected_fallbacks.size();
    EXPECT_EQ(expected_size, cache->entries().size());

    // Verify basic cache details.
    EXPECT_TRUE(cache->online_safelist_namespaces_.empty());
    EXPECT_FALSE(cache->online_safelist_all_);
    EXPECT_TRUE(cache->update_time_ > base::Time());

    // Verify manifest.
    ASSERT_TRUE(!std::string(tested_manifest_path_override_).empty());
    AppCacheEntry* entry = cache->GetEntry(
        MockHttpServer::GetMockUrl(tested_manifest_path_override_));
    ASSERT_TRUE(entry);
    EXPECT_EQ(AppCacheEntry::MANIFEST, entry->types());

    // Verify intercepts.
    ASSERT_EQ(expected_intercepts.size(), cache->intercept_namespaces_.size());
    for (auto it = expected_intercepts.begin(); it != expected_intercepts.end();
         ++it) {
      EXPECT_EQ(it->type, APPCACHE_INTERCEPT_NAMESPACE);
      entry = cache->GetEntry(it->target_url);
      ASSERT_TRUE(entry);
      EXPECT_TRUE(entry->IsIntercept());
      EXPECT_TRUE(CheckNamespaceExists(cache->intercept_namespaces_, *it));
    }

    // Verify fallbacks.
    for (auto it = expected_fallbacks.begin(); it != expected_fallbacks.end();
         ++it) {
      EXPECT_EQ(it->type, APPCACHE_FALLBACK_NAMESPACE);
      entry = cache->GetEntry(it->target_url);
      ASSERT_TRUE(entry);
      EXPECT_TRUE(entry->IsFallback());
      EXPECT_TRUE(CheckNamespaceExists(cache->fallback_namespaces_, *it));
    }
  }

 private:
  // Various manifest files used in this test.
  enum TestedManifest {
    NONE,
    MANIFEST1,
    MANIFEST1_WITH_MAYBEMODIFIED,
    MANIFEST1_WITH_NOTMODIFIED,
    MANIFEST2_WITH_ROOT_SCOPE,
    MANIFEST2_WITH_FILES_SCOPE,
    MANIFEST_MERGED_TYPES,
    EMPTY_MANIFEST,
    EMPTY_FILE_MANIFEST,
    PENDING_MASTER_NO_UPDATE,
    MANIFEST_WITH_INTERCEPT,
    SCOPE_MANIFEST_ROOT,
    SCOPE_MANIFEST_BAR,
    SCOPE_MANIFEST_OTHER,
  };

  void ScopeTest(const char* tested_manifest_path,
                 const TestedManifest& tested_manifest) {
    GURL manifest_url = MockHttpServer::GetMockUrl(tested_manifest_path);

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    ASSERT_TRUE(group_->last_full_update_check_time().is_null());

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_full_update_time_newer_than_ = base::Time::Now() - kOneHour;
    tested_manifest_ = tested_manifest;
    tested_manifest_path_override_ = tested_manifest_path;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    // Other events are only sent to associated hosts.

    WaitForUpdateToFinish();
  }

  void AddExpectedProgressEvents(MockFrontend* frontend,
                                 const int& number_of_progress_events) {
    for (auto i = 0; i < number_of_progress_events; ++i) {
      frontend->AddExpectedEvent(
          blink::mojom::AppCacheEventID::APPCACHE_PROGRESS_EVENT);
    }
  }

  void Scope304Test(const char* tested_manifest_path,
                    const std::string& previous_scope,
                    const TestedManifest& tested_manifest) {
    GURL manifest_url = MockHttpServer::GetMockUrl(tested_manifest_path);

    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), manifest_url, service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    // Create response writer to get a response id.
    response_writer_ =
        service_->storage()->CreateResponseWriter(group_->manifest_url());

    int64_t manifest_response_id = response_writer_->response_id();
    AppCache* cache = MakeCacheForGroup(service_->storage()->NewCacheId(),
                                        manifest_response_id);
    cache->set_manifest_scope(previous_scope);
    MockFrontend* frontend1 = MakeMockFrontend();
    MockFrontend* frontend2 = MakeMockFrontend();
    AppCacheHost* host1 = MakeHost(frontend1);
    AppCacheHost* host2 = MakeHost(frontend2);
    host1->AssociateCompleteCache(cache);
    host2->AssociateCompleteCache(cache);

    // Seed the response_info working set with canned data so that an existing
    // manifest is reused by the update job.
    const char kData[] =
        "HTTP/1.1 200 OK\0"
        "Last-Modified: Sat, 29 Oct 1994 19:43:31 GMT\0"
        "\0";
    const std::string kRawHeaders(kData, base::size(kData));
    MakeAppCacheResponseInfo(manifest_url, manifest_response_id, kRawHeaders);

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = true;
    expect_old_cache_ = cache;
    tested_manifest_ = tested_manifest;
    tested_manifest_path_override_ = tested_manifest_path;

    int number_of_progress_events = 0;
    switch (tested_manifest_) {
      case SCOPE_MANIFEST_ROOT:
        number_of_progress_events = 9;
        break;
      case SCOPE_MANIFEST_BAR:
        number_of_progress_events = 5;
        break;
      case SCOPE_MANIFEST_OTHER:
        number_of_progress_events = 3;
        break;
      default:
        NOTREACHED();
    }

    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    AddExpectedProgressEvents(frontend1, number_of_progress_events);
    frontend1->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT);
    AddExpectedProgressEvents(frontend2, number_of_progress_events);
    frontend2->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_UPDATE_READY_EVENT);

    // Seed storage with kScopeManifestContents so a possible scope change will
    // result in reading this manifest data from the cache and re-parsing it
    // with the new scope.
    const std::string seed_data(kScopeManifestContents);
    scoped_refptr<net::StringIOBuffer> io_buffer =
        base::MakeRefCounted<net::StringIOBuffer>(seed_data);
    response_writer_->WriteData(
        io_buffer.get(), seed_data.length(),
        base::BindOnce(
            &AppCacheUpdateJobTest::StartUpdateAfterSeedingStorageData,
            base::Unretained(this)));

    // Start update after data write completes asynchronously.
  }

  void RetryRequestTest(bool expect_group_has_cache) {
    MakeService();
    group_ = base::MakeRefCounted<AppCacheGroup>(
        service_->storage(), GURL(RetryRequestTestJob::kRetryUrl),
        service_->storage()->NewGroupId());
    AppCacheUpdateJob* update =
        new AppCacheUpdateJob(service_.get(), group_.get());
    group_->update_job_ = update;

    MockFrontend* frontend = MakeMockFrontend();
    AppCacheHost* host = MakeHost(frontend);
    update->StartUpdate(host, GURL());

    // Set up checks for when update job finishes.
    do_checks_after_update_finished_ = true;
    expect_group_obsolete_ = false;
    expect_group_has_cache_ = expect_group_has_cache;
    frontend->AddExpectedEvent(
        blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT);

    WaitForUpdateToFinish();
  }

  content::BrowserTaskEnvironment task_environment_;

  std::unique_ptr<MockAppCacheService> service_;
  scoped_refptr<AppCacheGroup> group_;
  scoped_refptr<AppCache> protect_newest_cache_;
  base::OnceClosure test_completed_cb_;
  base::OnceClosure post_update_finished_cb_;

  std::unique_ptr<AppCacheResponseWriter> response_writer_;
  std::unique_ptr<AppCacheCacheTestHelper> cache_helper_;

  // Hosts used by an async test that need to live until update job finishes.
  // Otherwise, test can put host on the stack instead of here.
  std::vector<std::unique_ptr<AppCacheHost>> hosts_;

  // Response infos used by an async test that need to live until update job
  // finishes.
  std::vector<scoped_refptr<AppCacheResponseInfo>> response_infos_;

  // Flag indicating if test cares to verify the update after update finishes.
  bool do_checks_after_update_finished_;
  bool expect_group_obsolete_;
  bool expect_group_has_cache_;
  bool expect_group_is_being_deleted_;
  bool expect_evictable_error_;
  bool expect_eviction_;
  base::Time expect_full_update_time_newer_than_;
  base::Time expect_full_update_time_equal_to_;
  base::Time expect_token_expires_;
  AppCache* expect_old_cache_;
  AppCache* expect_newest_cache_;
  bool expect_non_null_update_time_;
  std::vector<std::unique_ptr<MockFrontend>>
      frontends_;  // to check expected events
  TestedManifest tested_manifest_;
  const char* tested_manifest_path_override_;
  AppCache::EntryMap expect_extra_entries_;
  std::map<GURL, int64_t> expect_response_ids_;

  URLLoaderInterceptor interceptor_;
  const int process_id_;
  std::map<GURL, std::unique_ptr<HttpHeadersRequestTestJob>>
      http_headers_request_test_jobs_;

  base::test::ScopedFeatureList appcache_require_origin_trial_feature_;

  // Lazily create these to avoid data races in the FeatureList between
  // service workers (reading) and appcache tests (writing).
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  std::unique_ptr<base::WeakPtrFactory<StoragePartitionImpl>>
      weak_partition_factory_;
};

class AppCacheUpdateJobOriginTrialTest : public AppCacheUpdateJobTest {
 public:
  AppCacheUpdateJobOriginTrialTest() {
    scoped_feature_list_.InitAndEnableFeature(
        blink::features::kAppCacheRequireOriginTrial);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AppCacheUpdateJobNoUpdateOn304Test : public AppCacheUpdateJobTest {
 public:
  AppCacheUpdateJobNoUpdateOn304Test() = default;
};

class AppCacheUpdateJobWithCorruptionRecoveryTest
    : public AppCacheUpdateJobTest {
 public:
  AppCacheUpdateJobWithCorruptionRecoveryTest() {
    scoped_feature_list_.InitAndEnableFeature(
        kAppCacheCorruptionRecoveryFeature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class AppCacheUpdateJobWithoutCorruptionRecoveryTest
    : public AppCacheUpdateJobTest {
 public:
  AppCacheUpdateJobWithoutCorruptionRecoveryTest() {
    scoped_feature_list_.InitAndDisableFeature(
        kAppCacheCorruptionRecoveryFeature);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(AppCacheUpdateJobTest, AlreadyChecking) {
  MockAppCacheService service;
  auto group = base::MakeRefCounted<AppCacheGroup>(
      service.storage(), GURL("http://manifesturl.com"),
      service.storage()->NewGroupId());

  AppCacheUpdateJob update(&service, group.get());

  // Pretend group is in checking state.
  group->update_job_ = &update;
  group->update_status_ = AppCacheGroup::CHECKING;

  update.StartUpdate(nullptr, GURL());
  EXPECT_EQ(AppCacheGroup::CHECKING, group->update_status());

  MockFrontend mock_frontend;
  const int kMockProcessId1 = 1;
  AppCacheHost host(/*host_id=*/base::UnguessableToken::Create(),
                    kMockProcessId1, /*render_frame_id=*/1,
                    ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
                        kMockProcessId1),
                    mojo::NullRemote(), &service);
  host.set_frontend_for_testing(&mock_frontend);
  update.StartUpdate(&host, GURL());

  MockFrontend::RaisedEvents events = mock_frontend.raised_events_;
  ASSERT_EQ(1u, events.size());
  EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT, events[0]);
  EXPECT_EQ(AppCacheGroup::CHECKING, group->update_status());
}

TEST_F(AppCacheUpdateJobTest, AlreadyDownloading) {
  MockAppCacheService service;
  auto group = base::MakeRefCounted<AppCacheGroup>(
      service.storage(), GURL("http://manifesturl.com"),
      service.storage()->NewGroupId());

  AppCacheUpdateJob update(&service, group.get());

  // Pretend group is in downloading state.
  group->update_job_ = &update;
  group->update_status_ = AppCacheGroup::DOWNLOADING;

  update.StartUpdate(nullptr, GURL());
  EXPECT_EQ(AppCacheGroup::DOWNLOADING, group->update_status());

  MockFrontend mock_frontend;
  const int kMockProcessId1 = 1;
  AppCacheHost host(/*host_id=*/base::UnguessableToken::Create(),
                    kMockProcessId1, /*render_frame_id=*/1,
                    ChildProcessSecurityPolicyImpl::GetInstance()->CreateHandle(
                        kMockProcessId1),
                    mojo::NullRemote(), &service);
  host.set_frontend_for_testing(&mock_frontend);
  update.StartUpdate(&host, GURL());

  MockFrontend::RaisedEvents events = mock_frontend.raised_events_;
  ASSERT_EQ(2u, events.size());
  EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_CHECKING_EVENT, events[0]);
  EXPECT_EQ(blink::mojom::AppCacheEventID::APPCACHE_DOWNLOADING_EVENT,
            events[1]);

  EXPECT_EQ(AppCacheGroup::DOWNLOADING, group->update_status());
}

TEST_F(AppCacheUpdateJobTest, StartCacheAttempt) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::StartCacheAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpgradeAttempt) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::StartUpgradeAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptFetchManifestFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::CacheAttemptFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFetchManifestFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestRedirect) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::ManifestRedirectTest);
}

TEST_F(AppCacheUpdateJobTest, ManifestMissingMimeTypeTest) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::ManifestMissingMimeTypeTest);
}

TEST_F(AppCacheUpdateJobOriginTrialTest, ManifestNotFound) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::ManifestNotFoundTest);
}

TEST_F(AppCacheUpdateJobOriginTrialTest, ManifestGoneFetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::ManifestGoneFetchTest);
}

TEST_F(AppCacheUpdateJobOriginTrialTest, ManifestGoneUpgrade) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::ManifestGoneUpgradeTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptNotModified) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::CacheAttemptNotModifiedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeNotModified) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeNotModifiedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeNotModifiedVersion1) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeNotModifiedVersion1Test);
}

TEST_F(AppCacheUpdateJobTest, UpgradeManifestDataChangedScopeUnchanged) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::UpgradeManifestDataChangedScopeUnchangedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeManifestDataChangedScopeChanged) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::UpgradeManifestDataChangedScopeChangedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeManifestDataUnchangedScopeUnchanged) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::UpgradeManifestDataUnchangedScopeUnchangedTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeManifestDataUnchangedScopeChanged) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::UpgradeManifestDataUnchangedScopeChangedTest);
}

TEST_F(AppCacheUpdateJobTest, Bug95101Test) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::Bug95101Test);
}

TEST_F(AppCacheUpdateJobTest, BasicCacheAttemptSuccess) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::BasicCacheAttemptSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestRootWithNoOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestRootWithNoOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestBarWithNoOverrideTest) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::ScopeManifestBarWithNoOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestRootWithRootOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestRootWithRootOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestBarWithRootOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestBarWithRootOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestRootWithBarOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestRootWithBarOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestBarWithBarOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestBarWithBarOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestRootWithOtherOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestRootWithOtherOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifestBarWithOtherOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifestBarWithOtherOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304RootWithNoOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304RootWithNoOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304BarWithNoOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304BarWithNoOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304RootWithRootOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304RootWithRootOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304BarWithRootOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304BarWithRootOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304RootWithBarOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304RootWithBarOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304BarWithBarOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304BarWithBarOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304RootWithOtherOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304RootWithOtherOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, ScopeManifest304BarWithOtherOverrideTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::ScopeManifest304BarWithOtherOverrideTest);
}

TEST_F(AppCacheUpdateJobTest, DownloadInterceptEntriesTest) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::DownloadInterceptEntriesTest);
}

TEST_F(AppCacheUpdateJobTest, BasicUpgradeSuccess) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::BasicUpgradeSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeLoadFromNewestCache) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeLoadFromNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeNoLoadFromNewestCache) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeNoLoadFromNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeLoadFromNewestCacheVaryHeader) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::UpgradeLoadFromNewestCacheVaryHeaderTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeLoadFromNewestCacheReuseVaryHeader) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::UpgradeLoadFromNewestCacheReuseVaryHeaderTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeSuccessMergedTypes) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeSuccessMergedTypesTest);
}

TEST_F(AppCacheUpdateJobTest, CacheAttemptFailUrlFetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::CacheAttemptFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailUrlFetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailMasterUrlFetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeFailMasterUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyManifest) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::EmptyManifestTest);
}

TEST_F(AppCacheUpdateJobTest, EmptyFile) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::EmptyFileTest);
}

TEST_F(AppCacheUpdateJobTest, RetryRetryAfter) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::RetryRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetryNoRetryAfter) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::RetryNoRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetryNonzeroRetryAfter) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::RetryNonzeroRetryAfterTest);
}

TEST_F(AppCacheUpdateJobTest, RetrySuccess) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::RetrySuccessTest);
}

TEST_F(AppCacheUpdateJobTest, RetryUrl) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::RetryUrlTest);
}

TEST_F(AppCacheUpdateJobTest, FailStoreNewestCache) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::FailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFailStoreNewestCacheTest) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::MasterEntryFailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeFailStoreNewestCache) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeFailStoreNewestCacheTest);
}

TEST_F(AppCacheUpdateJobOriginTrialTest, UpgradeFailMakeGroupObsolete) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeFailMakeGroupObsoleteTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFetchManifestFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntryFetchManifestFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryBadManifest) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntryBadManifestTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryManifestNotFound) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntryManifestNotFoundTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryFailUrlFetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntryFailUrlFetchTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryAllFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntryAllFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeMasterEntryAllFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeMasterEntryAllFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntrySomeFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntrySomeFailTest);
}

TEST_F(AppCacheUpdateJobTest, UpgradeMasterEntrySomeFail) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::UpgradeMasterEntrySomeFailTest);
}

TEST_F(AppCacheUpdateJobTest, MasterEntryNoUpdate) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MasterEntryNoUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidCacheAttempt) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::StartUpdateMidCacheAttemptTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidNoUpdate) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::StartUpdateMidNoUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, StartUpdateMidDownload) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::StartUpdateMidDownloadTest);
}

TEST_F(AppCacheUpdateJobTest, QueueMasterEntry) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::QueueMasterEntryTest);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedSinceCache) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::IfModifiedSinceTestCache);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedRefetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::IfModifiedTestRefetch);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedLastModified) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::IfModifiedTestLastModified);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedSinceUpgradeParserVersion0) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::IfModifiedSinceUpgradeParserVersion0Test);
}

TEST_F(AppCacheUpdateJobTest, IfModifiedSinceUpgradeParserVersion1) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::IfModifiedSinceUpgradeParserVersion1Test);
}

TEST_F(AppCacheUpdateJobTest, IfNoneMatchUpgradeParserVersion0) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::IfNoneMatchUpgradeParserVersion0Test);
}

TEST_F(AppCacheUpdateJobTest, IfNoneMatchUpgradeParserVersion1) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::IfNoneMatchUpgradeParserVersion1Test);
}

TEST_F(AppCacheUpdateJobTest, RequestResponseTimesAreSet) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::RequestResponseTimesAreSetTest);
}

TEST_F(AppCacheUpdateJobNoUpdateOn304Test, RequestResponseTimesAreNotModified) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::RequestResponseTimesAreNotModifiedTest);
}

TEST_F(AppCacheUpdateJobWithCorruptionRecoveryTest,
       RequestResponseTimesCorruptionFixed) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::RequestResponseTimesCorruptionFixedTest);
}

TEST_F(AppCacheUpdateJobWithoutCorruptionRecoveryTest,
       RequestResponseTimesCorruptionNotFixed) {
  RunTestOnUIThread(
      &AppCacheUpdateJobTest::RequestResponseTimesCorruptionNotFixedTest);
}

TEST_F(AppCacheUpdateJobTest, IfNoneMatchRefetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::IfNoneMatchRefetchTest);
}

TEST_F(AppCacheUpdateJobTest, MultipleHeadersRefetch) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::MultipleHeadersRefetchTest);
}

TEST_F(AppCacheUpdateJobTest, CrossOriginHttpsSuccess) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::CrossOriginHttpsSuccessTest);
}

TEST_F(AppCacheUpdateJobTest, CrossOriginHttpsDenied) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::CrossOriginHttpsDeniedTest);
}

TEST_F(AppCacheUpdateJobOriginTrialTest, OriginTrialUpdateWithFeature) {
  // Updating a manifest with a valid token should work
  // with or without the kAppCacheRequireOriginTrial feature.
  RunTestOnUIThread(&AppCacheUpdateJobTest::OriginTrialUpdateTest);
}

TEST_F(AppCacheUpdateJobTest, OriginTrialUpdateWithoutFeature) {
  // The default AppCacheUpdateJobTest disables the origin trial.
  RunTestOnUIThread(&AppCacheUpdateJobTest::OriginTrialUpdateTest);
}

TEST_F(AppCacheUpdateJobOriginTrialTest, OriginTrialRequiredNoToken) {
  RunTestOnUIThread(&AppCacheUpdateJobTest::OriginTrialRequiredNoTokenTest);
}

}  // namespace appcache_update_job_unittest
}  // namespace content
