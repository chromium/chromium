// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CHALLENGE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CHALLENGE_TAB_HELPER_H_

#import <cstddef>
#import <cstdint>
#import <memory>
#import <optional>
#import <string>

#import "base/containers/flat_map.h"
#import "base/functional/callback.h"
#import "base/memory/raw_ref.h"
#import "base/memory/weak_ptr.h"
#import "base/scoped_observation.h"
#import "base/sequence_checker.h"
#import "base/timer/timer.h"
#import "base/types/strong_alias.h"
#import "components/enterprise/device_trust/core/common_types.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "url/origin.h"

class GURL;

namespace web {
class WebFrame;
}  // namespace web

// DeviceTrustChallengeTabHelper is the orchestrator for the iOS Device Trust
// challenge-response flow. It is attached to a WebState and receives challenge
// requests forwarded from DeviceTrustJavaScriptFeature, manages browser-side
// pending requests and timeouts, and routes the challenge response back via
// DeviceTrustResponse.
class DeviceTrustChallengeTabHelper
    : public web::WebFramesManager::Observer,
      public web::WebStateObserver,
      public web::WebStateUserData<DeviceTrustChallengeTabHelper> {
 public:
  // Maximum number of pending attestation requests per WebState.
  static constexpr size_t kMaxPendingRequests = 3;

  using AttestationCallback = base::OnceCallback<void(
      const enterprise_connectors::DeviceTrustResponse&)>;

  ~DeviceTrustChallengeTabHelper() override;

  DeviceTrustChallengeTabHelper(const DeviceTrustChallengeTabHelper&) = delete;
  DeviceTrustChallengeTabHelper& operator=(
      const DeviceTrustChallengeTabHelper&) = delete;

  // Initiates the asynchronous device attestation signing process for
  // `challenge`.
  //
  // `security_origin` is the security origin of the calling frame.
  // `request_url` is the optional full URL of the frame. When present, valid,
  // and same-origin with `security_origin`, `request_url` is used for policy
  // allowlist matching to support path-scoped patterns (e.g.
  // https://example.com/login). Otherwise, `security_origin.GetURL()` is used.
  //
  // Resolves via `callback` with a DeviceTrustResponse containing the signed
  // payload or an error.
  void BuildChallengeResponse(const url::Origin& security_origin,
                              const std::optional<GURL>& request_url,
                              const std::string& challenge,
                              AttestationCallback callback);

  // web::WebFramesManager::Observer:
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

 private:
  friend class web::WebStateUserData<DeviceTrustChallengeTabHelper>;
  explicit DeviceTrustChallengeTabHelper(web::WebState* web_state);

  // State for a request awaiting a reply: `callback` answers the JavaScript
  // promise, while `timer` answers with a timeout error if the service is
  // slower than the deadline. The first to fire removes the entry so the page
  // is answered exactly once; the timer does not cancel the service call.
  struct PendingRequest {
    explicit PendingRequest(AttestationCallback callback);
    ~PendingRequest();

    AttestationCallback callback;
    base::OneShotTimer timer;
  };

  using RequestId = base::StrongAlias<class RequestTag, uint64_t>;

  // Installs the Device Trust API in an eligible main frame.
  void MaybeSetupDeviceTrustAPI(web::WebFrame* web_frame);

  // Takes and removes the pending request with `request_id`, or returns
  // nullptr if it does not exist.
  std::unique_ptr<PendingRequest> TakePendingRequest(RequestId request_id);

  // Callback invoked when the device trust service finishes building the
  // challenge response.
  void OnChallengeResponseReady(
      RequestId request_id,
      const enterprise_connectors::DeviceTrustResponse& response);

  // Callback invoked when an attestation request times out.
  void OnAttestationTimeout(RequestId request_id);

  // Posts an error response to `callback` asynchronously on the current
  // sequence.
  void PostError(AttestationCallback callback,
                 enterprise_connectors::DeviceTrustError error);

  // Helper invoked by `PostError` to run `callback` with `error`.
  void RunPostedError(AttestationCallback callback,
                      enterprise_connectors::DeviceTrustError error);

  SEQUENCE_CHECKER(sequence_checker_);
  const raw_ref<web::WebState> web_state_;
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
  base::ScopedObservation<web::WebFramesManager,
                          web::WebFramesManager::Observer>
      web_frames_manager_observation_{this};

  uint64_t next_request_id_ = 1;
  base::flat_map<RequestId, std::unique_ptr<PendingRequest>> pending_requests_;
  base::WeakPtrFactory<DeviceTrustChallengeTabHelper> weak_factory_{this};
};

#endif  // IOS_CHROME_BROWSER_ENTERPRISE_CONNECTORS_DEVICE_TRUST_MODEL_DEVICE_TRUST_CHALLENGE_TAB_HELPER_H_
