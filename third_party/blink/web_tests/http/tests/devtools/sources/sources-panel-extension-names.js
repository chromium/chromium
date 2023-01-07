// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`The test verifies that extension names are resolved properly in navigator view.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');

  var contentScriptsNavigatorView = new Sources.ContentScriptsNavigatorView();
  contentScriptsNavigatorView.show(UI.inspectorView.element);

  var mockExecutionContext =
      {id: 1234567, isDefault: false, origin: 'chrome-extension://113581321345589144', name: 'FibExtension'};
  var mockContentScriptURL = mockExecutionContext.origin + '/script.js';

  TestRunner.runTestSuite([
    async function testAddExecutionContextBeforeFile(next) {
      TestRunner.runtimeModel.executionContextCreated(mockExecutionContext);
      await SourcesTestRunner.addScriptUISourceCode(mockContentScriptURL, '', true, 1234567);
      SourcesTestRunner.dumpNavigatorView(contentScriptsNavigatorView);
      next();
    },
  ]);
})();
