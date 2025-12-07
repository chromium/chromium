// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Test that OOPIF processes and threads are recorded.\n`);

  await TestRunner.showPanel('timeline');

  await PerformanceTestRunner.startTimeline();
  await TestRunner.navigatePromise('resources/page.html');
  await PerformanceTestRunner.stopTimeline();

  const expectedProcesses = [
    {url: 'http://127.0.0.1:8000/devtools/resources/inspected-page.html', isOnMainFrame: true},
    {url: 'http://devtools.oopif.test:8000/devtools/oopif/resources/iframe.html?second', isOnMainFrame: false},
  ];

  const parsedTrace = PerformanceTestRunner.traceEngineParsedTrace();
  const processes = Array.from(parsedTrace.Renderer.processes.values());
  for (const {url, isOnMainFrame} of expectedProcesses) {
    const match = processes.find(p => p.url === url && p.isOnMainFrame === isOnMainFrame);
    if(match) {
      TestRunner.addResult(`MATCH FOUND: ${url} - isOnMainFrame: ${isOnMainFrame}`);
    } else {
      TestRunner.addResult(`ERROR: no match found for ${url} - isOnMainFrame: ${isOnMainFrame}`);
    }
  }

  TestRunner.completeTest();
})();
