#include "third_party/blink/renderer/modules/credentialmanagement/json.h"

#include "base/numerics/safe_conversions.h"
#include "base/types/expected.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_inputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_inputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_inputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_supplemental_pub_keys_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_creation_options_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_descriptor_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_public_key_credential_user_entity_js_on.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/heap_traits.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

namespace {

std::optional<DOMArrayBuffer*> WebAuthnBase64UrlDecode(const String& in) {
  VectorOf<char> out;
  if (!Base64UnpaddedURLDecode(in, out)) {
    return std::nullopt;
  }
  return DOMArrayBuffer::Create(out.data(), out.size());
}

base::expected<PublicKeyCredentialUserEntity*, const char*>
PublicKeyCredentialUserEntityFromJSON(
    const PublicKeyCredentialUserEntityJSON& json) {
  auto* result = PublicKeyCredentialUserEntity::Create();
  if (auto id = WebAuthnBase64UrlDecode(json.id()); id.has_value()) {
    result->setId(
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(*id));
  } else {
    return base::unexpected("'user.id' contains invalid base64url");
  }
  result->setName(json.name());
  result->setDisplayName(json.displayName());
  return result;
}

base::expected<PublicKeyCredentialDescriptor*, const char*>
PublicKeyCredentialDescriptorFromJSON(
    const PublicKeyCredentialDescriptorJSON& json) {
  auto* result = PublicKeyCredentialDescriptor::Create();
  if (auto id = WebAuthnBase64UrlDecode(json.id()); id.has_value()) {
    result->setId(
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(*id));
  } else {
    return base::unexpected(
        "'excludeCredentials' contains PublicKeyCredentialDescriptorJSON "
        "with invalid base64url data in 'id'");
  }
  result->setType(json.type());
  if (json.hasTransports()) {
    Vector<String> transports;
    for (const String& transport : json.transports()) {
      transports.push_back(transport);
    }
    result->setTransports(std::move(transports));
  }
  return result;
}

std::optional<AuthenticationExtensionsPRFValues*>
AuthenticationExtensionsPRFValuesFromJSON(
    const AuthenticationExtensionsPRFValuesJSON& json) {
  auto* values = AuthenticationExtensionsPRFValues::Create();
  auto first = WebAuthnBase64UrlDecode(json.first());
  if (!first.has_value()) {
    return std::nullopt;
  }
  values->setFirst(
      MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(first.value()));
  if (json.hasSecond()) {
    auto second = WebAuthnBase64UrlDecode(json.second());
    if (!second.has_value()) {
      return std::nullopt;
    }
    values->setSecond(MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
        second.value()));
  }
  return values;
}

