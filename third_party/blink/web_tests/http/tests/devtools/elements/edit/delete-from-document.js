// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that removing child from the document is handled properly in the elements panel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN" "http://www.w3.org/TR/html4/loose.dtd">
    `);
  await TestRunner.evaluateInPagePromise(`
      function removeDoctype()
      {
          document.removeChild(document.firstChild);
      }
  `);

  ElementsTestRunner.expandElementsTree(step1);

  function step1() {
    TestRunner.addResult('Before remove doctype');
    ElementsTestRunner.dumpElementsTree(null, 1);
    TestRunner.evaluateInPage('removeDoctype()', step2);
  }

  function step2() {
    TestRunner.addResult('After remove doctype');
    ElementsTestRunner.dumpElementsTree(null, 1);
    TestRunner.completeTest();
  }
})();
