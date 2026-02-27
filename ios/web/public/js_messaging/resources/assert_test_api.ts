// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertNonNull, assertNotReached} from '//ios/web/public/js_messaging/resources/assert.js';
import {CrWebApi, gCrWeb} from '//ios/web/public/js_messaging/resources/gcrweb.js';

const testApi = new CrWebApi('assert_tests');

testApi.addFunction('assert', () => {
  assert(false, 'Fatal assertion.');
});

testApi.addFunction('assertType', () => {
  const documentElement = document.getElementsByTagName('body')[0];
  assertInstanceof(documentElement, HTMLInputElement);
});

testApi.addFunction('assertTypeWithCustomMessage', () => {
  const documentElement = document.getElementsByTagName('body')[0];
  assertInstanceof(
      documentElement, HTMLInputElement,
      'Element is not of expected type, HTMLInputElement.');
});

testApi.addFunction('assertNotReached', () => {
  assertNotReached();
});

testApi.addFunction('assertNotReachedWithCustomMessage', () => {
  assertNotReached('This code should never hit.');
});

testApi.addFunction('assertNonNull', () => {
  assertNonNull(null, 'Object can not be null');
});

gCrWeb.registerApi(testApi);
