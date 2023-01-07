// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verify that navigator properly handles main frame navigated event.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadTestModule('bindings_test_runner');

  var sourcesNavigator = new Sources.NetworkNavigatorView();
  sourcesNavigator.show(UI.inspectorView.element);

  TestRunner.markStep('dumpInitialNavigator');
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('attachFrame');
  await Promise.all([
    BindingsTestRunner.attachFrame('frame', './resources/sourcemap-frame.html'),
    BindingsTestRunner.waitForSourceMap('sourcemap-script.js.map'),
    BindingsTestRunner.waitForSourceMap('sourcemap-style.css.map'),
  ]);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.markStep('navigateMainFrame');
  var url = 'http://127.0.0.1:8000/devtools/bindings/resources/empty-page.html';
  await TestRunner.navigatePromise(url);
  SourcesTestRunner.dumpNavigatorView(sourcesNavigator, false);

  TestRunner.completeTest();
})();
