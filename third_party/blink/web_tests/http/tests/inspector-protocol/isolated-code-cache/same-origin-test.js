// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests V8 code cache for javascript resources\n`);
  const CodeCacheHelper =
      await testRunner.loadScript('resources/code-cache-helper.js');
  const helper = new CodeCacheHelper(testRunner, session,
                                     ['v8.compile', 'v8.produceCache']);

  // Clear browser cache to avoid any existing entries for the fetched
  // scripts in the cache.
  await dp.Network.clearBrowserCache();

  // There are two scripts:
  // [A] http://127.0.0.1:8000/devtools/resources/v8-cache-script.cgi
  // [B] http://localhost:8000/devtools/resources/v8-cache-script.cgi

  // An iframe that loads [A].
  // The script is executed as a parser-inserted script,
  // to keep the ScriptResource on the MemoryCache.
  // ScriptResources for dynamically-inserted <script>s can be
  // garbage-collected and thus removed from MemoryCache after its execution.
  const scope = 'resources/same-origin-script.html';
  // An iframe that loads [B].
  const scopeCrossOrigin = 'resources/cross-origin-script.html';

  testRunner.log('--- Trace events related to code caches ------');
  await helper.startTracing();

  // Load [A] thrice. With the current V8 heuristics (defined in
  // third_party/blink/renderer/bindings/core/v8/v8_code_cache.cc) we produce
  // cache on second fetch and consume it in the third fetch. This tests these
  // heuristics.
  // Note that addIframe() waits for iframe's load event, which waits for the
  // <script> loading.
  await helper.expectationComment('Load [A] 1st time. Produce timestamp. -->');
  await helper.addIframe(scope);

  await helper.expectationComment('Load [A] 2nd time. Produce code cache. -->');
  await helper.addIframe(scope);
  await helper.waitUntilIdle();

  await helper.expectationComment('Load [A] 3rd time. Consume code cache. -->');
  await helper.addIframe(scope);

  await helper.expectationComment(
      'Load [B]. Should not use the cached code. -->');
  await helper.addIframe(scopeCrossOrigin);

  await helper.expectationComment('Load [A] again from MemoryCache. ' +
                                  'Should use the cached code. -->');
  await helper.addIframe(scope);

  await helper.expectationComment('Clear [A] from MemoryCache. -->');
  // Blink evicts previous Resource when a new request to the same URL but with
  // different resource type is started.  We fetch() to the URL of [A], and thus
  // evicts the previous ScriptResource of [A].
  await session.evaluateAsync(
      `fetch('/devtools/resources/v8-cache-script.cgi').then(() => {})`);

  await helper.expectationComment('Load [A] from Disk Cache. -->');
  // As we cleared [A] from MemoryCache, this doesn't hit MemoryCache, but still
  // hits Disk Cache.
  await helper.addIframe(scope);

  await helper.stopTracingAndPrint();
  testRunner.log('-----------------------------------------------');
  testRunner.completeTest();
})
