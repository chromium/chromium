// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that cache data is correctly deleted by the inspector.\n`);
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
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache1', 'http://fake.request.com/1', 'OK'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache1', 'http://fake.request.com/2', 'Not Found'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request2.com/1', 'OK'))
        .then(ApplicationTestRunner.addCacheEntry.bind(this, 'testCache2', 'http://fake.request2.com/2', 'Not Found'))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.deleteCache.bind(this, 'testCache1'))
        .then(ApplicationTestRunner.deleteCacheFromInspector.bind(this, 'testCache2', undefined))
        .then(ApplicationTestRunner.dumpCacheTree)
        .then(ApplicationTestRunner.clearAllCaches)
        .then(TestRunner.completeTest)
        .catch(errorAndExit);
  }

  main();
})();
