// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as TimelineModel from 'devtools/models/timeline_model/timeline_model.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

function waitUntilIdle() {
  return new Promise(resolve=>{
    window.requestIdleCallback(()=>resolve());
  });
}

(async function() {
  TestRunner.addResult('Tests V8 code cache for resources revalidated with 304.\n');
  // The main purpose of the test is to demonstrate that after producing the
  // code cache on the 2nd load, it gets cleared on disk by subsequent loads of
  // the subresource, even if the response instructs to reuse the old resource.
  await TestRunner.showPanel('timeline');

  // Clear browser cache to avoid any existing entries for the fetched scripts
  // in the cache.
  SDK.NetworkManager.MultitargetNetworkManager.instance().clearBrowserCache();

  // The script is executed as a parser-inserted script, to keep the
  // ScriptResource in blink::MemoryCache and reduce flake. ScriptResource for
  // a dynamically-inserted <script> can be garbage-collected and thus removed
  // from MemoryCache after its execution.
  const resource = 'resources/revalidated-script.html'

  TestRunner.addResult('--- Begin trace events related to code cache. ------');
  await PerformanceTestRunner.startTimeline();

  async function stopAndPrintTimeline() {
    await PerformanceTestRunner.stopTimeline();
    await PerformanceTestRunner.printTimelineRecordsWithDetails(
        TimelineModel.TimelineModel.RecordType.CompileScript,
        TimelineModel.TimelineModel.RecordType.CacheScript);
  }

  async function expectationComment(msg) {
    await stopAndPrintTimeline();
    TestRunner.addResult(msg);
    await PerformanceTestRunner.startTimeline();
  }

  // Load the resource a few times. With the current V8 heuristics
  // we produce cache on second fetch and consume it in the
  // third fetch. Note that addIframe() waits for
  // iframe's load event, which waits for the <script> to load.
  await expectationComment('1st load. Produce timestamp. -->');
  await TestRunner.addIframe(resource);

  await expectationComment('2nd load. Produce code cache. -->');
  await TestRunner.addIframe(resource);
  await waitUntilIdle();

  await expectationComment('3rd load. Consume code cache. -->');
  await TestRunner.addIframe(resource);

  await expectationComment('Clear the resource from the MemoryCache. -->');
  // Blink evicts previous Resource when a new request to the same URL but with
  // different resource type is started. We fetch() the URL, and thus
  // evict the previous ScriptResource of the URL.
  await TestRunner.evaluateInPageAsync(
      `fetch('/devtools/resources/v8-cache-revalidated-script.cgi')`);

  await expectationComment('Load the resource with revalidation. ' +
      'The code cache got dropped because its timestamp did not ' +
      'match the one of the request. -->');
  // The timestamp is written in the next step because there was no cached code.
  await TestRunner.addIframe(resource);

  await stopAndPrintTimeline();
  TestRunner.addResult('----- End trace events related to code cache. ------');
  TestRunner.completeTest();
})();
