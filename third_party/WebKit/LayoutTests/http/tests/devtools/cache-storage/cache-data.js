// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that cache data is correctly populated in the Inspector.\n`);
  await TestRunner.loadModule('application_test_runner');
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel);
  cacheStorageModel.enable();

  function errorAndExit(error) {
    if (error)
      TestRunner.addResult(error);
    TestRunner.completeTest();
  }

  function main() {
    ApplicationTestRunner.clearAllCaches()
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache1'))
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache2'))
        .then(ApplicationTestRunner.createCache.bind(this, 'testCache3'))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache1', 'http://fake.request.com/1', 'OK'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache1', 'http://fake.request.com/2', 'Not Found'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request2.com/1', 'OK'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request2.com/2', 'Not Found'))
        .then(ApplicationTestRunner.addCacheEntryWithNoCorsRequest.bind(this, 'testCache3', TestRunner.url('../resources/image.png')))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.clearAllCaches)
        .then(TestRunner.completeTest)
        .catch(errorAndExit);
  }

  ApplicationTestRunner.waitForCacheRefresh(main);
})();
