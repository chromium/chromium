// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_

#import "base/memory/weak_ptr.h"
#import "components/webauthn/core/browser/passkey_model.h"
#import "components/webauthn/ios/ios_passkey_client.h"
#import "components/webauthn/ios/passkey_request_params.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"
#import "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace sync_pb {
class WebauthnCredentialSpecifics;
}  // namespace sync_pb

namespace web {
class WebFrame;
}  // namespace web

namespace webauthn {

// Handles script messages received from PasskeyJavaScriptFeature related to
// interactions with WebAuthn credentials and for now logs appropriate metrics.
class PasskeyTabHelper : public web::WebStateObserver,
                         public web::WebStateUserData<PasskeyTabHelper> {
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
    kMaxValue = kCreateResolvedNonGpm,
  };
  // LINT.ThenChange(//tools/metrics/histograms/metadata/webauthn/enums.xml)

  PasskeyTabHelper(const PasskeyTabHelper&) = delete;
  PasskeyTabHelper& operator=(const PasskeyTabHelper&) = delete;

  ~PasskeyTabHelper() override;

  // Logs metric indicating that an event of the given type occurred.
  void LogEvent(WebAuthenticationIOSContentAreaEvent event_type);

  // Handles passkey assertion requests. Yields if any parameter is missing.
  void HandleGetRequestedEvent(AssertionRequestParams params);

  // Handles passkey registration requests. Yields if any parameter is missing.
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

 private:
  friend class web::WebStateUserData<PasskeyTabHelper>;
  friend class PasskeyTabHelperTest;

  explicit PasskeyTabHelper(web::WebState* web_state,
                            PasskeyModel* passkey_model,
                            std::unique_ptr<IOSPasskeyClient> client);

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
                               const SharedKeyList& shared_key_list);

  // Callback which uses the provided passkey for assertion given the provided
  // shared keys list and params. The parameters required to resolve the
  // PublicKeyCredential request are sent to PasskeyJavaScriptFeature.
  void CompletePasskeyAssertion(AssertionRequestParams params,
                                sync_pb::WebauthnCredentialSpecifics passkey,
                                std::string client_data_json,
                                const SharedKeyList& shared_key_list);

  // Adds a passkey to the passkey model while enabling the passkey creation
  // infobar to be displayed if possible.
  void AddNewPasskey(sync_pb::WebauthnCredentialSpecifics& passkey);

  // Utility function to defer the passkey request back to the renderer.
  void DeferToRenderer(web::WebFrame* web_frame,
                       const PasskeyRequestParams& request_params) const;

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
  web::WebFrame* GetWebFrame(const PasskeyRequestParams& request_params) const;

  // WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;

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

  // This is necessary because this object could be deleted during any callback,
  // and we don't want to risk a UAF if that happens.
  base::WeakPtrFactory<PasskeyTabHelper> weak_factory_{this};
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_TAB_HELPER_H_
