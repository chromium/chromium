// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.showPanel('elements');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(
    '../../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  TestRunner.addResult('Testing accessibility in the DOM breakpoints pane.');

  // Expand the DOM Breakpoints container
  const domBreakpointContainer = UI.panels.sources._sidebarPaneStack._expandableContainers.get('sources.domBreakpoints');
  await domBreakpointContainer._expand();

  TestRunner.addResult('Setting DOM breakpoints.');
  const rootElement = await ElementsTestRunner.nodeWithIdPromise('rootElement');
  TestRunner.domDebuggerModel.setDOMBreakpoint(
      rootElement, SDK.DOMDebuggerModel.DOMBreakpoint.Type.SubtreeModified);

  const hostElement = await ElementsTestRunner.nodeWithIdPromise('hostElement');
  const breakpoint = TestRunner.domDebuggerModel.setDOMBreakpoint(
      hostElement, SDK.DOMDebuggerModel.DOMBreakpoint.Type.NodeRemoved);
  TestRunner.domDebuggerModel.toggleDOMBreakpoint(breakpoint, false);

  const domBreakpointsPane =
    self.runtime.sharedInstance(BrowserDebugger.DOMBreakpointsSidebarPane);

  TestRunner.addResult(`DOM breakpoints container text content: ${domBreakpointContainer.contentElement.deepTextContent()}`);
  TestRunner.addResult(`DOM breakpoints pane text content: ${domBreakpointsPane.contentElement.deepTextContent()}`);

  TestRunner.addResult(
      'Running the axe-core linter on the DOM breakpoints pane.');

  //TODO(crbug.com/1004940): expected.txt file has 'label' exceptions
  await AxeCoreTestRunner.runValidation(domBreakpointContainer.element);
  TestRunner.completeTest();
})();