base::expected<AuthenticationExtensionsClientInputs*, const char*>
AuthenticationExtensionsClientInputsFromJSON(
    const AuthenticationExtensionsClientInputsJSON& json,
    ExceptionState& exception_state) {
  auto* result = AuthenticationExtensionsClientInputs::Create();
  if (json.hasAppid()) {
    result->setAppid(json.appid());
  }
  if (json.hasAppidExclude()) {
    result->setAppidExclude(json.appidExclude());
  }
  if (json.hasHmacCreateSecret()) {
    result->setHmacCreateSecret(json.hmacCreateSecret());
  }
  if (json.hasUvm()) {
    result->setUvm(json.uvm());
  }
  if (json.hasCredentialProtectionPolicy()) {
    result->setCredentialProtectionPolicy(json.credentialProtectionPolicy());
  }
  if (json.hasEnforceCredentialProtectionPolicy()) {
    result->setEnforceCredentialProtectionPolicy(
        json.enforceCredentialProtectionPolicy());
  }
  if (json.hasMinPinLength()) {
    result->setMinPinLength(json.minPinLength());
  }
  result->setCredProps(json.credProps());
  if (json.hasLargeBlob()) {
    auto* large_blob = AuthenticationExtensionsLargeBlobInputs::Create();
    if (json.largeBlob()->hasSupport()) {
      large_blob->setSupport(json.largeBlob()->support());
    }
    if (json.largeBlob()->hasRead()) {
      large_blob->setRead(json.largeBlob()->read());
    }
    if (json.largeBlob()->hasWrite()) {
      if (auto write = WebAuthnBase64UrlDecode(json.largeBlob()->write());
          write.has_value()) {
        large_blob->setWrite(
            MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
                write.value()));
      } else {
        return base::unexpected(
            "'extensions.largeBlob.write' contains invalid base64url data");
      }
    }
    result->setLargeBlob(large_blob);
  }
  if (json.hasCredBlob()) {
    if (auto cred_blob = WebAuthnBase64UrlDecode(json.credBlob());
        cred_blob.has_value()) {
      result->setCredBlob(
          MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(
              cred_blob.value()));
    } else {
      return base::unexpected(
          "'extensions.credBlob' contains invalid base64url data");
    }
  }
  if (json.getCredBlob()) {
    result->setGetCredBlob(json.getCredBlob());
  }
  if (json.hasPayment()) {
    result->setPayment(json.payment());
  }
  if (json.hasRemoteDesktopClientOverride()) {
    result->setRemoteDesktopClientOverride(json.remoteDesktopClientOverride());
  }
  if (json.hasSupplementalPubKeys()) {
    result->setSupplementalPubKeys(json.supplementalPubKeys());
  }
  if (json.hasPrf()) {
    auto* prf = AuthenticationExtensionsPRFInputs::Create();
    if (json.prf()->hasEval()) {
      std::optional<AuthenticationExtensionsPRFValues*> eval =
          AuthenticationExtensionsPRFValuesFromJSON(*(json.prf()->eval()));
      if (!eval) {
        return base::unexpected(
            "'extensions.prf.eval' contains invalid base64url data");
      }
      prf->setEval(eval.value());
    }
    if (json.prf()->hasEvalByCredential()) {
      VectorOfPairs<String, AuthenticationExtensionsPRFValues> eval;
      for (const auto& [key, json_values] : json.prf()->evalByCredential()) {
        std::optional<AuthenticationExtensionsPRFValues*> values =
            AuthenticationExtensionsPRFValuesFromJSON(*json_values);
        if (!values) {
          return base::unexpected(
              "'extensions.prf.evalByCredential' contains invalid base64url "
              "data");
        }
        eval.emplace_back(key, *values);
      }
      prf->setEvalByCredential(std::move(eval));
    }
    result->setPrf(prf);
  }
  return result;
}

}  // namespace

WTF::String WebAuthnBase64UrlEncode(DOMArrayPiece buffer) {
  // WTF::Base64URLEncode always pads, so we strip trailing '='.
  String encoded =
      WTF::Base64URLEncode(static_cast<const char*>(buffer.Data()),
                           base::checked_cast<wtf_size_t>(buffer.ByteLength()));
  unsigned padding_start = encoded.length();
  for (; padding_start > 0; --padding_start) {
    if (encoded[padding_start - 1] != '=') {
      break;
    }
  }
  encoded.Truncate(padding_start);
  return encoded;
}

