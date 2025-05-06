// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrWebApi, gCrWeb, gCrWebLegacy} from '//ios/web/public/js_messaging/resources/gcrweb.js';

const stringProperty = 'Set a property with test purpose.';
const arrayProperty: number[] = [];

function addNum(a: number, b: number): number {
  return a + b;
}

function setupRegisterApi() {
  const testApi = new CrWebApi();

  testApi.addProperty('stringProperty', stringProperty);
  testApi.addFunction('addNum', addNum);
  testApi.addProperty('arrayProperty', arrayProperty);

  gCrWeb.registerApi('crWebAPI', testApi);
}

gCrWebLegacy.unit_tests = {
  registerApi: gCrWeb.registerApi.bind(gCrWeb),
  getRegisteredApi: gCrWeb.getRegisteredApi.bind(gCrWeb),
  getFrameId: gCrWeb.getFrameId.bind(gCrWeb),
  setupRegisterApi,
};
