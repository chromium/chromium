// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that DOM debugger will not crash when editing DOM nodes from the Web Inspector. Chromium bug 249655\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <div id="rootElement" style="color: red">
      <div id="elementToRemove"></div>
      </div>
    `);

  SourcesTestRunner.runDebuggerTestSuite([
    function testRemoveNode(next) {
      TestRunner.addResult('Testing NodeRemoved DOM breakpoint.');
      ElementsTestRunner.nodeWithId('elementToRemove', step2);

      function step2(node) {
        TestRunner.domDebuggerModel.setDOMBreakpoint(node, Protocol.DOMDebugger.DOMBreakpointType.NodeRemoved);
        TestRunner.addResult('Set NodeRemoved DOM breakpoint.');
        node.removeNode(next);
      }
    },

    function testModifyAttribute(next) {
      TestRunner.addResult('Testing AttributeModified DOM breakpoint.');
      ElementsTestRunner.nodeWithId('rootElement', step2);

      function step2(node) {
        TestRunner.domDebuggerModel.setDOMBreakpoint(node, Protocol.DOMDebugger.DOMBreakpointType.AttributeModified);
        TestRunner.addResult('Set AttributeModified DOM breakpoint.');
        node.setAttribute('title', 'a title', next);
      }
    }
  ]);
})();
