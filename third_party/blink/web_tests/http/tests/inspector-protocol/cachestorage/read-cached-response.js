(async function(testRunner) {
  var {page, session, dp} = await testRunner.startURL(
      'resources/service-worker.html',
      `Tests reading cached response from the protocol.`);

  async function dumpResponse(cacheId, entry) {
    var {error, result} = await dp.CacheStorage.requestCachedResponse({cacheId, requestURL: entry ? entry.requestURL : null, requestHeaders: []});
    if (error) {
      testRunner.log(`Error: ${error.message} ${error.data || ""}`);
      return;
    }
    var header = entry.responseHeaders.find(header => header.name.toLowerCase() === 'content-type');
    testRunner.log(header ? header.value : '');
    testRunner.log("Type of body: " + (typeof result.response.body));
  }

  async function waitForServiceWorkerActivation() {
    do {
      var result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      var versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
  }

  var swActivatedPromise = waitForServiceWorkerActivation();

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();
  await swActivatedPromise;

  var {result} = await dp.CacheStorage.requestCacheNames({securityOrigin: "http://127.0.0.1:8000"});
  var cacheId = result.caches[0].cacheId;
  result = await dp.CacheStorage.requestEntries({cacheId, skipCount: 0, pageSize: 5});
  var entries = result.result.cacheDataEntries;
  entries.sort((a, b) => a.requestURL.localeCompare(b.requestURL));
  testRunner.log("Cached requests:");

  for (var entry of entries)
    await dumpResponse(cacheId, entry);

  testRunner.log('Trying without specifying all the arguments:')
  await dumpResponse(null, null);
  testRunner.log('Trying without specifying the request path:')
  await dumpResponse(cacheId, null);
  testRunner.log('Trying with non existant cache:')
  await dumpResponse("bogus", entries[0]);
  testRunner.log('Trying with non existant request path:')
  await dumpResponse(cacheId, {requestURL: "http://localhost:8080/bogus"});

  testRunner.completeTest()
});
