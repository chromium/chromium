#include "third_party/blink/renderer/modules/credentialmanagement/json.h"

#include "base/numerics/safe_conversions.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_object_builder.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_union_arraybuffer_arraybufferview.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_client_outputs_js_on.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_large_blob_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_outputs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_authentication_extensions_prf_values.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer_base.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"

namespace blink {

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
      results_builder.Add("first",
                          WebAuthnBase64UrlEncode(prf.results()->first()));
      if (prf.results()->hasSecond()) {
        results_builder.Add("second",
                            WebAuthnBase64UrlEncode(prf.results()->second()));
      }
    }
    json->setPrf(builder.GetScriptValue());
  }
  return json;
}

}  // namespace blink
