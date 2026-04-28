// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

/**
 * @fileoverview API for the AIM Cobrowse feature.
 */

/**
 * Posts a message to the window.
 * @param {string} base64Message The base64 encoded message.
 */
function postMessage(base64Message: string): void {
  const binary = atob(base64Message);
  const queryMsg = Uint8Array.from(binary, c => c.charCodeAt(0));
  window.postMessage(queryMsg, window.location.origin);
}

const aimCobrowseApi = new CrWebApi('aimCobrowse');

aimCobrowseApi.addFunction('postMessage', postMessage);

gCrWeb.registerApi(aimCobrowseApi);
