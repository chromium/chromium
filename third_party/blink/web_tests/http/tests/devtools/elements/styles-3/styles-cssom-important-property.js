// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that !important modifier is shown for CSSOM-generated properties.`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
    <style id="mycss"></style>
    <script>mycss.sheet.insertRule("div{color:red !important;}",0);</script>
    <div id="inspected">test</div>
  `);

  await new Promise(x => ElementsTestRunner.selectNodeAndWaitForStyles('inspected', x));
  await ElementsTestRunner.dumpSelectedElementStyles(true, false, true);
  TestRunner.completeTest();
})();
