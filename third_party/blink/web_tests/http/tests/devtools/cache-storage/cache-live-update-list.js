// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that the cache storage list live updates.\n`);
    // Note: every test that uses a storage API must manually clean-up state from previous tests.
  await ApplicationTestRunner.resetState();

  await TestRunner.showPanel('resources');

  var cacheStorageModel = TestRunner.mainTarget.model(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel);
  cacheStorageModel.enable();
  cacheStorageModel.setThrottlerSchedulesAsSoonAsPossibleForTest();

  await ApplicationTestRunner.clearAllCaches();
  await ApplicationTestRunner.dumpCacheTree();

  var promise = TestRunner.addSnifferPromise(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel.prototype, 'cacheAdded');
  ApplicationTestRunner.createCache('testCache1');
  await promise;
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();

  promise = TestRunner.addSnifferPromise(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel.prototype, 'cacheAdded');
  ApplicationTestRunner.createCache('testCache2');
  await promise;
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();

  promise = TestRunner.addSnifferPromise(SDK.ServiceWorkerCacheModel.ServiceWorkerCacheModel.prototype, 'cacheRemoved');
  ApplicationTestRunner.deleteCache('testCache1');
  await promise;
  await ApplicationTestRunner.dumpCacheTreeNoRefresh();

  await ApplicationTestRunner.clearAllCaches();
  TestRunner.completeTest();
})();
