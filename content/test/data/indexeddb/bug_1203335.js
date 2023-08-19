// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The purpose of this test is to asserts that errors are returned appropriately
// when commit() fails. See crbug.com/1203335. This test is a near-duplicate of
// quota_test.js, with the difference being that this test commit()s
// transactions.

function test() {
  if (navigator.storage) {
    window.jsTestIsAsync = true;
    navigator.storage.estimate()
        .then(initUsageCallback)
        .catch(unexpectedErrorCallback);
  } else
    debug('This test requires navigator.storage.');
}

function initUsageCallback(result) {
  origReturnedUsage = returnedUsage = result.usage;
  origReturnedQuota = returnedQuota = result.quota;
  debug('original quota is ' + displaySize(origReturnedQuota));
  debug('original usage is ' + displaySize(origReturnedUsage));

  indexedDBTest(prepareDatabase, initQuotaEnforcing);
}

function prepareDatabase() {
  db = event.target.result;
  objectStore = db.createObjectStore('test123');
}

function displaySize(bytes) {
  var k = bytes / 1024;
  var m = k / 1024;
  return bytes + ' (' + k + 'k) (' + m + 'm)';
}

function initQuotaEnforcing() {
  var availableSpace = origReturnedQuota - origReturnedUsage;
  var kMaxMbPerWrite = 5;
  var kMinWrites = 5;
  var len = Math.min(
      kMaxMbPerWrite * 1024 * 1024, Math.floor(availableSpace / kMinWrites));
  maxExpectedWrites = Math.floor(availableSpace / len) + 1;
  debug('Chunk size: ' + displaySize(len));
  debug(
      'Expecting at most ' + maxExpectedWrites + ' writes, but we could ' +
      'have more if snappy is used or LevelDB is about to compact.');
  // The data needs to be randomized to avoid compression.
  data = '';
  for (let i = 0; i < 1 + len / 8; i++) {
    data += Math.random().toString(36).slice(2, 10);
  }
  dataLength = data.length;
  dataAdded = 0;
  successfulWrites = 0;
  startNewTransaction();
}

function startNewTransaction() {
  if (dataAdded > origReturnedQuota) {
    fail('dataAdded > quota ' + dataAdded + ' > ' + origReturnedQuota);
    return;
  }
  debug('');
  debug('Starting new transaction.');

  var trans = db.transaction(['test123'], 'readwrite');
  trans.onabort = onAbort;
  trans.oncomplete = getQuotaAndUsage;
  var store = trans.objectStore('test123');
  request = store.put({x: data}, dataAdded);
  request.onerror = logError;
  // Unlike quota_test.js, commit() the transaction.
  trans.commit();
}

function getQuotaAndUsage() {
  successfulWrites++;
  if (successfulWrites > maxExpectedWrites) {
    debug(
        'Weird: too many writes. There were ' + successfulWrites +
        ' but we only expected ' + maxExpectedWrites);
  }
  navigator.webkitTemporaryStorage.queryUsageAndQuota(
      usageCallback, unexpectedErrorCallback);
}

function usageCallback(usage, quota) {
  debug('Transaction finished.');
  dataAdded += dataLength;
  debug('We\'ve added ' + displaySize(dataAdded));
  returnedUsage = usage;
  returnedQuota = quota;
  debug('Allotted quota is ' + displaySize(returnedQuota));
  debug('LevelDB usage is ' + displaySize(returnedUsage));
  startNewTransaction();
}

function onAbort() {
  shouldBeEqualToString('event.target.error.name', 'QuotaExceededError');
  done('Transaction aborted. Data added: ' + displaySize(dataAdded));
  debug('There were ' + successfulWrites + ' successful writes');
}

function logError() {
  debug(
      'Error function called: (' + event.target.error.name + ') ' +
      event.target.error.message);
  event.preventDefault();
}
