// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Enables passkey-related interactions between the browser and
 * the renderer by shimming the `navigator.credentials` API.
 */

import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

// Must be kept in sync with passkey_java_script_feature.mm.
const HANDLER_NAME = 'PasskeyInteractionHandler';

// AAGUID value of Google Password Manager.
const GPM_AAGUID = new Uint8Array([
  // clang-format off
  0xea, 0x9b, 0x8d, 0x66, 0x4d, 0x01, 0x1d, 0x21,
  0x3c, 0xe4, 0xb6, 0xb4, 0x8c, 0xb5, 0x75, 0xd4,
  // clang-format on
]);

// Checks whether provided aaguid is equal to Google Password Manager's aaguid.
function isGpmAaguid(aaguid: Uint8Array): boolean {
  if (aaguid.byteLength !== GPM_AAGUID.byteLength) {
    return false;
  }
  for (let i = 0; i < aaguid.byteLength; i++) {
    if (aaguid[i] !== GPM_AAGUID[i]) {
      return false;
    }
  }
  return true;
}

/**
 * Caches the existing value of WebKit's navigator.credentials, so that the
 * calls can be forwarded to it, if needed.
 */
const cachedNavigatorCredentials: CredentialsContainer = navigator.credentials;

/**
 * Chromium-specific implementation of CredentialsContainer.
 */
const credentialsContainer: CredentialsContainer = {
  get: function(options?: CredentialRequestOptions): Promise<Credential|null> {
    // Only process WebAuthn requests.
    if (!options?.publicKey) {
      return cachedNavigatorCredentials.get(options);
    }

    sendWebKitMessage(HANDLER_NAME, {'event': 'getRequested'});

    return cachedNavigatorCredentials.get(options).then((credential) => {
      if (credential && credential instanceof PublicKeyCredential) {
        // rpId is an optional member of publicKey. Default value (caller's
        // origin domain) should be used if it is not specified
        // (https://w3c.github.io/webauthn/#dom-publickeycredentialrequestoptions-rpid).
        const rpId = options!.publicKey!.rpId ?? document.location.host;
        sendWebKitMessage(HANDLER_NAME, {
          'event': 'getResolved',
          'credential_id': credential.id,
          'rp_id': rpId,
        });
      }
      return credential;
    });
  },
  create: function(
      options?: CredentialCreationOptions|undefined): Promise<Credential|null> {
    // Only process WebAuthn requests.
    if (!options?.publicKey) {
      return cachedNavigatorCredentials.create(options);
    }

    sendWebKitMessage(HANDLER_NAME, {'event': 'createRequested'});

    return cachedNavigatorCredentials.create(options).then((credential) => {
      if (credential && credential instanceof PublicKeyCredential &&
          credential.response instanceof AuthenticatorAttestationResponse) {
        // Parse the aaguid from authenticator data according to
        // https://w3c.github.io/webauthn/#sctn-authenticator-data.
        const aaguid = new Uint8Array(
            credential.response.getAuthenticatorData().slice(37).slice(0, 16));
        sendWebKitMessage(HANDLER_NAME, {
          'event': isGpmAaguid(aaguid) ? 'createResolvedGpm' :
                                         'createResolvedNonGpm',
        });
      }
      return credential;
    });
  },
  preventSilentAccess: function(): Promise<any> {
    return cachedNavigatorCredentials.preventSilentAccess();
  },
  store: function(credentials?: any): Promise<any> {
    return cachedNavigatorCredentials.store(credentials);
  },
};

// Override the existing value of `navigator.credentials` with our own. The use
// of Object.defineProperty (versus just doing `navigator.credentials = ...`) is
// a workaround for the fact that `navigator.credentials` is readonly.
Object.defineProperty(navigator, 'credentials', {value: credentialsContainer});
