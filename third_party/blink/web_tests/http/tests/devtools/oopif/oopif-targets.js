// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that targets are created for oopif iframes.\n`);

  TestRunner.addResult('\nTargets before navigate');
  TestRunner.addResults(SDK.TargetManager.TargetManager.instance().targets().map(t => t.name()).sort());

  await TestRunner.navigatePromise('resources/page.html');
  TestRunner.addResult('\nTargets after navigate');
  TestRunner.addResults(SDK.TargetManager.TargetManager.instance().targets().map(t => t.name()).sort());

  await TestRunner.navigatePromise('about:blank');
  TestRunner.addResult('\nTargets on about:blank');
  TestRunner.addResults(SDK.TargetManager.TargetManager.instance().targets().map(t => t.name()).sort());

  TestRunner.completeTest();
})();
