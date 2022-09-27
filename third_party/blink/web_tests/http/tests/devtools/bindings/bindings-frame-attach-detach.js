// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  TestRunner.addResult(`Verify that UISourceCodes are added and removed as iframe gets attached and detached.\n`);
  await TestRunner.loadTestModule('bindings_test_runner');

  TestRunner.markStep('dumpInitialWorkspace');
  var snapshot = BindingsTestRunner.dumpWorkspace();

  TestRunner.markStep('attachFrame');
  await BindingsTestRunner.attachFrame('frame', './resources/magic-frame.html', '_test_attachFrame.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);

  TestRunner.markStep('detachFrame');
  await BindingsTestRunner.detachFrame('frame', '_test_detachFrame.js');
  snapshot = BindingsTestRunner.dumpWorkspace(snapshot);
  TestRunner.completeTest();
})();
