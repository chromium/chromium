// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

(async function() {
  await TestRunner.showPanel('sources');
  await TestRunner.loadLegacyModule('browser_debugger');

  // this rule causes false negatives due to axe not handling the shadow DOM properly
  const noRequiredParent = {'aria-required-parent': {enabled: false}};

  await UI.viewManager.showView('sources.xhrBreakpoints');
  TestRunner.addResult('Adding XHR breakpoint.');
  const xhrBreakpointsPane = BrowserDebugger.XHRBreakpointsSidebarPane.instance();
  xhrBreakpointsPane.setBreakpoint('test xhr breakpoint', true);
  TestRunner.addResult('Running axe on the XHR breakpoints pane.');

  await AxeCoreTestRunner.runValidation(xhrBreakpointsPane.contentElement, noRequiredParent);

  TestRunner.completeTest();
})();
