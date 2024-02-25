// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Elements from 'devtools/panels/elements/elements.js';
(async function() {
  TestRunner.addResult(
      'Tests accessibility in DOM breakpoints using the axe-core linter.');

  await TestRunner.showPanel('elements');
  Elements.ElementsPanel.ElementsPanel.instance().sidebarPaneView.tabbedPane().selectTab('elements.domBreakpoints', true);

  await TestRunner.navigatePromise(
      '../../sources/debugger-breakpoints/resources/dom-breakpoints.html');

  const rootElement = await ElementsTestRunner.nodeWithIdPromise('rootElement');

  // Add Dom breakpoints and then test
  TestRunner.domDebuggerModel.setDOMBreakpoint(rootElement, Protocol.DOMDebugger.DOMBreakpointType.SubtreeModified);
  TestRunner.domDebuggerModel.setDOMBreakpoint(rootElement, Protocol.DOMDebugger.DOMBreakpointType.AttributeModified);
  TestRunner.addResult(
      'Test DOM breakpoint container with multiple breakpoints.');

  const widget = Elements.ElementsPanel.ElementsPanel.instance().sidebarPaneView.tabbedPane().visibleView;
  await AxeCoreTestRunner.runValidation(widget.element);
  TestRunner.completeTest();
})();