AuthenticationExtensionsClientOutputsJSON*
AuthenticationExtensionsClientOutputsToJSON(
    ScriptState* script_state,
    const blink::AuthenticationExtensionsClientOutputs& in) {
  auto* json = AuthenticationExtensionsClientOutputsJSON::Create();
  if (in.hasAppid()) {
    json->setAppid(in.appid());
  }
  if (in.hasHmacCreateSecret()) {
    json->setHmacCreateSecret(in.hmacCreateSecret());
  }
  if (in.hasCredProps()) {
    json->setCredProps(in.credProps());
  }
  if (in.hasLargeBlob()) {
    V8ObjectBuilder builder(script_state);
    const auto* large_blob = in.largeBlob();
    if (large_blob->hasSupported()) {
      builder.AddBoolean("supported", large_blob->supported());
    }
    if (large_blob->hasBlob()) {
      builder.AddString("blob", WebAuthnBase64UrlEncode(large_blob->blob()));
    }
    if (large_blob->hasWritten()) {
      builder.AddBoolean("written", large_blob->written());
    }
    json->setLargeBlob(builder.GetScriptValue());
  }
  if (in.hasCredBlob()) {
    json->setCredBlob(in.getCredBlob());
  }
  if (in.hasGetCredBlob()) {
    json->setGetCredBlob(WebAuthnBase64UrlEncode(in.getCredBlob()));
  }
  if (in.hasPrf()) {
    V8ObjectBuilder builder(script_state);
    const AuthenticationExtensionsPRFOutputs& prf = *in.prf();
    if (prf.hasEnabled()) {
      builder.AddBoolean("enabled", prf.enabled());
    }
    if (prf.hasResults()) {
      V8ObjectBuilder results_builder(script_state);
      results_builder.AddString(
          "first", WebAuthnBase64UrlEncode(prf.results()->first()));
      if (prf.results()->hasSecond()) {
        results_builder.AddString(
            "second", WebAuthnBase64UrlEncode(prf.results()->second()));
      }
    }
    json->setPrf(builder.GetScriptValue());
  }
  if (in.hasSupplementalPubKeys()) {
    const AuthenticationExtensionsSupplementalPubKeysOutputs&
        supplemental_pub_keys = *in.supplementalPubKeys();
    V8ObjectBuilder builder(script_state);
    if (supplemental_pub_keys.hasSignatures()) {
      builder.AddVector<DOMArrayBuffer>("signatures",
                                        supplemental_pub_keys.signatures());
    }
    json->setSupplementalPubKeys(builder.GetScriptValue());
  }
  return json;
}

PublicKeyCredentialCreationOptions* PublicKeyCredentialOptionsFromJSON(
    ScriptState* script_sate,
    const PublicKeyCredentialCreationOptionsJSON* json,
    ExceptionState& exception_state) {
  auto* result = PublicKeyCredentialCreationOptions::Create();
  result->setRp(json->rp());
  if (auto user = PublicKeyCredentialUserEntityFromJSON(*json->user());
      user.has_value()) {
    result->setUser(user.value());
  } else {
    exception_state.ThrowDOMException(DOMExceptionCode::kEncodingError,
                                      user.error());
    return nullptr;
  }
  if (auto challenge = WebAuthnBase64UrlDecode(json->challenge());
      challenge.has_value()) {
    result->setChallenge(
        MakeGarbageCollected<V8UnionArrayBufferOrArrayBufferView>(*challenge));
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kEncodingError,
        "'challenge' contains invalid base64url data");
    return nullptr;
  }
  result->setPubKeyCredParams(json->pubKeyCredParams());
  if (json->hasTimeout()) {
    result->setTimeout(json->timeout());
  }
  if (json->hasExcludeCredentials()) {
    VectorOf<PublicKeyCredentialDescriptor> exclude_credentials;
    for (const PublicKeyCredentialDescriptorJSON* json_descriptor :
         json->excludeCredentials()) {
      if (auto descriptor =
              PublicKeyCredentialDescriptorFromJSON(*json_descriptor);
          descriptor.has_value()) {
        exclude_credentials.push_back(descriptor.value());
      } else {
        exception_state.ThrowDOMException(DOMExceptionCode::kEncodingError,
                                          descriptor.error());
        return nullptr;
      }
    }
    result->setExcludeCredentials(std::move(exclude_credentials));
  }
  if (json->hasAuthenticatorSelection()) {
    result->setAuthenticatorSelection(json->authenticatorSelection());
  }
  if (json->hasHints()) {
    result->setHints(json->hints());
  }
  if (json->hasAttestation()) {
    result->setAttestation(json->attestation());
  }
  if (json->hasExtensions()) {
    if (auto extensions = AuthenticationExtensionsClientInputsFromJSON(
            *json->extensions(), exception_state);
        extensions.has_value()) {
      result->setExtensions(extensions.value());
    } else {
      exception_state.ThrowDOMException(DOMExceptionCode::kEncodingError,
                                        extensions.error());
      return nullptr;
    }
  }
  return result;
}

}  // namespace blink
