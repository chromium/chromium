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

      <div id="node-without-listeners"></div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function documentClickHandler(event) { console.log("click - document - attribute"); }

      function setupEventListeners()
      {
          function f() {}
          var button = document.getElementById("node");
          function clickHandler(event) { console.log("click - button - bubbling (registered before attribute)"); }
          button.addEventListener("click", clickHandler, false);
          button.addEventListener("hover", function hoverHandler(event) { console.log("hover - button - bubbling"); }, false);
          button.addEventListener("click", function(event) { console.log("click - button - capturing"); }, true);
          button.onclick = function(event) { console.log("click - button - attribute"); }
          button.addEventListener("click", function(event) { console.log("click - button - bubbling (registered after attribute)"); }, false);
          document.onclick = documentClickHandler;
          document.addEventListener("click", function(event) { console.log("click - document - capturing"); }, true);
          document.addEventListener("mousedown", f, false);
          document.removeEventListener("mousedown", f, false);

          document.body.addEventListener("custom event", f, {capture: true, once: true});

          function ObjectHandler() { document.addEventListener("click", this, true); }
          ObjectHandler.prototype.toString = function() { return "ObjectHandler"; }
          new ObjectHandler();

          function EventListenerImpl() { document.addEventListener("click", this, true); }
          EventListenerImpl.prototype.toString = function() { return "EventListenerImpl"; }
          EventListenerImpl.prototype.handleEvent = function() { console.log("click - document - handleEvent"); }
          new EventListenerImpl();
          document.body.addEventListener("wheel", f, {"passive": true});
          document.body.addEventListener("wheel", f, {"passive": true, "capture": true});
          document.body.removeEventListener("wheel", f, {"passive": true, "capture": true});
      }

      setupEventListeners();
  `);

  Common.Settings.settingForTest('show-event-listeners-for-ancestors').set(true);
  ElementsTestRunner.selectNodeWithId('node', step1);

  function step1() {
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step2);
  }

  function step2() {
    ElementsTestRunner.selectNodeWithId('node-without-listeners', step3);
  }

  function step3() {
    Common.Settings.settingForTest('show-event-listeners-for-ancestors').set(false);
    TestRunner.addResult('Listeners for selected node only(should be no listeners):');
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step4);
  }

  function step4() {
    TestRunner.completeTest();
  }
})();
