// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_

#import <optional>
#import <variant>

#import "base/memory/weak_ptr.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/core/browser/remote_validation.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/passkey_request_params.h"
#import "components/webauthn/ios/passkey_types.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

namespace web {
class WebFrame;
}  // namespace web

@protocol IOSPasskeyClientCommands;

namespace webauthn {

// Handles script messages received from PasskeyJavaScriptFeature related to
// interactions with WebAuthn credentials and for now logs appropriate metrics.
class PasskeyTabHelper : public web::WebStateObserver,
                         public web::WebStateUserData<PasskeyTabHelper>,
                         public web::WebFramesManager::Observer {
 public:
  // These values are logged to UMA. Entries should not be renumbered and
  // numeric values should never be reused.
  //
  // LINT.IfChange
  enum class WebAuthenticationIOSContentAreaEvent {
    kGetRequested,
    kCreateRequested,
    kGetResolvedGpm,
    kGetResolvedNonGpm,
    kCreateResolvedGpm,
    kCreateResolvedNonGpm,
    kIncognitoInterstitialShown,
    kMaxValue = kIncognitoInterstitialShown,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

  PasskeyTabHelper(const PasskeyTabHelper&) = delete;
  PasskeyTabHelper& operator=(const PasskeyTabHelper&) = delete;

  ~PasskeyTabHelper() override;

  // Logs metric indicating that an event of the given type occurred.
  void LogEvent(WebAuthenticationIOSContentAreaEvent event_type);

  // Handles passkey assertion requests. Yields if the request ID is missing.
  void HandleGetRequestedEvent(AssertionRequestParams params);

  // Handles passkey registration requests. Yields if the request ID is missing.
  void HandleCreateRequestedEvent(RegistrationRequestParams params);

  // Returns whether the tab helper's passkey model contains a passkey matching
  // the provided rp id and credential id.
  bool HasCredential(const std::string& rp_id,
                     const std::string& credential_id) const;

  // Requests a passkey to be created given the provided request ID. Fetches the
  // shared keys list and calls the CompletePasskeyCreation callback.
  // TODO(crbug.com/460485333): Test passkey creation flow.
  void StartPasskeyCreation(std::string request_id);

  // Requests that the passkey matching the provided credential ID be used for
  // passkey assertion given the provided request ID. Fetches the shared keys
  // list and calls the CompletePasskeyAssertion callback.
  // TODO(crbug.com/460485333): Test passkey assertion flow.
  void StartPasskeyAssertion(std::string request_id, std::string credential_id);

  // Utility function to defer the passkey request back to the renderer.
  void DeferToRenderer(IOSPasskeyClient::RequestInfo request_info,
                       PasskeyRequestParams::RequestType request_type) const;

  // Utility function to reject a pending passkey.
  void RejectPendingRequest(const std::string& request_id);

  // Utility function to defer a pending passkey request back to the renderer.
  void DeferPendingRequestToRenderer(const std::string& request_id);

  // Returns the username associated with the current request ID or an empty
  // string if the request is not found. Note that only registration requests
  // have a username.
  std::string UsernameForRequest(const std::string& request_id);

  // Sets the passkey command handler.
  void SetIOSPasskeyClientCommandsHandler(id<IOSPasskeyClientCommands> handler);

  // Returns whether user verification should be performed for `request_id`.
  // It returns std::nullopt if the request is unknown.
  std::optional<bool> ShouldPerformUserVerification(
      const std::string& request_id,
      bool is_biometric_authentication_enabled) const;

  // Returns whether there is a pending remote validation for testing.
  bool HasPendingValidationForTesting() const;

  // Returns whether the interstitial is necessary for the current state.
  bool ShowCreationInterstitialIfNecessary(
      base::OnceCallback<void(bool)> callback);

 private:
  friend class web::WebStateUserData<PasskeyTabHelper>;
  friend class PasskeyTabHelperTest;

  // Pending requests keyed by frame ID when a WebFrame isn't yet available.
  using PendingRequest =
      std::variant<AssertionRequestParams, RegistrationRequestParams>;

  explicit PasskeyTabHelper(web::WebState* web_state,
                            PasskeyModel* passkey_model,
                            std::unique_ptr<IOSPasskeyClient> client);

  // Handles passkey assertion requests. Defers if the rp ID is invalid.
  void HandleGetRequestedEvent(web::WebFrame* web_frame,
                               AssertionRequestParams params);

  // Handles passkey registration requests. Defers if the rp ID is invalid.
  void HandleCreateRequestedEvent(web::WebFrame* web_frame,
                                  RegistrationRequestParams params);

  // Returns whether the passkey model contains a passkey from the
  // exclude credentials list from the provided parameters.
  bool HasExcludedPasskey(const RegistrationRequestParams& params) const;

  // Returns the list of passkeys filtered by the allowed credentials list.
  std::vector<sync_pb::WebauthnCredentialSpecifics> GetFilteredPasskeys(
      const AssertionRequestParams& params) const;

  // Callback which creates a passkey given the provided shared keys list and
  // params. The newly created passkey is added to the passkey model and the
  // parameters required to resolve the PublicKeyCredential request are sent to
  // PasskeyJavaScriptFeature.
  void CompletePasskeyCreation(RegistrationRequestParams params,
                               std::string client_data_json,
                               SharedKeyList shared_key_list,
                               NSError* error);

  // Callback which uses the provided passkey for assertion given the provided
  // shared keys list and params. The parameters required to resolve the
  // PublicKeyCredential request are sent to PasskeyJavaScriptFeature.
  void CompletePasskeyAssertion(AssertionRequestParams params,
                                sync_pb::WebauthnCredentialSpecifics passkey,
                                std::string client_data_json,
                                SharedKeyList shared_key_list,
                                NSError* error);

  // Starts remote validation for the given origin and RP ID. If validation
  // starts successfully, the loader is stored in `loaders_` with
  // `passkey_request_id` as the key. Returns true if validation started, false
  // otherwise.
  bool PerformRemoteRpIdValidation(
      const url::Origin& origin,
      const std::string& rp_id,
      const std::string& passkey_request_id,
      base::OnceCallback<void(ValidationStatus)> callback);

  // Callback for processing remote validation result for a pending request.
  void OnRemoteRpIdValidationCompleted(PendingRequest request,
                                       ValidationStatus status);

  // Handles passkey assertion request after it passes validation.
  void HandleAssertion(AssertionRequestParams params);

  // Whether automatic passkey upgrade is allowed.
  bool CanPerformAutomaticPasskeyUpgrade(
      const RegistrationRequestParams& params) const;

  // Handles passkey registration requests after it passes validation.
  void HandleRegistration(RegistrationRequestParams params);

  // Adds a passkey to the passkey model while enabling the passkey creation
  // infobar to be displayed if possible.
  void AddNewPasskey(sync_pb::WebauthnCredentialSpecifics& passkey);

  // Returns information (Frame ID and Request Type) for a request identified by
  // `request_id`. Returns std::nullopt if the request is not found.
  std::optional<std::pair<std::string, PasskeyRequestParams::RequestType>>
  ExtractRequestInfo(const std::string& request_id);

  // Utility function to reject a passkey request.
  void RejectPasskeyRequest(web::WebFrame* web_frame,
                            const std::string& request_id);

  // Utility function to defer the passkey request back to the renderer.
  void DeferToRenderer(web::WebFrame* web_frame,
                       const std::string& request_id,
                       PasskeyRequestParams::RequestType request_type) const;

  // If `request_id` exists in the `assertion_requests_` map, this function will
  // remove the parameters from the `assertion_requests_` map and return them.
  // Returns std::nullopt otherwise.
  std::optional<AssertionRequestParams> ExtractParamsFromAssertionRequestsMap(
      std::string request_id);

  // If `request_id` exists in the `registration_requests_` map, this function
  // will remove the parameters from the `registration_requests_` map and return
  // them. Returns std::nullopt otherwise.
  std::optional<RegistrationRequestParams>
  ExtractParamsFromRegistrationRequestsMap(std::string request_id);

  // Returns a web frame from a web frame id. May return null.
  web::WebFrame* GetWebFrame(const std::string& frame_id) const;

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

  // WebFramesManager::Observer:
  void WebFrameBecameAvailable(web::WebFramesManager* web_frames_manager,
                               web::WebFrame* web_frame) override;

  // Gets a weak pointer to this object.
  base::WeakPtr<PasskeyTabHelper> AsWeakPtr();

  // Provides access to stored WebAuthn credentials.
  const raw_ref<PasskeyModel> passkey_model_;

  // The WebState with which this object is associated.
  base::WeakPtr<web::WebState> web_state_;

  // The client used to perform user facing tasks for the PasskeyTabHelper.
  std::unique_ptr<IOSPasskeyClient> client_;

  // A map of request IDs (as std::string) to assertion request parameters.
  absl::flat_hash_map<std::string, AssertionRequestParams> assertion_requests_;

  // A map of request IDs (as std::string) to registration request parameters.
  absl::flat_hash_map<std::string, RegistrationRequestParams>
      registration_requests_;

  absl::flat_hash_map<std::string, std::vector<PendingRequest>>
      pending_requests_by_frame_;

  // Map of request IDs to their ongoing remote validation loaders.
  absl::flat_hash_map<std::string, std::unique_ptr<RemoteValidation>> loaders_;

  // This is necessary because this object could be deleted during any callback,
  // and we don't want to risk a UAF if that happens.
  base::WeakPtrFactory<PasskeyTabHelper> weak_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
