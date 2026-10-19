// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Tests that CacheStorage.requestCacheNames, CacheStorage.requestEntries, and CacheStorage.requestCachedResponse report cache entries, path filtering, and Vary header matching.`);

  await session.evaluate(`
    function createCache(cacheName) {
      return caches.open(cacheName);
    }

    function addCacheEntryImpl(cacheName, requestUrl, responseText, blobType) {
      return caches.open(cacheName).then(function(cache) {
        let request = new Request(requestUrl);
        let myBlob = new Blob(['Y'], { 'type': blobType });
        let init = { 'status': 200, 'statusText': responseText };
        let response = new Response(myBlob, init);
        return cache.put(request, response);
      });
    }

    function addCacheEntryWithVarsResponse(cacheName, requestUrl) {
      return caches.open(cacheName).then(function(cache) {
        let request = new Request(requestUrl, { headers: { 'Accept': '*/*' } });
        let myBlob = new Blob(['Z'], { 'type': 'text/plain' });
        let init = { 'headers': { 'Vary': 'Accept' } };
        let response = new Response(myBlob, init);
        return cache.put(request, response);
      });
    }

    function addCacheEntryWithNoCorsRequest(cacheName, requestUrl) {
      return caches.open(cacheName).then(async function(cache) {
        let request = new Request(requestUrl, {mode: 'no-cors'});
        return cache.put(request, await fetch(request));
      });
    }

    function clearAllCaches() {
      return caches.keys().then(
          keys => Promise.all(keys.map(key => caches.delete(key))));
    }
  `);

  function headerValue(headers, name) {
    const header =
        headers.find(h => h.name.toLowerCase() === name.toLowerCase());
    return header ? header.value : undefined;
  }

  async function dumpCacheTree(pathFilter) {
    if (!pathFilter) {
      testRunner.log('Dumping CacheStorage tree:');
    } else {
      testRunner.log(`Dumping CacheStorage tree with URL path filter string "${
          pathFilter}"`);
    }
    const securityOrigin = await session.evaluate('location.origin');
    const {caches} =
        (await dp.CacheStorage.requestCacheNames({securityOrigin})).result;
    if (!caches.length) {
      testRunner.log('    (empty)');
      return;
    }
    for (const cache of caches) {
      testRunner.log(
          `    cache: ${cache.cacheName} - ${cache.securityOrigin}/`);
      const {cacheDataEntries: allEntries} =
          (await dp.CacheStorage.requestEntries({
            cacheId: cache.cacheId,
            skipCount: 0,
            pageSize: 50,
            pathFilter: '',
          })).result;
      if (!allEntries.length) {
        testRunner.log('        (cache empty)');
        continue;
      }
      const {cacheDataEntries, returnCount} = pathFilter ?
          (await dp.CacheStorage.requestEntries({
            cacheId: cache.cacheId,
            skipCount: 0,
            pageSize: 50,
            pathFilter,
          })).result :
          {cacheDataEntries: allEntries, returnCount: allEntries.length};
      if (!cacheDataEntries.length) {
        testRunner.log('        (no matching entries)');
        continue;
      }
      cacheDataEntries.forEach((entry, index) => {
        const parsedUrl = new URL(entry.requestURL);
        const pathAndQuery = parsedUrl.pathname + parsedUrl.search;
        const contentType = headerValue(entry.responseHeaders, 'content-type');
        const contentLength =
            headerValue(entry.responseHeaders, 'content-length') ?? '0';
        const vary = headerValue(entry.responseHeaders, 'vary');
        const cols = [
          index,
          pathAndQuery,
          entry.responseType,
          contentType,
          contentLength,
        ];
        if (vary) {
          cols.push(vary);
        }
        testRunner.log('        ' + cols.join(', '));
      });
      testRunner.log('        totalCount: ' + returnCount);
    }
  }

  async function dumpCachedEntryContent(cacheName, requestUrl, withHeader) {
    testRunner.log(
        `Dumping ${cacheName}'s entry with request URL: ${requestUrl}`);
    const securityOrigin = await session.evaluate('location.origin');
    const {caches} =
        (await dp.CacheStorage.requestCacheNames({securityOrigin})).result;
    const cache = caches.find(c => c.cacheName === cacheName);
    const {cacheDataEntries} = (await dp.CacheStorage.requestEntries({
                                 cacheId: cache.cacheId,
                                 skipCount: 0,
                                 pageSize: 50,
                                 pathFilter: '',
                               })).result;
    const entry = cacheDataEntries.find(e => e.requestURL === requestUrl);
    if (entry.requestHeaders.length) {
      testRunner.log(
          `    the original request has headers; query with headers? ${
              withHeader}`);
    }
    const response = await dp.CacheStorage.requestCachedResponse({
      cacheId: cache.cacheId,
      requestURL: requestUrl,
      requestHeaders: withHeader ? entry.requestHeaders : [],
    });
    const text = response.result?.response?.body ?
        atob(response.result.response.body) :
        '';
    testRunner.log('        ' + (text || '(nothing to preview)'));
  }

  await session.evaluateAsync('clearAllCaches()');
  await dumpCacheTree();
  await session.evaluateAsync(`createCache('testCache1')`);
  await session.evaluateAsync(`createCache('testCache2')`);
  await session.evaluateAsync(`createCache('testCache3')`);
  await dumpCacheTree();

  await session.evaluateAsync(
      `addCacheEntryImpl('testCache1', 'http://fake.request.com/1', 'OK', 'text/plain')`);
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache1', 'http://fake.request.com/2', 'Not Found', 'text/plain')`);
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache1', 'http://fake.request.com/T', 'OK', 'image/png')`);
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache2', 'http://fake.request.com/1', 'OK', 'text/plain')`);
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache2', 'http://fake.request.com/2?query=19940123', 'Not Found', 'text/plain')`);
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache2', 'http://fake.request.com/L/11', 'OK', 'text/plain')`);
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache2', 'http://fake.request.com/H/22', 'OK', 'text/plain')`);
  // Note: testCache1, testCache2 both have entries from '.../T' but these two
  // entries have different blob types intentionally.
  await session.evaluateAsync(
      `addCacheEntryImpl('testCache2', 'http://fake.request.com/T', 'OK', 'text/javascript')`);
  await session.evaluateAsync(`addCacheEntryWithNoCorsRequest('testCache3', '${
      testRunner.url('../resources/image.png')}')`);
  await session.evaluateAsync(
      `addCacheEntryWithVarsResponse('testCache3', 'http://fake.request.com/vars')`);

  await dumpCacheTree();
  await dumpCacheTree('2');
  await dumpCacheTree('image');
  await dumpCachedEntryContent('testCache2', 'http://fake.request.com/1',
                               false);
  await dumpCachedEntryContent('testCache3', 'http://fake.request.com/vars',
                               true);
  await dumpCachedEntryContent('testCache3', 'http://fake.request.com/vars',
                               false);
  await session.evaluateAsync('clearAllCaches()');
  testRunner.completeTest();
});
