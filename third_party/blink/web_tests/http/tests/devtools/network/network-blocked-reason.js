// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that blocked reason is recognized correctly.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.navigatePromise('resources/csp.html');

  var requestName;
  var nextStep;
  SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockingEnabled(true);

  function onRequest(event) {
    var request = event.data;
    if (request.name() !== requestName)
      return;
    requestName = undefined;
    TestRunner.addResult('BlockedReason: ' + request.blockedReason());
    nextStep();
  }

  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestFinished, onRequest);

  function testBlockedURL(patterns, url, next) {
    TestRunner.addResult('Blocked patterns: ' + patterns.join(';'));
    TestRunner.addResult('Request: ' + url);
    var blockedPatterns = patterns.map(p => ({url: p, enabled: true}));
    SDK.NetworkManager.MultitargetNetworkManager.instance().setBlockedPatterns(blockedPatterns);
    nextStep = next;
    TestRunner.deprecatedRunAfterPendingDispatches(addScript);

    function addScript() {
      requestName = url.substring(url.lastIndexOf('/') + 1);
      TestRunner.evaluateInPage('addBlockedScript("' + url + '")');
    }
  }

  TestRunner.runTestSuite([
    function testCSP(next) {
      requestName = 'csp.js';
      nextStep = next;
      TestRunner.evaluateInPage('sendCSPRequest()');
    },

    function testBlockedByDevTools1(next) {
      testBlockedURL(['resources**/silent*.js'], 'silent_script.js', next);
    },

    function testBlockedByDevTools2(next) {
      testBlockedURL(['a*b'], 'ba', next);
    },

    function testBlockedByDevTools3(next) {
      testBlockedURL(['***pattern***'], 'there/is/a/pattern/inside.js', next);
    },

    function testBlockedByDevTools4(next) {
      testBlockedURL(['pattern'], 'patt1ern', next);
    },

    function testBlockedByDevTools5(next) {
      testBlockedURL(['*this***is*a*pattern'], 'file/this/is/the/pattern', next);
    },

    function testBlockedByDevTools6(next) {
      testBlockedURL(['*this***is*a*pattern'], 'this/is/a/pattern', next);
    },

    function testBlockedByDevTools6(next) {
      testBlockedURL(['*this***is*a*pattern'], 'this/is', next);
    },

    function testBlockedByDevTools7(next) {
      testBlockedURL(['pattern'], 'long/pattern/inside', next);
    },

    function testBlockedByDevTools8(next) {
      testBlockedURL(['pattern'], 'pattern', next);
    },

    function testBlockedByDevTools9(next) {
      testBlockedURL(['pattern', 'pattern'], 'pattern', next);
    },

    function testBlockedByDevTools10(next) {
      testBlockedURL(['a*b*c*d*e'], 'edcbaedcbaedcbaedcba', next);
    },

    function testBlockedByDevTools11(next) {
      testBlockedURL(['a*b*c*d*e'], 'edcbaedcbaedcbaedcbae', next);
    },

    function testBlockedByDevTools12(next) {
      testBlockedURL(['one1', 'two2'], 'one1two2', next);
    },

    function testBlockedByDevTools13(next) {
      testBlockedURL(['one1', 'two2', 'three3'], 'four4', next);
    },

    function testBlockedByDevTools14(next) {
      testBlockedURL(['one1', 'two2', 'three3'], 'only-two2-here', next);
    },

    function cleanupBlockedURLs(next) {
      testBlockedURL([], 'silent_script.js', next);
    }
  ]);
})();
