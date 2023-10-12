// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that cache data is correctly populated in the Inspector.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel);
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
        .then(ApplicationTestRunner.addCacheEntryWithBlobType.bind(this, 'testCache1', 'http://fake.request.com/T', 'image/png'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request.com/1', 'OK'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request.com/2?query=19940123', 'Not Found'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request.com/L/11', 'OK'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request.com/H/22', 'OK'))
        // Note: testCache1, testCache2 both have entries from '.../T' but these two entries have different blob types intentionally
        .then(ApplicationTestRunner.addCacheEntryWithBlobType.bind(this, 'testCache2', 'http://fake.request.com/T', 'text/javascript'))
        .then(ApplicationTestRunner.addCacheEntryWithNoCorsRequest.bind(this, 'testCache3', TestRunner.url('../resources/image.png')))
        .then(ApplicationTestRunner.addCacheEntryWithVarsResponse.bind(this, 'testCache3', 'http://fake.request.com/vars'))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.dumpCacheTree.bind(this, '2'))
        .then(ApplicationTestRunner.dumpCacheTree.bind(this, 'image'))
        .then(ApplicationTestRunner.dumpCachedEntryContent.bind(this, 'testCache2', 'http://fake.request.com/1', false))
        .then(ApplicationTestRunner.dumpCachedEntryContent.bind(this, 'testCache3', 'http://fake.request.com/vars', true))
        .then(ApplicationTestRunner.dumpCachedEntryContent.bind(this, 'testCache3', 'http://fake.request.com/vars', false))
        .then(ApplicationTestRunner.clearAllCaches)
        .then(TestRunner.completeTest)
        .catch(errorAndExit);
  }

  main();
})();
