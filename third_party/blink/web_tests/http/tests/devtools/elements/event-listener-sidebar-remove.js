// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests removing event listeners in the Elements sidebar panel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <button id="node">Inspect Me</button>
      <button id="node-sibling">Inspect Sibling</button>
    `);
  await TestRunner.evaluateInPagePromise(`
      function setupEventListeners()
      {
          function f() {}
          function g() {}
          var button = document.getElementById("node");
          button.addEventListener("click", f, false);
          button.addEventListener("mouseover", f, false);
          var sibling = document.getElementById("node-sibling");
          sibling.addEventListener("click", g, false);
          sibling.addEventListener("mouseover", g, false);
      }

      setupEventListeners();
  `);

  Common.Settings.settingForTest('show-event-listeners-for-ancestors').set(false);


  function step1() {
    ElementsTestRunner.selectNodeWithId('node', function() {
      ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step2);
    });
  }

  function step2() {
    ElementsTestRunner.removeFirstEventListener();
    TestRunner.addResult('Listeners after removal:');
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step3, true);
  }

  function step3() {
    ElementsTestRunner.selectNodeWithId('node-sibling', function() {
      TestRunner.addResult('Listeners for sibling node:');
      ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step4);
    });
  }

  function step4() {
    TestRunner.completeTest();
  }

  step1();
})();
