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
  create: function(options?: CredentialCreationOptions|undefined):
      Promise<Credential|null> {
        // Only process WebAuthn requests.
        if (options?.publicKey) {
          sendWebKitMessage(HANDLER_NAME, {'event': 'createRequested'});
        }
        return cachedNavigatorCredentials.create(options);
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
