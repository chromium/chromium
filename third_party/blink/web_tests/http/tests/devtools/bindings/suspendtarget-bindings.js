// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that bindings handle target suspension as expected.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  TestRunner.markStep('initialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('createIframesAndWaitForSourceMaps');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/sourcemap-frame.html', '_test_create-iframe1.js'),
    BindingsTestRunner.attachFrame('frame2', './resources/sourcemap-frame.html', '_test_create-iframe2.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map')
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('Suspending targets.');
  await SDK.targetManager.suspendAllTargets();
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');

  TestRunner.markStep('Resuming targets.');
  await Promise.all([
    SDK.targetManager.resumeAllTargets(), BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map')
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
