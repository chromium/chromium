// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests V8 code cache for resources revalidated with 304.\n');
  // The main purpose of the test is to demonstrate that the code cache
  // produced on the 2nd load is still consumed after the subresource is
  // evicted from the MemoryCache and revalidated with a 304 response. The code
  // cache is validated against the original response time, which does not
  // change on revalidation (https://crbug.com/373703283).
  const CodeCacheHelper =
      await testRunner.loadScript('resources/code-cache-helper.js');
  const helper = new CodeCacheHelper(testRunner, session,
                                     ['v8.compile', 'v8.produceCache']);

  // Clear browser cache to avoid any existing entries for the fetched scripts
  // in the cache.
  await dp.Network.clearBrowserCache();

  // The script is executed as a parser-inserted script, to keep the
  // ScriptResource in blink::MemoryCache and reduce flake. ScriptResource for
  // a dynamically-inserted <script> can be garbage-collected and thus removed
  // from MemoryCache after its execution.
  const resource = 'resources/revalidated-script.html'

  testRunner.log('--- Begin trace events related to code cache. ------');
  await helper.startTracing();

  // Load the resource a few times. With the current V8 heuristics
  // we produce cache on second fetch and consume it in the
  // third fetch. Note that addIframe() waits for
  // iframe's load event, which waits for the <script> to load.
  await helper.expectationComment('1st load. Produce timestamp. -->');
  await helper.addIframe(resource);

  await helper.expectationComment('2nd load. Produce code cache. -->');
  await helper.addIframe(resource);
  await helper.waitUntilIdle();

  await helper.expectationComment('3rd load. Consume code cache. -->');
  await helper.addIframe(resource);

  await helper.expectationComment(
      'Clear the resource from the MemoryCache. -->');
  // Blink evicts previous Resource when a new request to the same URL but with
  // different resource type is started. We fetch() the URL, and thus
  // evict the previous ScriptResource of the URL.
  await session.evaluateAsync(
      `fetch('/devtools/resources/v8-cache-revalidated-script.cgi')` +
      `.then(() => {})`);

  await helper.expectationComment(
      'Load the resource with revalidation. ' +
      'The code cache is consumed because the 304 response does not ' +
      'change the original response time. -->');
  await helper.addIframe(resource);

  await helper.stopTracingAndPrint();
  testRunner.log('----- End trace events related to code cache. ------');
  testRunner.completeTest();
})
