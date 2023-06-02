// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';


(async function () {
  TestRunner.addResult(`Checks that breakpoint set inside a portalactivate event handler is hit on activation`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.showPanel('sources');

  await TestRunner.navigatePromise('resources/append-predecessor-host.html');

  await SourcesTestRunner.startDebuggerTestPromise();
  const sourceFrame = await SourcesTestRunner.showScriptSourcePromise('append-predecessor.html');
  await SourcesTestRunner.toggleBreakpoint(sourceFrame, 4);
  TestRunner.evaluateInPage(`setTimeout(() => document.querySelector('portal').activate());`);
  const callFrames = await SourcesTestRunner.waitUntilPausedPromise();
  await SourcesTestRunner.captureStackTrace(callFrames);
  SourcesTestRunner.completeDebuggerTest();
})();
