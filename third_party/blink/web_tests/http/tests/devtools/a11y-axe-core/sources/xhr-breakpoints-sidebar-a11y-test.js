// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function () {
  await TestRunner.loadModule('axe_core_test_runner');
  await TestRunner.showPanel('sources');

  // this rule causes false negatives due to axe not handling the shadow DOM properly
  const noRequiredParent = {'aria-required-parent': {enabled: false}};

  await UI.viewManager.showView('sources.xhrBreakpoints');
  TestRunner.addResult('Adding XHR breakpoint.');
  const xhrBreakpointsPane = runtime.sharedInstance(BrowserDebugger.XHRBreakpointsSidebarPane);
  xhrBreakpointsPane._setBreakpoint('test xhr breakpoint', true);
  TestRunner.addResult('Running axe on the XHR breakpoints pane.');

  await AxeCoreTestRunner.runValidation(xhrBreakpointsPane.contentElement, noRequiredParent);

  TestRunner.completeTest();
})();
