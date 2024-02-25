// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that the inspected page does not crash in a debug build when reloading a page containing shadow DOM with open inspector. Bug 84154. https://bugs.webkit.org/show_bug.cgi?id=84154\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
      <input type="radio">
    `);

  ElementsTestRunner.expandElementsTree(step1);

  function step1() {
    TestRunner.reloadPage(step2);
  }

  function step2() {
    TestRunner.completeTest();
  }
})();
