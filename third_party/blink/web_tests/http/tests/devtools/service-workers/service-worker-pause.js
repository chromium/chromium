// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that we can pause in service worker.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var scriptURL = 'http://127.0.0.1:8000/devtools/service-workers/resources/service-worker-debugger.js';
  var scope = 'http://127.0.0.1:8000/devtools/service-workers/resources/scope-debugger/';

  SourcesTestRunner.startDebuggerTest(start);

  function start() {
    SourcesTestRunner.waitUntilPaused(onPaused);
    ApplicationTestRunner.registerServiceWorker(scriptURL, scope);
  }

  async function onPaused(frames, reason, breakpointIds, async) {
    await SourcesTestRunner.captureStackTrace(frames, async);
    SourcesTestRunner.completeDebuggerTest();
  }
})();
