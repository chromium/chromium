// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CONTENT_PUBLIC_TEST_KEEP_ALIVE_URL_LOADER_UTILS_H_
#define CONTENT_PUBLIC_TEST_KEEP_ALIVE_URL_LOADER_UTILS_H_

#include <stdint.h>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/keep_alive_request_tracker.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

class BrowserContext;
class KeepAliveURLLoadersTestObserverImpl;

// Observes behaviors of all `KeepAliveURLLoader` instances in synchronous way.
//
// KeepAliveURLLoader itself is running in browser UI thread, but there can be
// multiple instances created and triggered by different renderers.
// For example:
//   - Renderer A triggers `KeepAliveURLLoader::OnReceiveRedirectForwarded()`
//     and then `KeepAliveURLLoader::OnReceiveResponseProcessed()
//   - Renderer B triggers `KeepAliveURLLoader::OnReceiveRedirectForwarded()`
//     twice.
//   - Users can call `WaitForTotalOnReceiveRedirectForwarded()` to wait until
//     all 3 triggerings of `KeepAliveURLLoader::OnReceiveRedirectForwarded()`
//     completed.
//
// The methods provided by this class can also be used to assert that
// KeepAliveURLLoader has entered certain state, e.g.
// `WaitForTotalOnReceiveRedirectProcessed()` indicates the loader handles the
// redirect itself rather than handing over to renderer as renderer is gone.
class KeepAliveURLLoadersTestObserver {
 public:
  // Begins observing the internal states of all instances of KeepAliveURLLoader
  // created under the given `browser_context` immediately.
  explicit KeepAliveURLLoadersTestObserver(BrowserContext* browser_context);
  ~KeepAliveURLLoadersTestObserver();

  // Not Copyable.
  KeepAliveURLLoadersTestObserver(const KeepAliveURLLoadersTestObserver&) =
      delete;
  KeepAliveURLLoadersTestObserver& operator=(
      const KeepAliveURLLoadersTestObserver&) = delete;

