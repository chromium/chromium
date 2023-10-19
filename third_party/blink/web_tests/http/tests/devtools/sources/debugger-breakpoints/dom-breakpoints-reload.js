// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as BrowserDebugger from 'devtools/panels/browser_debugger/browser_debugger.js';

(async function() {
  TestRunner.addResult(`Tests DOM breakpoints.`);
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('elements');
  await TestRunner.navigatePromise('resources/dom-breakpoints.html');

  var pane = BrowserDebugger.DOMBreakpointsSidebarPane.DOMBreakpointsSidebarPane.instance();
  var rootElement;
  var outerElement;
  var authorShadowRoot;
  SourcesTestRunner.runDebuggerTestSuite([
    function testReload(next) {
      TestRunner.addResult(
          'Test that DOM breakpoints are persisted between page reloads.');
      ElementsTestRunner.nodeWithId('rootElement', step2);

      function step2(node) {
        TestRunner.domDebuggerModel.setDOMBreakpoint(
            node, Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
        TestRunner.addResult(
            'Set \'Subtree Modified\' DOM breakpoint on rootElement.');
        TestRunner.reloadPage(step3);
      }

      function step3() {
        ElementsTestRunner.expandElementsTree(step4);
      }

      function step4() {
        TestRunner.evaluateInPageWithTimeout(
            'appendElement(\'rootElement\', \'childElement\')');
        TestRunner.addResult('Append childElement to rootElement.');
        SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
      }
    },

    function testInsertChildIntoAuthorShadowTree(next) {
      ElementsTestRunner.shadowRootByHostId('hostElement', callback);

      function callback(node) {
        authorShadowRoot = node;
        TestRunner.addResult(
            'Test that \'Subtree Modified\' breakpoint on author shadow root is hit when appending a child.');
        TestRunner.domDebuggerModel.setDOMBreakpoint(
            authorShadowRoot,
            Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
        TestRunner.addResult(
            'Set \'Subtree Modified\' DOM breakpoint on author shadow root.');
        TestRunner.evaluateInPageWithTimeout(
            'appendElementToOpenShadowRoot(\'childElement\')');
        TestRunner.addResult('Append childElement to author shadow root.');
        SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
      }
    },

    function testReloadWithShadowElementBreakpoint(next) {
      ElementsTestRunner.nodeWithId('outerElement', step1);

      function step1(node) {
        outerElement = node;

        TestRunner.addResult(
            'Test that shadow DOM breakpoints are persisted between page reloads.');
        TestRunner.domDebuggerModel.setDOMBreakpoint(
            outerElement,
            Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
        TestRunner.addResult(
            'Set \'Subtree Modified\' DOM breakpoint on outerElement.');
        TestRunner.reloadPage(step2);
      }

      function step2() {
        ElementsTestRunner.expandElementsTree(step3);
      }

      function step3() {
        TestRunner.evaluateInPageWithTimeout(
            'appendElementToAuthorShadowTree(\'outerElement\', \'childElement\')');
        TestRunner.addResult('Append childElement to outerElement.');
        SourcesTestRunner.waitUntilPausedAndDumpStackAndResume(next);
      }
    }

  ]);
})();
