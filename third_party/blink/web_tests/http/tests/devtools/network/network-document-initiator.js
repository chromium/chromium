// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that page navigation initiated by JS is correctly reported.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.navigatePromise('resources/initiator.html');
  TestRunner.runWhenPageLoads(step1);
  TestRunner.resourceTreeModel.addEventListener(SDK.ResourceTreeModel.Events.Load, TestRunner.pageLoaded);
  await TestRunner.evaluateInPage('navigateFromScript()');

  function dumpInitiator(request) {
    var initiator = request.initiator();
    TestRunner.addResult(request.url() + ': ' + initiator.type);
    if (initiator.url)
      TestRunner.addResult('    ' + initiator.url + ' ' + initiator.lineNumber);
    var stackTrace = initiator.stack;
    if (!stackTrace)
      return;
    for (var i = 0; i < stackTrace.callFrames.length; ++i) {
      var frame = stackTrace.callFrames[i];
      if (frame.lineNumber) {
        TestRunner.addResult('    ' + frame.functionName + ' ' + frame.url + ' ' + frame.lineNumber);
        return;
      }
    }
  }

  function step1() {
    var results = NetworkTestRunner.findRequestsByURLPattern(/\?foo/).filter(
        (e, i, a) => i % 2 == 0);
    TestRunner.assertEquals(1, results.length);
    dumpInitiator(results[0]);
    TestRunner.completeTest();
  }
})();
