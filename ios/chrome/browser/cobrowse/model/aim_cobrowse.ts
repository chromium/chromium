// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

/**
 * @fileoverview API for the AIM Cobrowse feature.
 */

/**
 * Sends a message from native to web (exposing it via postMessage to the page).
 * @param {string} base64Message The base64 encoded message.
 */
function sendNativeToWeb(base64Message: string): void {
  const binary = atob(base64Message);
  const queryMsg = Uint8Array.from(binary, c => c.charCodeAt(0));
  window.postMessage(queryMsg, window.location.origin);
}
const aimCobrowseApi = new CrWebApi('aimCobrowse');
aimCobrowseApi.addFunction('sendNativeToWeb', sendNativeToWeb);
gCrWeb.registerApi(aimCobrowseApi);

/**
 * Sends a message directly to the native iOS client via WebKit.
 * @param {string} base64Message The base64 encoded message.
 */
function sendWebToNative(base64Message: string): void {
  sendWebKitMessage('AimCobrowseMessageHandler', { 'message': base64Message });
}

// Expose the sendWebToNative API under a dedicated global object __gAimCobrowse
// to avoid polluting the gCrWeb object.
(window as any).__gAimCobrowse = {
  sendWebToNative: sendWebToNative,
};
