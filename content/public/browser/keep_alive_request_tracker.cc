// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/keep_alive_request_tracker.h"

#include "base/no_destructor.h"

namespace content {

KeepAliveRequestTracker::KeepAliveRequestTracker(RequestType request_type)
    : request_type_(request_type),
      current_stage_({RequestStageType::kLoaderCreated, std::nullopt}) {}
KeepAliveRequestTracker::~KeepAliveRequestTracker() = default;

KeepAliveRequestTracker::RequestStage::RequestStage(
    const RequestStageType& other_type,
    std::optional<network::URLLoaderCompletionStatus> other_status)
    : type(other_type), status(other_status) {}
KeepAliveRequestTracker::RequestStage::RequestStage(
    const RequestStageType& other_type)
    : RequestStage(other_type, std::nullopt) {}
KeepAliveRequestTracker::RequestStage::~RequestStage() = default;

KeepAliveRequestTracker::RequestStage::RequestStage(const RequestStage& other) =
    default;
KeepAliveRequestTracker::RequestStage&
KeepAliveRequestTracker::RequestStage::operator=(const RequestStage&) = default;

std::ostream& operator<<(
    std::ostream& os,
    const KeepAliveRequestTracker::RequestStageType& stage_type) {
  switch (stage_type) {
    case KeepAliveRequestTracker::RequestStageType::kLoaderCreated:
      os << "Loader Created";
      break;
    case KeepAliveRequestTracker::RequestStageType::kRequestStarted:
      os << "Request Started";
      break;
    case KeepAliveRequestTracker::RequestStageType::kFirstRedirectReceived:
      os << "First Redirect Received";
      break;
    case KeepAliveRequestTracker::RequestStageType::kSecondRedirectReceived:
      os << "Second Redirect Received";
      break;
    case KeepAliveRequestTracker::RequestStageType::
        kThirdOrLaterRedirectReceived:
      os << "Third or Later Redirect Received";
      break;
    case KeepAliveRequestTracker::RequestStageType::kResponseReceived:
      os << "Response Received";
      break;
    case KeepAliveRequestTracker::RequestStageType::kRequestFailed:
      os << "Request Failed";
      break;
    case KeepAliveRequestTracker::RequestStageType::
        kLoaderDisconnectedFromRenderer:
      os << "Loader Disconnected from Renderer";
      break;
    case KeepAliveRequestTracker::RequestStageType::kRequestCancelledByRenderer:
      os << "Request Cancelled by Renderer";
      break;
    case KeepAliveRequestTracker::RequestStageType::
        kRequestCancelledAfterTimeLimit:
      os << "Request Cancelled after Time Limit";
      break;
    case KeepAliveRequestTracker::RequestStageType::kBrowserShutdown:
      os << "Browser Shutdown";
      break;
    case KeepAliveRequestTracker::RequestStageType::kLoaderCompleted:
      os << "Loader Completed";
      break;
    default:
      os << "<invalid value: " << static_cast<int>(stage_type) << ">";
  }
  return os;
}

void KeepAliveRequestTracker::AdvanceToNextStage(
    const RequestStageType& next_stage_type,
    std::optional<network::URLLoaderCompletionStatus> next_stage_status) {
  RequestStage next_stage(next_stage_type, next_stage_status);
#if DCHECK_IS_ON()
  static const base::NoDestructor<base::StateTransitions<RequestStageType>>
      transitions(base::StateTransitions<RequestStageType>({
          {RequestStageType::kLoaderCreated,
           {
               RequestStageType::kRequestStarted,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kBrowserShutdown,
           }},
          {RequestStageType::kRequestStarted,
           {
               RequestStageType::kFirstRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kFirstRedirectReceived,
           {
               RequestStageType::kSecondRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kSecondRedirectReceived,
           {
               RequestStageType::kThirdOrLaterRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kThirdOrLaterRedirectReceived,
           {
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kResponseReceived,
           {
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kRequestFailed,
           {
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kLoaderDisconnectedFromRenderer,
           {
               RequestStageType::kRequestStarted,
               RequestStageType::kFirstRedirectReceived,
               RequestStageType::kSecondRedirectReceived,
               RequestStageType::kThirdOrLaterRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kRequestCancelledByRenderer,
           {
               RequestStageType::kRequestStarted,
               RequestStageType::kFirstRedirectReceived,
               RequestStageType::kSecondRedirectReceived,
               RequestStageType::kThirdOrLaterRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kRequestCancelledAfterTimeLimit,
           {
               RequestStageType::kRequestStarted,
               RequestStageType::kFirstRedirectReceived,
               RequestStageType::kSecondRedirectReceived,
               RequestStageType::kThirdOrLaterRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kBrowserShutdown,
               RequestStageType::kLoaderCompleted,
           }},
          {RequestStageType::kBrowserShutdown,
           {
               RequestStageType::kRequestStarted,
               RequestStageType::kFirstRedirectReceived,
               RequestStageType::kSecondRedirectReceived,
               RequestStageType::kThirdOrLaterRedirectReceived,
               RequestStageType::kResponseReceived,
               RequestStageType::kRequestFailed,
               RequestStageType::kLoaderDisconnectedFromRenderer,
               RequestStageType::kRequestCancelledByRenderer,
               RequestStageType::kRequestCancelledAfterTimeLimit,
               RequestStageType::kLoaderCompleted,
           }},
      }));

  DCHECK_STATE_TRANSITION(transitions, current_stage_.type, next_stage.type);
#endif  // DCHECK_IS_ON()
  // kLoaderCreated is the initial stage set in ctor.
  CHECK_NE(next_stage.type, RequestStageType::kLoaderCreated);

  switch (next_stage.type) {
    case RequestStageType::kLoaderCreated:
      // kLoaderCreated is the initial stage set in ctor.
      NOTREACHED();
    case RequestStageType::kRequestStarted:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kFirstRedirectReceived:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kSecondRedirectReceived:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kThirdOrLaterRedirectReceived:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kResponseReceived:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kRequestFailed:
      CHECK(next_stage.status.has_value());
      break;
    case RequestStageType::kLoaderDisconnectedFromRenderer:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kRequestCancelledByRenderer:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kRequestCancelledAfterTimeLimit:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kBrowserShutdown:
      CHECK(!next_stage.status.has_value());
      break;
    case RequestStageType::kLoaderCompleted:
      CHECK(next_stage.status.has_value());
      break;
  }

  AddStageMetrics(next_stage);

  previous_stage_ = current_stage_;
  current_stage_ = next_stage;
}

const KeepAliveRequestTracker::RequestStage&
KeepAliveRequestTracker::GetCurrentStage() const {
  return current_stage_;
}

const std::optional<KeepAliveRequestTracker::RequestStage>&
KeepAliveRequestTracker::GetPreviousStage() const {
  return previous_stage_;
}

KeepAliveRequestTracker::RequestStageType
KeepAliveRequestTracker::GetNextRedirectStageType() const {
  switch (num_redirects_) {
    case 0:
      return KeepAliveRequestTracker::RequestStageType::kFirstRedirectReceived;
    case 1:
      return KeepAliveRequestTracker::RequestStageType::kSecondRedirectReceived;
    case 2:
      return KeepAliveRequestTracker::RequestStageType::
          kThirdOrLaterRedirectReceived;
    default:
      return KeepAliveRequestTracker::RequestStageType::
          kThirdOrLaterRedirectReceived;
  }
}

uint32_t KeepAliveRequestTracker::GetNumRedirects() const {
  return num_redirects_;
}

void KeepAliveRequestTracker::IncreaseNumRedirects() {
  ++num_redirects_;
}

}  // namespace content
