// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel correctly displays DOM tree structure for bi-di pages.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <div title="ویکی‌پدیا:خوش‌آمدید">ویکی‌پدیا:خوش‌آمدید</div>
    `);

  // Warm up highlighter module.
  ElementsTestRunner.expandElementsTree(step1);

  function step1() {
    ElementsTestRunner.dumpElementsTree();
    TestRunner.completeTest();
  }
})();