  // Waits for `KeepAliveURLLoader::OnReceiveRedirectForwarded()` to be called
  // `total` times.
  void WaitForTotalOnReceiveRedirectForwarded(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveRedirectProcessed()` to be called
  // `total` times.
  void WaitForTotalOnReceiveRedirectProcessed(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveResponse()` to be called `total`
  // times.
  void WaitForTotalOnReceiveResponse(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveResponseForwarded()` to be called
  // `total` times.
  void WaitForTotalOnReceiveResponseForwarded(size_t total);
  // Waits for `KeepAliveURLLoader::OnReceiveResponseProcessed()` to be called
  // `total` times.
  void WaitForTotalOnReceiveResponseProcessed(size_t total);
  // Waits for `KeepAliveURLLoader::OnComplete()` to be called
  // `error_codes.size()` times, and the error codes from all previous calls to
  // that method should match `error_codes`.
  void WaitForTotalOnComplete(const std::vector<int>& error_codes);
  // Waits for `KeepAliveURLLoader::OnCompleteForwarded()` to be called
  // `error_codes.size()` times, and the error codes from all previous calls to
  // that method should match `error_codes`.
  void WaitForTotalOnCompleteForwarded(const std::vector<int>& error_codes);
  // Waits for `KeepAliveURLLoader::OnCompleteProcessed()` to be called
  // `error_codes.size()` times, and the error codes from all previous calls to
  // that method should match `error_codes`.
  void WaitForTotalOnCompleteProcessed(const std::vector<int>& error_codes);

 private:
  std::unique_ptr<KeepAliveURLLoadersTestObserverImpl> impl_;
};

// `KeepAliveRequestUkmMatcher` provides common matchers and expectations to
// help make test assertions on fetch keepalive request-related UKM metrics.
class KeepAliveRequestUkmMatcher {
 protected:
  using UkmEvent = ukm::builders::FetchKeepAliveRequest_WithCategory;
  static constexpr std::string_view kUkmName =
      "FetchKeepAliveRequest.WithCategory";
  virtual ukm::TestAutoSetUkmRecorder& ukm_recorder() = 0;

  // Returns the last recorded UKM event.
  // This assumes that only a single UKM event is recorded.
  const ukm::mojom::UkmEntry* GetUkmEntry();

  // Verifies no UKM event logged.
  void ExpectNoUkm();

  struct CommonUkm {
    KeepAliveRequestTracker::RequestType request_type;
    size_t category_id;
    size_t num_redirects;
    size_t num_retries;
    bool is_context_detached;
    KeepAliveRequestTracker::RequestStageType end_stage;
    std::optional<KeepAliveRequestTracker::RequestStageType> previous_stage =
        std::nullopt;
    std::optional<base::UnguessableToken> keepalive_token = std::nullopt;
    std::optional<int64_t> failed_error_code = std::nullopt;
    std::optional<int64_t> failed_extended_error_code = std::nullopt;
    std::optional<int64_t> completed_error_code = std::nullopt;
    std::optional<int64_t> completed_extended_error_code = std::nullopt;
    std::optional<int64_t> retried_error_code = std::nullopt;
    std::optional<int64_t> retried_extended_error_code = std::nullopt;

    CommonUkm(
        KeepAliveRequestTracker::RequestType request_type,
        size_t category_id,
        size_t num_redirects,
        size_t num_retries,
        bool is_context_detached,
        KeepAliveRequestTracker::RequestStageType end_stage,
        std::optional<KeepAliveRequestTracker::RequestStageType>
            previous_stage = std::nullopt,
        const std::optional<base::UnguessableToken>& keepalive_token =
            std::nullopt,
        std::optional<int64_t> failed_error_code = std::nullopt,
        std::optional<int64_t> failed_extended_error_code = std::nullopt,
        std::optional<int64_t> completed_error_code = std::nullopt,
        std::optional<int64_t> completed_extended_error_code = std::nullopt,
        std::optional<int64_t> retried_error_code = std::nullopt,
        std::optional<int64_t> retried_extended_error_code = std::nullopt);
    CommonUkm(const CommonUkm& other);
  };

  // Verifies all the common UKM metrics.
  //
  // `keepalive_token` is optional. If not provided, this method will just
  // assert the existence of Id.Low and Id.High metrics.
  void ExpectCommonUkm(
      KeepAliveRequestTracker::RequestType request_type,
      size_t category_id,
      size_t num_redirects,
      size_t num_retries,
      bool is_context_detached,
      KeepAliveRequestTracker::RequestStageType end_stage,
      std::optional<KeepAliveRequestTracker::RequestStageType> previous_stage =
          std::nullopt,
      const std::optional<base::UnguessableToken>& keepalive_token =
          std::nullopt,
      std::optional<int64_t> failed_error_code = std::nullopt,
      std::optional<int64_t> failed_extended_error_code = std::nullopt,
      std::optional<int64_t> completed_error_code = std::nullopt,
      std::optional<int64_t> completed_extended_error_code = std::nullopt,
      std::optional<int64_t> retried_error_code = std::nullopt,
      std::optional<int64_t> retried_extended_error_code = std::nullopt);
  void ExpectCommonUkms(const std::vector<CommonUkm>& ukms);

  // Verifies that UKM TimeDelta.* listed in `time_sorted_metric_names` are all
  // non-null and have their values in the given order. All the other
  // TimeDelta.* metrics must be null.
  //
  // For example, if `time_sorted_metric_names` is
  // {"TimeDelta.RequestStarted", "TimeDelta.RequestFailed"}, this method will
  // check that the logged value for "TimeDelta.RequestStarted" <=
  // "TimeDelta.RequestFailed".
  void ExpectTimeSortedTimeDeltaUkm(
      const std::vector<std::string>& time_sorted_metric_names);

 private:
  void ExpectCommonUkm(
      const ukm::mojom::UkmEntry* entry,
      KeepAliveRequestTracker::RequestType request_type,
      size_t category_id,
      size_t num_redirects,
      size_t num_retries,
      bool is_context_detached,
      KeepAliveRequestTracker::RequestStageType end_stage,
      std::optional<KeepAliveRequestTracker::RequestStageType> previous_stage =
          std::nullopt,
      const std::optional<base::UnguessableToken>& keepalive_token =
          std::nullopt,
      std::optional<int64_t> failed_error_code = std::nullopt,
      std::optional<int64_t> failed_extended_error_code = std::nullopt,
      std::optional<int64_t> completed_error_code = std::nullopt,
      std::optional<int64_t> completed_extended_error_code = std::nullopt,
      std::optional<int64_t> retried_error_code = std::nullopt,
      std::optional<int64_t> retried_extended_error_code = std::nullopt);
};

// `NavigationKeepAliveRequestUkmMatcher` provides common matchers and
// expectations to help make test assertions on navigation-fetch keepalive
// request-related UKM metrics.
class NavigationKeepAliveRequestUkmMatcher {
 protected:
  using UkmEvent = ukm::builders::FetchKeepAliveRequest_WithCategory_Navigation;
  static constexpr std::string_view kUkmName =
      "FetchKeepAliveRequest.WithCategory.Navigation";
  virtual ukm::TestAutoSetUkmRecorder& ukm_recorder() = 0;

  // Returns the last recorded UKM event.
  // This assumes that only a single UKM event is recorded.
  const ukm::mojom::UkmEntry* GetUkmEntry();

  // Verifies no UKM event logged.
  void ExpectNoUkm();

  struct NavigationUkm {
    size_t category_id;
    std::optional<int64_t> navigation_id = std::nullopt;
    std::optional<base::UnguessableToken> keepalive_token = std::nullopt;
  };

  // Verifies all the navigation-related UKM metrics.
  //
  // `navigation_id` is optional. If not provided, this method will just
  // assert the existence of it.
  // `keepalive_token` is optional. If not provided, this method will just
  // assert the existence of Id.Low and Id.High metrics.
  void ExpectNavigationUkm(size_t category_id,
                           std::optional<int64_t> navigation_id,
                           const std::optional<base::UnguessableToken>&
                               keepalive_token = std::nullopt);
  void ExpectNavigationUkms(const std::vector<NavigationUkm>& ukms);

 private:
  void ExpectNavigationUkm(const ukm::mojom::UkmEntry* entry,
                           size_t category_id,
                           std::optional<int64_t> navigation_id,
                           const std::optional<base::UnguessableToken>&
                               keepalive_token = std::nullopt);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_KEEP_ALIVE_URL_LOADER_UTILS_H_
