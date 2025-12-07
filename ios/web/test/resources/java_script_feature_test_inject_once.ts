// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Setup functions used in JavaScriptFeature inttests. This file
 * will be executed once for a given `window` JS object.
 */

import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';
import {sendWebKitMessage} from '//ios/web/public/js_messaging/resources/utils.js';

const errorReceivedCount_: number = 0;

function getErrorCount() {
  return gCrWeb.getRegisteredApi('javaScriptFeatureTest')
      .getProperty('errorReceivedCount');
}

function replaceDivContents() {
  const div = document.getElementById('div');
  if (div) {
    div.innerHTML = 'updated';
  }
}

function replyWithPostMessage(messageBody: object) {
  sendWebKitMessage('FakeHandlerName', messageBody);
}

const body = document.getElementsByTagName('body')[0];
if (body) {
  body.appendChild(document.createTextNode('injected_script_loaded'));
}

const javaScriptFeatureTest = new CrWebApi();

javaScriptFeatureTest.addFunction('getErrorCount', getErrorCount);
javaScriptFeatureTest.addFunction('replaceDivContents', replaceDivContents);
javaScriptFeatureTest.addFunction('replyWithPostMessage', replyWithPostMessage);
javaScriptFeatureTest.addProperty('errorReceivedCount', errorReceivedCount_);

gCrWeb.registerApi('javaScriptFeatureTest', javaScriptFeatureTest);
