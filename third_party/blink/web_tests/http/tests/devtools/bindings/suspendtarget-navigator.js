// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that navigator is rendered properly when targets are suspended and resumed.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('initialWorkspace');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFramesAndWaitForSourceMaps');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame1', './resources/sourcemap-frame.html', '_test_create-iframe1.js'),
    BindingsTestRunner.attachFrame('frame2', './resources/sourcemap-frame.html', '_test_create-iframe2.js'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('Suspending targets.');
  await SDK.targetManager.suspendAllTargets();
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('detachFrame');
  await BindingsTestRunner.detachFrame('frame1', '_test_detachFrame.js');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);
  await TestRunner.evaluateInPageAnonymously('GCController.collectAll()');

  TestRunner.markStep('Resuming targets.');
  await Promise.all([
    SDK.targetManager.resumeAllTargets(), BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map')
  ]);

  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
