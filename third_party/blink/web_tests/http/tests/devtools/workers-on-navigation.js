// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that workers are correctly detached upon navigation.\n`);

  // Suppress the following protocol error from being printed (which had a race condition):
  // error: Connection is closed, can't dispatch pending Debugger.setBlackboxPatterns
  console.error = () => undefined;

  var workerTargetId;
  var navigated = false;
  var workerAddedCallback;
  var workerAddedPromise = new Promise(f => workerAddedCallback = f);
  var observer = {
    targetAdded(target) {
      if (target.type() !== SDK.Target.Type.Worker)
        return;
      TestRunner.addResult('Worker added');
      workerTargetId = target.id();
      if (navigated)
        TestRunner.completeTest();
      else
        workerAddedCallback();
    },
    targetRemoved(target) {
      if (target.type() !== SDK.Target.Type.Worker)
        return;
      if (target.id() === workerTargetId) {
        TestRunner.addResult('Worker removed');
        workerTargetId = '';
      } else {
        TestRunner.addResult('Unknown worker removed');
      }
    }
  };
  SDK.TargetManager.TargetManager.instance().observeTargets(observer);
  await TestRunner.navigatePromise('resources/workers-on-navigation-resource.html');
  TestRunner.evaluateInPagePromise('startWorker()');
  await workerAddedPromise;
  await TestRunner.reloadPagePromise();
  navigated = true;
  await TestRunner.evaluateInPagePromise('startWorker()');
})();
