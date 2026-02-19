// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAUTHN_IOS_PASSKEY_JAVA_SCRIPT_FEATURE_H_
#define COMPONENTS_WEBAUTHN_IOS_PASSKEY_JAVA_SCRIPT_FEATURE_H_

#import "base/no_destructor.h"
#import "base/values.h"
#import "components/webauthn/core/browser/passkey_model_utils.h"
#import "components/webauthn/ios/passkey_request_params.h"
#import "ios/web/public/js_messaging/java_script_feature.h"

namespace webauthn {

// Communicates with the JavaScript file passkey_controller.ts, which contains
// a shim of the navigator.credentials API.
//
// The main intent of the shim is to facilitate certain passkey functionality
// (e.g. assertion / creation) in Chromium, hence the name of this class. It is
// worth noting though, that the navigator.credentials API might be used for any
// type of credential. Requests for non-passkey credentials are not handled by
// this feature (with a possible exception of logging metrics).
class PasskeyJavaScriptFeature : public web::JavaScriptFeature {
 public:
  // Provides parameters for passkey attestation, as specified by the webauthn
  // spec:
  // https://www.w3.org/TR/webauthn-2/#iface-authenticatorattestationresponse
  struct AttestationData {
    AttestationData(
        std::vector<uint8_t> attestation_object,
        std::vector<uint8_t> authenticator_data,
        std::vector<uint8_t> public_key_spki_der,
        std::string client_data_json,
        passkey_model_utils::ExtensionOutputData extension_output_data);
    AttestationData(AttestationData&& other);
    ~AttestationData();

    std::vector<uint8_t> attestation_object;
    std::vector<uint8_t> authenticator_data;
    std::vector<uint8_t> public_key_spki_der;
    std::string client_data_json;
    passkey_model_utils::ExtensionOutputData extension_output_data;
  };

  // Provides parameters for passkey assertion, as specified by the webauthn
  // spec:
  // https://www.w3.org/TR/webauthn-2/#iface-authenticatorassertionresponse
  struct AssertionData {
    AssertionData(
        std::vector<uint8_t> signature,
        std::vector<uint8_t> authenticator_data,
        std::vector<uint8_t> user_handle,
        std::string client_data_json,
        passkey_model_utils::ExtensionOutputData extension_output_data);
    AssertionData(AssertionData&& other);
    ~AssertionData();

    std::vector<uint8_t> signature;
    std::vector<uint8_t> authenticator_data;
    std::vector<uint8_t> user_handle;
    std::string client_data_json;
    passkey_model_utils::ExtensionOutputData extension_output_data;
  };

  // This feature holds no state, so only a single static instance is ever
  // needed.
  static PasskeyJavaScriptFeature* GetInstance();

  // Rejects the current attestation or registration request.
  void RejectPasskeyRequest(web::WebFrame* web_frame,
                            std::string_view request_id);

  // Yields the current attestation or registration request back to the OS.
  void DeferToRenderer(web::WebFrame* web_frame,
                       std::string_view request_id,
                       PasskeyRequestParams::RequestType request_type);

  // Resolves the attestation request with a valid passkey.
  void ResolveAttestationRequest(web::WebFrame* web_frame,
                                 std::string_view request_id,
                                 std::string_view credential_id,
                                 AttestationData attestation_data);

  // Resolves the assertion request with a valid passkey.
  void ResolveAssertionRequest(web::WebFrame* web_frame,
                               std::string_view request_id,
                               std::string_view credential_id,
                               AssertionData assertion_data);

 private:
  friend class base::NoDestructor<PasskeyJavaScriptFeature>;

  PasskeyJavaScriptFeature();
  PasskeyJavaScriptFeature(const PasskeyJavaScriptFeature&) = delete;
  PasskeyJavaScriptFeature& operator=(const PasskeyJavaScriptFeature&) = delete;
  ~PasskeyJavaScriptFeature() override;

  // web::JavaScriptFeature:
  std::optional<std::string> GetScriptMessageHandlerName() const override;
  void ScriptMessageReceived(web::WebState* web_state,
                             const web::ScriptMessage& message) override;

  // Continues the creation request flow after the Incognito check has passed.
  void ProcessCreateRequest(web::WebState* web_state,
                            IOSPasskeyClient::RequestInfo request_info,
                            base::DictValue dict);

  // Callback handling the user's decision from the interstitial.
  void OnInterstitialDecision(base::WeakPtr<web::WebState> web_state,
                              IOSPasskeyClient::RequestInfo request_info,
                              base::DictValue dict,
                              bool proceed);
};

}  // namespace webauthn

#endif  // COMPONENTS_WEBAUTHN_IOS_PASSKEY_JAVA_SCRIPT_FEATURE_H_
