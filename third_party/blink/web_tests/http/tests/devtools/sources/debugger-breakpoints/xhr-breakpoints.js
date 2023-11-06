// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests XHR breakpoints.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function sendRequest(url)
      {
          var request = new XMLHttpRequest();
          request.open("GET", url, true);
          request.send()
      }
  `);

  SourcesTestRunner.runDebuggerTestSuite([
    function testXHRBreakpoint(next) {
      SDK.DOMDebuggerModel.DOMDebuggerManager.instance().addXHRBreakpoint('foo', true);
      SourcesTestRunner.waitUntilPaused(step1);
      TestRunner.evaluateInPageWithTimeout('sendRequest(\'/foo?a=b\')');

      async function step1(callFrames) {
        await SourcesTestRunner.captureStackTrace(callFrames);
        SourcesTestRunner.resumeExecution(step2);
      }

      function step2() {
        TestRunner.evaluateInPage('sendRequest(\'/bar?a=b\')', step3);
      }

      function step3() {
        SDK.DOMDebuggerModel.DOMDebuggerManager.instance().removeXHRBreakpoint('foo');
        TestRunner.evaluateInPage('sendRequest(\'/foo?a=b\')', next);
      }
    },

    function testPauseOnAnyXHR(next) {
      SDK.DOMDebuggerModel.DOMDebuggerManager.instance().addXHRBreakpoint('', true);
      SourcesTestRunner.waitUntilPaused(pausedFoo);
      TestRunner.evaluateInPageWithTimeout('sendRequest(\'/foo?a=b\')');

      function pausedFoo(callFrames) {
        function resumed() {
          SourcesTestRunner.waitUntilPaused(pausedBar);
          TestRunner.evaluateInPage('sendRequest(\'/bar?a=b\')');
        }
        SourcesTestRunner.resumeExecution(resumed);
      }

      function pausedBar(callFrames) {
        function resumed() {
          SDK.DOMDebuggerModel.DOMDebuggerManager.instance().removeXHRBreakpoint('');
          TestRunner.evaluateInPage('sendRequest(\'/baz?a=b\')', next);
        }
        SourcesTestRunner.resumeExecution(resumed);
      }
    },

    function testDisableBreakpoint(next) {
      SDK.DOMDebuggerModel.DOMDebuggerManager.instance().addXHRBreakpoint('', true);
      SourcesTestRunner.waitUntilPaused(paused);
      TestRunner.evaluateInPage('sendRequest(\'/foo\')');

      function paused(callFrames) {
        function resumed() {
          SDK.DOMDebuggerModel.DOMDebuggerManager.instance().toggleXHRBreakpoint('', false);
          SourcesTestRunner.waitUntilPaused(pausedAgain);
          TestRunner.evaluateInPage('sendRequest(\'/foo\')', next);
        }
        SourcesTestRunner.resumeExecution(resumed);
      }

      function pausedAgain(callFrames) {
        TestRunner.addResult('Fail, paused again after breakpoint was removed.');
        next();
      }
    }
  ]);
})();
