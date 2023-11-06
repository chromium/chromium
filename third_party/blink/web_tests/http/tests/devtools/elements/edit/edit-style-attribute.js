// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests that style modification generates attribute updated event only when attribute is actually changed.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <div id="container">
          <div id="node-set-new-value" style="color:red"></div>
          <div id="node-set-same-value" style="color:red"></div>
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function testSetNewValue()
      {
          document.getElementById("node-set-new-value").style.setProperty("color", "blue");
      }

      function testSetSameValue()
      {
          document.getElementById("node-set-same-value").style.setProperty("color", "red");
      }
  `);

  // Save time on style updates.
  ElementsTestRunner.ignoreSidebarUpdates();

  TestRunner.runTestSuite([
    function testSetUp(next) {
      ElementsTestRunner.expandElementsTree(next);
    },

    function testSetNewValue(next) {
      TestRunner.evaluateInPage('testSetNewValue()');

      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, listener);
      function listener(event) {
        TestRunner.addResult('SDK.DOMModel.Events.AttrModified should be issued');
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrModified, listener);
        next();
      }
    },

    function testSetSameValue(next) {
      TestRunner.evaluateInPage('testSetSameValue()', next);

      TestRunner.domModel.addEventListener(SDK.DOMModel.Events.AttrModified, listener);
      function listener(event) {
        TestRunner.addResult('SDK.DOMModel.Events.AttrModified should not be issued');
        TestRunner.domModel.removeEventListener(SDK.DOMModel.Events.AttrModified, listener);
      }
    }
  ]);
})();
