// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  TestRunner.addResult(`Verify that UISourceCodes are removed as iframes are getting detached.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  TestRunner.markStep('initialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('createIframes');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/magic-frame.html', '_test_create-iframe1.js'),
    BindingsTestRunner.attachFrame('frame2', './resources/magic-frame.html', '_test_create-iframe2.js'),
  ]);
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame1');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame1.js');
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame2');
  await BindingsTestRunner.detachFrame('frame2', '_test_detachFrame2.js');
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.completeTest();
})();
