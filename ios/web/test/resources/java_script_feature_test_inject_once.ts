// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Setup functions used in JavaScriptFeature inttests. This file
 * will be executed once for a given `window` JS object.
 */

import {gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

let errorReceivedCount: number = 0;

function getErrorCount() {
  return gCrWeb.javaScriptFeatureTest.errorReceivedCount;
}

function replaceDivContents() {
  const div = document.getElementById('div')
  if (div) {
    div.innerHTML = 'updated';
  }
}

function replyWithPostMessage(messageBody: object) {
  sendWebKitMessage('FakeHandlerName', messageBody);
}

// This function offers the same functionality as `replyWithPostMessage`, but
// uses the sendWebKitMessage implementation in __gCrWeb.common.
function replyWithPostMessageCommonHelper(messageBody: object) {
  sendWebKitMessage('FakeHandlerName', messageBody);
}

const body = document.getElementsByTagName('body')[0];
if (body) {
  body.appendChild(document.createTextNode('injected_script_loaded'));
}

gCrWeb.javaScriptFeatureTest = {
  errorReceivedCount,
  getErrorCount,
  replaceDivContents,
  replyWithPostMessage,
  replyWithPostMessageCommonHelper
};
