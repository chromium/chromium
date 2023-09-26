// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(
      `This test verifies that author shadow root's #document-fragment is displayed and user-agent one is hidden by default.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <div id="container">
          <div id="test1"></div>
          <div id="test2">only test</div>
          <div id="test3">with <span>elements</span></div>
          <input type="text" value="Test">
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      test1.attachShadow({mode: 'open'});
      test2.attachShadow({ mode: "open" });
      test3.attachShadow({ mode: "closed" });
    `);

  ElementsTestRunner.expandElementsTree(function() {
    var container = ElementsTestRunner.expandedNodeWithId('container');
    ElementsTestRunner.dumpElementsTree(container);
    TestRunner.completeTest();
  });
})();
