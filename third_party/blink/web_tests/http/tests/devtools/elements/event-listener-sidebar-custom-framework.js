// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {ConsoleTestRunner} from 'console_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests framework event listeners output in the Elements sidebar panel.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <button id="inspectedNode">Inspect Me</button>
    `);
  await TestRunner.evaluateInPagePromise(`
      function setupNormalPath()
      {
          var inspectedNode = document.getElementById("inspectedNode");

          inspectedNode.addEventListener("click", internalHandler);

          function customFirstEventListener(e)
          {
              console.log("I'm first custom event listener");
          }

          function customSecondEventListener(e)
          {
              console.log("I'm second custom event listener");
          }

          function internalHandler(e)
          {
              console.log("I'm internal event handler");
              if (e.type === "customFirst")
                  customFirstEventListener(e);
              if (e.type === "customSecond")
                  customSecondEventListener(e);
          }

          // Example of API usage.
          window.devtoolsFrameworkEventListeners = window.devtoolsFrameworkEventListeners || [];
          window.devtoolsFrameworkEventListeners.push(frameworkEventListeners);

          function frameworkEventListeners(node)
          {
              if (node === inspectedNode) {
                  return {eventListeners: [{type: "customFirst", useCapture: true, passive: false, once: false, handler: customFirstEventListener},
                                           {type: "customSecond", useCapture: false, passive: false, once: false, handler: customSecondEventListener}],
                          internalHandlers: [internalHandler]};
              }
              return {eventListeners: []};
          }
      }

      function setupExceptionInGetter()
      {
          Object.defineProperty(window, "devtoolsFrameworkEventListeners", { get: function() { throw "Error in getter" }});
      }

      function setupReturnIncorrectResult()
      {
          window.devtoolsFrameworkEventListeners = window.devtoolsFrameworkEventListeners || [];
          window.devtoolsFrameworkEventListeners.push(frameworkEventListenersWithException);
          window.devtoolsFrameworkEventListeners.push(function(){ return null; });
          window.devtoolsFrameworkEventListeners.push(function(){ return undefined; });
          window.devtoolsFrameworkEventListeners.push(function(){ return {}; });
          window.devtoolsFrameworkEventListeners.push(function(){ return {eventListeners: null}; });
          window.devtoolsFrameworkEventListeners.push(function(){ return {eventListeners: undefined}; });
          window.devtoolsFrameworkEventListeners.push(function(){ return {eventListeners: [], internalHandlers: null}; });
          window.devtoolsFrameworkEventListeners.push(function(){ return {eventListeners: [], internalHandlers: undefined}; });
          window.devtoolsFrameworkEventListeners.push(function(){ return {eventListeners: [], internalHandlers: [undefined, null]}; });
          window.devtoolsFrameworkEventListeners.push(returnFrameworkEventListenersWithGetter);
          window.devtoolsFrameworkEventListeners.push(returnIncrrectEventListeners);

          function frameworkEventListenersWithException()
          {
              throw "Error in fetcher";
          }

          function returnFrameworkEventListenersWithGetter()
          {
              var obj = {};
              Object.defineProperty(obj, "eventListeners", { get: function() { throw "Error in getter" }});
              return obj;
          }

          function returnIncrrectEventListeners()
          {
              var listeners = [];
              listeners.push({});
              var listener = {type: "type", useCapture: true};
              Object.defineProperty(listener, "handler", { get: function() { throw "Error in getter" }});
              listeners.push(listener);
              listener = {type: "type", handler: (function(){})};
              Object.defineProperty(listener, "useCapture", { get: function() { throw "Error in getter"}});
              listeners.push(listener);
              return {eventListeners: listeners, internalHandlers: [239, null, undefined]};
          }
      }

      setupReturnIncorrectResult();
      setupNormalPath();
  `);

  Common.Settings.settingForTest('show-event-listeners-for-ancestors').set(false);
  ElementsTestRunner.selectNodeWithId('inspectedNode', step1);

  function step1() {
    TestRunner.addResult('== Incorrect fetchers');
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step2);
  }

  function step2() {
    TestRunner.evaluateInPage('setupExceptionInGetter()', step3);
  }

  function step3() {
    ElementsTestRunner.selectNodeWithId('inspectedNode', step4);
  }

  function step4() {
    TestRunner.addResult('== Exception in fetchers\' getter');
    ElementsTestRunner.expandAndDumpSelectedElementEventListeners(step5);
    ElementsTestRunner.eventListenersWidget().doUpdate();
  }

  async function step5() {
    await ConsoleTestRunner.dumpConsoleMessages(false, false, TestRunner.textContentWithLineBreaks);
    TestRunner.completeTest();
  }
})();
