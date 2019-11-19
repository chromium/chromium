(async function(testRunner) {
  const {page, session, dp} = await testRunner.startURL(
      'resources/service-worker.html',
      `Tests fetch cached response entries from the protocol.`);

  async function waitForServiceWorkerActivation() {
    let versions;
    do {
      const result = await dp.ServiceWorker.onceWorkerVersionUpdated();
      versions = result.params.versions;
    } while (!versions.length || versions[0].status !== "activated");
  }

  async function dumpEntries(skipCount, pageSize) {
    const result = await dp.CacheStorage.requestEntries({cacheId, skipCount, pageSize});
    const entries = result.result.cacheDataEntries;
    entries.sort((a, b) => a.requestURL.localeCompare(b.requestURL));
    testRunner.log(`Cached requests (${skipCount || '-'}/${pageSize || '-'}): `);
    for (let entry of entries)
      testRunner.log('   ' + entry.requestURL);
    testRunner.log('');
  }

  const swActivatedPromise = waitForServiceWorkerActivation();

  await dp.Runtime.enable();
  await dp.ServiceWorker.enable();
  await swActivatedPromise;

  const {result} = await dp.CacheStorage.requestCacheNames({securityOrigin: "http://127.0.0.1:8000"});
  const cacheId = result.caches[0].cacheId;

  testRunner.log(`Expecting skipCount and pageSize to slice the results`);
  await dumpEntries(0, 1);
  await dumpEntries(1, 1);
  await dumpEntries(0, 2);

  testRunner.log(`Expecting skipCount to default to 0`);
  await dumpEntries(undefined, 1);

  testRunner.log(`Expecting pageSize to default to all`);
  await dumpEntries(0, undefined)

  testRunner.completeTest()
});
