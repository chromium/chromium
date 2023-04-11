// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_JSON_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_JSON_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class AuthenticationExtensionsClientOutputsJSON;
class AuthenticationExtensionsClientOutputs;
class ScriptState;

// WebAuthn JSON-encodes binary-valued fields as Base64URL without trailing '='
// padding characters.
WTF::String WebAuthnBase64UrlEncode(DOMArrayPiece buffer);

AuthenticationExtensionsClientOutputsJSON*
AuthenticationExtensionsClientOutputsToJSON(
    ScriptState* script_state,
    const AuthenticationExtensionsClientOutputs& extension_outputs);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CREDENTIALMANAGEMENT_JSON_H_
