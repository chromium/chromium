// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests event listeners output in the Elements sidebar panel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <button id="node">Inspect Me</button>
    `);
  await TestRunner.addScriptTag('resources/jquery-1.11.3.min.js');
  await TestRunner.evaluateInPagePromise(`
      function setupEventListeners()
      {
          var node = $("#node")[0];
          $("#node").click(function(){ console.log("first jquery"); });
          $("#node").click(function(){ console.log("second jquery"); });
          node.addEventListener("click", function() { console.log("addEventListener"); });
      }

      setupEventListeners();
  `);

  Common.Settings.settingForTest('show-event-listeners-for-ancestors').set(true);
  ElementsTestRunner.selectNodeWithId('node', step1);

  function step1() {
    ElementsTestRunner.showEventListenersWidget();
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(TestRunner.completeTest.bind(this));
  }
})();
