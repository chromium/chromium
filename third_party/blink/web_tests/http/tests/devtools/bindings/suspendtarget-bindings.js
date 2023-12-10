// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {BindingsTestRunner} from 'bindings_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verify that bindings handle target suspension as expected.\n`);

  TestRunner.markStep('initialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('createIframesAndWaitForSourceMaps');
  await BindingsTestRunner.attachFrame('frame1', './resources/sourcemap-frame.html', '_test_create-iframe1.js');
  await Promise.all([
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  await BindingsTestRunner.attachFrame('frame2', './resources/sourcemap-frame.html', '_test_create-iframe2.js');
  await Promise.all([
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('Suspending targets.');
  await SDK.TargetManager.TargetManager.instance().suspendAllTargets();
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');

  TestRunner.markStep('Resuming targets.');
  await Promise.all([
    SDK.TargetManager.TargetManager.instance().resumeAllTargets(), BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map')
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
