// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(
      `Tests framework black-boxing on DOM, XHR and Event breakpoints.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="rootElement"></div>
      <input type="button" id="test">
    `);
  await TestRunner.addScriptTag('../debugger/resources/framework.js');
  await TestRunner.evaluateInPagePromise(`
      function appendElement(parentId, childId)
      {
          var child = document.createElement("div");
          child.id = childId;
          var parent = document.getElementById(parentId);
          Framework.appendChild(parent, child);
      }

      function sendXHR(url)
      {
          Framework.sendXHR(url);
      }

      function addListenerAndClick(stop)
      {
          function testElementClicked()
          {
              return 0;
          }

          var button = document.getElementById("test");
          var remover = Framework.addEventListener(button, "click", Framework.bind(Framework.empty, null), true); // Should be ignored.
          if (stop)
              debugger;
          button.click();
          remover();

          remover = Framework.addEventListener(button, "click", Framework.bind(testElementClicked, null), true);
          button.click();
          remover();

          // Test both handlers together.
          var remover1 = Framework.addEventListener(button, "click", Framework.bind(Framework.empty, null), true); // Should be ignored.
          var remover2 = Framework.addEventListener(button, "click", Framework.bind(testElementClicked, null), true);
          button.click();
          remover1();
          remover2();
      }

      function addFewBlackboxedListenersAndClick(addNonBlackboxedListener)
      {
          function testElementClicked()
          {
              return 0;
          }
          function inner()
          {
              var button = document.getElementById("test");
              var remover1 = Framework.addEventListener(button, "click", Framework.empty, true);
              var remover2 = Framework.addEventListener(button, "click", Framework.bind(Framework.throwFrameworkException, null, "EXPECTED"), true);
              var remover3 = Framework.addEventListener(button, "click", Framework.bind(Framework.safeRun, null, Framework.empty, Framework.empty, Framework.empty), true);
              var remover4 = function() {};
              if (addNonBlackboxedListener)
                  remover4 = Framework.addEventListener(button, "click", Framework.bind(Framework.safeRun, null, Framework.empty, testElementClicked, Framework.empty), true);
              debugger;
              button.click();
              remover1();
              remover2();
              remover3();
              remover4();
          }
          return inner();
      }
  `);

  var frameworkRegexString = '/framework\\.js$';
  Common.Settings.settingForTest('skip-stack-frames-pattern').set(frameworkRegexString);

  SourcesTestRunner.setQuiet(true);

  SourcesTestRunner.runDebuggerTestSuite([
    function testDOMBreakpoint(next) {
      ElementsTestRunner.nodeWithId('rootElement', step1);

      function step1(node) {
        TestRunner.domDebuggerModel.setDOMBreakpoint(
            node, Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
        TestRunner.evaluateInPageWithTimeout(
            'appendElement(\'rootElement\', \'childElement\')');
        SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
      }
    },

    function testXHRBreakpoint(next) {
      SDK.DOMDebuggerModel.DOMDebuggerManager.instance().addXHRBreakpoint('foo', true);
      TestRunner.evaluateInPageWithTimeout('sendXHR(\'resources/foo?xhr\')');
      SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
    },

    function testEventListenerBreakpoint(next) {
      SDK.DOMDebuggerModel.DOMDebuggerManager.instance()
          .resolveEventListenerBreakpoint({eventName: 'listener:click'})
          .setEnabled(true);
      TestRunner.evaluateInPageWithTimeout('addListenerAndClick(false)');
      SourcesTestRunner.waitUntilPausedAndPerformSteppingActions(
          [
            'Print',
            'Resume',
            'Print',
            'Resume',
          ],
          next);
    }
  ]);
})();
