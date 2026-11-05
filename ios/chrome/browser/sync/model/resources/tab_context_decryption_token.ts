// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {sendWebKitMessageWithReply} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview Exposes a `chrome.getContainerDecryptionToken` API to pages
 * hosted on the allowed tab context origin (`https://chromestorage.goog`).
 */

const MESSAGE_HANDLER_NAME = 'TabContextDecryptionTokenMessage';

// Ensure `window.chrome` exists.
if (!(window as any).chrome) {
  (window as any).chrome = {};
}

const chrome = (window as any).chrome;

/**
 * Fetches the decryption token for the specified tab context container.
 *
 * @param callback Called with the decryption token as an `ArrayBuffer`, or
 *     `null` if unavailable.
 * @param obfuscatedGaiaId The obfuscated Gaia ID of the signed-in user.
 * @param containerId The UUID string identifying the container.
 */
chrome.getContainerDecryptionToken = function(
    callback: (token: ArrayBuffer|null) => void, obfuscatedGaiaId: string,
    containerId: string): void {
  // Mirror the synchronous `TypeError` argument type validation performed by
  // Gin bindings in
  // `chrome/renderer/tab_context_decryption_token_extension.cc`.
  if (typeof callback !== 'function' || typeof obfuscatedGaiaId !== 'string' ||
      typeof containerId !== 'string') {
    throw new TypeError('Invalid arguments.');
  }

  sendWebKitMessageWithReply(MESSAGE_HANDLER_NAME, {
    'obfuscatedGaiaId': obfuscatedGaiaId,
    'containerId': containerId,
  })
      .then(
          (reply: unknown) => {
            if (!Array.isArray(reply)) {
              callback(null);
              return;
            }
            callback(new Uint8Array(reply).buffer);
          },
          () => {
            callback(null);
          });
};
