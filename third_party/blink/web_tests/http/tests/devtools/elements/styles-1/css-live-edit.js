// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that styles are updated when live-editing css resource.\n`);
  await TestRunner.loadModule('elements'); await TestRunner.loadTestModule('elements_test_runner');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="foo"></div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/css-live-edit.css');

  TestRunner.runTestSuite([function testLiveEdit(next) {
    SourcesTestRunner.showScriptSource('css-live-edit.css', didShowResource);

    function didShowResource(sourceFrame) {
      TestRunner.addSniffer(SDK.CSSModel.prototype, 'fireStyleSheetChanged', didEditResource);
      SourcesTestRunner.replaceInSource(sourceFrame, 'font-size: 12px;', 'font-size: 20px;');
    }

    function didEditResource() {
      ElementsTestRunner.selectNodeAndWaitForStylesWithComputed('foo', didSelectElement);
    }

    async function didSelectElement() {
      await ElementsTestRunner.dumpSelectedElementStyles(false, true);
      next();
    }
  }]);
})();
