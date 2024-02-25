// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that style properties of elements in iframes loaded from domain different from the main document domain can be inspected. See bug 31587.\n`);
  await TestRunner.navigatePromise("http://example.test:8000/devtools/resources/empty.html");
  // NOTE: evaluateInPageAsync() waits on the promise at the end of block before
  // resolving the promise it returned. Other forms of the evaluate including
  // evaluateInPagePromise() do not do this.
  await TestRunner.evaluateInPageAsync(`
    const frame = document.createElement('iframe');
    frame.src = 'http://other.domain.example.test:8000/devtools/resources/iframe-from-different-domain-data.html';
    document.body.appendChild(frame);
    new Promise(f => frame.onload = f);
  `);

  ElementsTestRunner.selectNodeAndWaitForStyles('iframe-body', step1);

  function step1() {
    var treeItem = ElementsTestRunner.getElementStylePropertyTreeItem('background');
    ElementsTestRunner.dumpStyleTreeItem(treeItem, '');
    TestRunner.completeTest();
  }
})();
