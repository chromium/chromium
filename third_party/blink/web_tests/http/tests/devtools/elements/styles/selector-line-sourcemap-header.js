// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests that sourcemap is applied correctly when specified by the respective HTTP header.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container">
          <div id="inspected">Text</div>
      </div>
    `);
  await TestRunner.addStylesheetTag('../styles/resources/selector-line-sourcemap-header.php');

  SourcesTestRunner.waitForScriptSource('selector-line-sourcemap-header.scss', onSourceMap);

  function onSourceMap() {
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);
  }

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
