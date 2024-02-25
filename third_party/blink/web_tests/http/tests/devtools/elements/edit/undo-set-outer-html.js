// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests undo for the DOMAgent.setOuterHTML protocol method.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container" style="display:none">
      <p>WebKit is used by <a href="http://www.apple.com/safari/">Safari</a>, Dashboard, etc..</p>
      <h2>Getting involved</h2>
      <p id="identity">There are many ways to get involved. You can:</p>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      document.getElementById("identity").wrapperIdentity = "identity";
  `);

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.setUpTestSuite(next);
    },

    function testChangeCharacterData(next) {
      ElementsTestRunner.patchOuterHTMLUseUndo('Getting involved', 'Getting not involved', next);
    },

    function testChangeAttributes(next) {
      ElementsTestRunner.patchOuterHTMLUseUndo('<a href', '<a foo="bar" href', next);
    },

    function testRemoveLastChild(next) {
      ElementsTestRunner.patchOuterHTMLUseUndo('Getting involved', '', next);
    },

    function testSplitNode(next) {
      ElementsTestRunner.patchOuterHTMLUseUndo('Getting involved', 'Getting</h2><h2>involved', next);
    },

    function testChangeNodeName(next) {
      ElementsTestRunner.patchOuterHTMLUseUndo('<h2>Getting involved</h2>', '<h3>Getting involved</h3>', next);
    }
  ]);
})();
