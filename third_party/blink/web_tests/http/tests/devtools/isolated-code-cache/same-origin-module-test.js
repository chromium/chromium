// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as TimelineModel from 'devtools/models/timeline_model/timeline_model.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

function waitUntilIdle() {
  return new Promise(resolve=>window.requestIdleCallback(resolve));
}

(async function() {
  TestRunner.addResult(`Tests V8 code cache for javascript resources\n`);
  await TestRunner.showPanel('timeline');

  // Clear browser cache to avoid any existing entries for the fetched
  // scripts in the cache.
  SDK.NetworkManager.MultitargetNetworkManager.instance().clearBrowserCache();

  // There are two scripts:
  // [A] http://127.0.0.1:8000/devtools/resources/v8-cache-script.cgi
  // [B] http://localhost:8000/devtools/resources/v8-cache-script.cgi

  // An iframe that loads [A].
  // The script is executed as a parser-inserted script,
  // to keep the ScriptResource on the MemoryCache.
  // ScriptResources for dynamically-inserted <script>s can be
  // garbage-collected and thus removed from MemoryCache after its execution.
  const scope = 'resources/same-origin-module-script.html';
  // An iframe that loads [B].
  const scopeCrossOrigin = 'resources/cross-origin-module-script.html';

  TestRunner.addResult('--- Trace events related to code caches ------');
  await PerformanceTestRunner.startTimeline();

  // Load [A] thrice. With the current V8 heuristics (defined
  // in third_party/blink/renderer/bindings/core/v8/v8_code_cache.cc) we produce
  // cache on second fetch and consume it in the third fetch. This tests these
  // heuristics.
  // Note that addIframe() waits for iframe's load event, which waits for the
  // <script> loading.
  await TestRunner.addIframe(scope);
  await TestRunner.addIframe(scope);
  await waitUntilIdle();
  await TestRunner.addIframe(scope);

  // Load [B]. Should not use the cached code.
  await TestRunner.addIframe(scopeCrossOrigin);

  // Load [A] again from MemoryCache. Should use cached code.
  await TestRunner.addIframe(scope);

  // Clear [A] from MemoryCache. Blink evicts previous Resource when a
  // new request to the same URL but with different resource type is started.
  // We fetch() to the URL of [A], and thus evicts the previous ScriptResource
  // of [A].
  await TestRunner.evaluateInPageAsync(
      `fetch('/devtools/resources/v8-cache-script.cgi')`);

  // Load [A] from Disk Cache. As we cleared [A] from MemoryCache, this
  // doesn't hit MemoryCache, but still hits Disk Cache.
  await TestRunner.addIframe(scope);

  await PerformanceTestRunner.stopTimeline();
  await PerformanceTestRunner.printTimelineRecordsWithDetails(
      TimelineModel.TimelineModel.RecordType.CompileModule,
      TimelineModel.TimelineModel.RecordType.CacheModule);

  TestRunner.addResult('-----------------------------------------------');
  TestRunner.completeTest();
})();
