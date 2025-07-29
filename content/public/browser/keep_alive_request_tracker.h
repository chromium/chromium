// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_KEEP_ALIVE_REQUEST_TRACKER_H_
#define CONTENT_PUBLIC_BROWSER_KEEP_ALIVE_REQUEST_TRACKER_H_

#include <ostream>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "content/common/content_export.h"
#include "services/network/public/cpp/url_loader_completion_status.h"

namespace content {

// Allows embedders to control logging of fetch keepalive requests.
class CONTENT_EXPORT KeepAliveRequestTracker {
 public:
  // A callback type to tell if the context of the request is detached.
  using IsContextDetachedCallback = base::RepeatingCallback<bool(void)>;

  // The type of fetch keepalive request to track for in this tracker.
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(RequestType)
  enum class RequestType {
    kFetch = 0,
    kAttribution = 1,
    kFetchLater = 2,

    kMaxValue = kFetchLater,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:FetchKeepAliveRequestType)

  explicit KeepAliveRequestTracker(RequestType request_type);
  virtual ~KeepAliveRequestTracker();
  // Not movable.
  KeepAliveRequestTracker(const KeepAliveRequestTracker&) = delete;
  KeepAliveRequestTracker& operator=(const KeepAliveRequestTracker&) = delete;

  // Returns the type of fetch keepalive request tracked by this.
  RequestType GetRequestType() const;

  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused.
  // LINT.IfChange(RequestStageType)
  enum class RequestStageType {
    // The browser-side loader for the fetch keepalive request is created.
    kLoaderCreated = 0,

    // The browser-side loader starts to load the fetch keepalive request.
    kRequestStarted = 1,

    // The browser-side loader receives the 1st redirect.
    kFirstRedirectReceived = 2,

    // The browser-side loader receives the 2nd redirect.
    kSecondRedirectReceived = 3,

    // The browser-side loader receives the 3rd+ redirect.
    kThirdOrLaterRedirectReceived = 4,

    // The browser-side loader receives the response.
    kResponseReceived = 5,

    // The browser-side loader fails to load the fetch keepalive request.
    kRequestFailed = 6,

    // The browser-side loader gets disconnected from the renderer.
    kLoaderDisconnectedFromRenderer = 7,

    // The browser-side loader receives cancel request from renderer.
    // This only happens when `GetRequestType()` is kFetchLater.
    kRequestCancelledByRenderer = 8,

    // The browser-side loader cancels all its operation after disconnecting
    // from renderer for a certain time limit, i.e.
    // `kDefaultDisconnectedKeepAliveURLLoaderTimeout`.
    kRequestCancelledAfterTimeLimit = 9,

    // The browser-side loader gets notified when browser is shutting down.
    kBrowserShutdown = 10,

    // The browser-side loader completes loading the request.
    kLoaderCompleted = 11,

    // The browser-side loader has retried the request from the beginning.
    kRequestRetried = 12,

    kMaxValue = kRequestRetried,
  };
  // LINT.ThenChange(//tools/metrics/histograms/enums.xml:FetchKeepAliveRequestStage)
  friend std::ostream& operator<<(std::ostream&, const RequestStageType&);

  // RequestStage stores the stage-related info.
  struct CONTENT_EXPORT RequestStage {
    RequestStage(const RequestStageType&,
                 std::optional<network::URLLoaderCompletionStatus>);
    explicit RequestStage(const RequestStageType&);
    ~RequestStage();

    // Copyable.
    RequestStage(const RequestStage&);
    RequestStage& operator=(const RequestStage&);

    RequestStageType type;
    std::optional<network::URLLoaderCompletionStatus> status = std::nullopt;
  };

  // Advances the request stage to a stage with `next_stage_type`.
  void AdvanceToNextStage(const RequestStageType& next_stage_type,
                          std::optional<network::URLLoaderCompletionStatus>
                              next_stage_status = std::nullopt);

  // Returns the next possible RequestStageType for a redirect based on the
  // current `num_redirects_`.
  RequestStageType GetNextRedirectStageType() const;

 protected:
  // Adds UKM metrics for the given `stage`.
  virtual void AddStageMetrics(const RequestStage& stage) = 0;

  // Returns the current request stage.
  const RequestStage& GetCurrentStage() const;
  // Returns the previous request stage;
  const std::optional<RequestStage>& GetPreviousStage() const;

  uint32_t GetNumRedirects() const;
  void IncreaseNumRedirects();

  uint32_t GetNumRetries() const;
  void IncreaseNumRetries();

 private:
  RequestType request_type_;

  // Stores the last request stage;
  RequestStage current_stage_;
  // Stores the previous request stage;
  std::optional<RequestStage> previous_stage_ = std::nullopt;

  // Records the number of redrects the tracked fetch keepalive request has
  // experienced so far.
  uint32_t num_redirects_ = 0;

  // Records the number of retries the tracked fetch keepalive request has
  // experienced so far.
  uint32_t num_retries_ = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_KEEP_ALIVE_REQUEST_TRACKER_H_
