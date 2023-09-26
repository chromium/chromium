// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that CSSParser correctly parses declarations with unterminated comments.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="inspected1" style="color: red /* foo: bar;"></div>
      <div id="inspected2" style="color: green; /* foo: bar;"></div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected1', dumpStylesFirst);

  async function dumpStylesFirst() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    ElementsTestRunner.selectNodeAndWaitForStyles('inspected2', dumpStylesSecond);
  }

  async function dumpStylesSecond() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
