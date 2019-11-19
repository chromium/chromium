// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(async function() {
  TestRunner.addResult(
      'Tests accessibility in DOM breakpoints using the axe-core linter.');

  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.loadModule('elements_test_runner');
  await TestRunner.showPanel('elements');
  UI.panels.elements.sidebarPaneView.tabbedPane().selectTab('elements.domBreakpoints', true);

  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  const rootElement = await ElementsTestRunner.nodeWithIdPromise('rootElement');

  // Add Dom breakpoints and then test
  TestRunner.domDebuggerModel.setDOMBreakpoint(
    rootElement, SDK.DOMDebuggerModel.DOMBreakpoint.Type.SubtreeModified);
  TestRunner.domDebuggerModel.setDOMBreakpoint(
    rootElement, SDK.DOMDebuggerModel.DOMBreakpoint.Type.AttributeModified);
  TestRunner.addResult(
      'Test DOM breakpoint container with multiple breakpoints.');

  const widget = UI.panels.elements.sidebarPaneView.tabbedPane().visibleView;
  await AxeCoreTestRunner.runValidation(widget.element);
  TestRunner.completeTest();
})();
