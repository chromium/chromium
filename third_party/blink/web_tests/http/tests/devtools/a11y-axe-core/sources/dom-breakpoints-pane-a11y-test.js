// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as BrowserDebugger from 'devtools/panels/browser_debugger/browser_debugger.js';

(async function() {
  await TestRunner.showPanel('elements');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
    '../../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  TestRunner.addResult('Testing accessibility in the DOM breakpoints pane.');

  // Expand the DOM Breakpoints container
  const domBreakpointContainer = Sources.SourcesPanel.SourcesPanel.instance().sidebarPaneStack.expandableContainers.get('sources.dom-breakpoints');
  await domBreakpointContainer.expand();

  TestRunner.addResult('Setting DOM breakpoints.');
  const rootElement = await ElementsTestRunner.nodeWithIdPromise('rootElement');
  TestRunner.domDebuggerModel.setDOMBreakpoint(rootElement, Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);

  const hostElement = await ElementsTestRunner.nodeWithIdPromise('hostElement');
  const breakpoint =
      TestRunner.domDebuggerModel.setDOMBreakpoint(hostElement, Protocol.DOMDebugger.DOMBreakpointType.NodeRemoved);
  TestRunner.domDebuggerModel.toggleDOMBreakpoint(breakpoint, false);

  const domBreakpointsPane = BrowserDebugger.DOMBreakpointsSidebarPane.DOMBreakpointsSidebarPane.instance();

  TestRunner.addResult(`DOM breakpoints container text content: ${domBreakpointContainer.contentElement.deepTextContent()}`);
  TestRunner.addResult(
      `DOM breakpoints container ARIA descriptions: ${getDeepARIADescriptions(domBreakpointContainer.contentElement)}`);
  TestRunner.addResult(`DOM breakpoints pane text content: ${domBreakpointsPane.contentElement.deepTextContent()}`);
  TestRunner.addResult(
      `DOM breakpoints pane ARIA descriptions: ${getDeepARIADescriptions(domBreakpointsPane.contentElement)}`);

  TestRunner.addResult(
      'Running the axe-core linter on the DOM breakpoints pane.');

  await AxeCoreTestRunner.runValidation(domBreakpointContainer.element);
  TestRunner.completeTest();

  function getDeepARIADescriptions(root) {
    let node = root;
    const descriptions = [];
    while (node) {
      if (node.nodeType === node.ELEMENT_NODE && node.hasAttribute('aria-description')) {
        descriptions.push(node.getAttribute('aria-description'));
      }
      node = node.traverseNextNode(root);
    }
    return descriptions.join();
  }
})();
