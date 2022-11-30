// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript test code for verifying proper committing
 * behaviour for transactions that are blocked when a tab is force
 * closed.
 */

function test() {
  if (document.location.hash === '#tab1') {
    setUpBlockedTransactions();
  } else if (document.location.hash === '#tab2') {
    checkForCommit();
  } else {
    result('fail - unexpected hash');
  }
}

function upgradeCallback() {
  db = event.target.result;
  deleteAllObjectStores(db);
  db.createObjectStore('store');
}

// We register four transactions, all on the same object store and all readwrite
// so as to incur blocking, and verify the expected results after a crash.
async function setUpBlockedTransactions() {
  const db = await
      promiseDeleteThenOpenDb('blocked-explicit-commit', upgradeCallback);

  // An auto-committed put that we expect to be committed.
  db.transaction('store', 'readwrite').objectStore('store')
      .put('auto', 'auto-key');

  // A transaction with a put request on it that we keep alive with a request
  // loop and which we expect to never commit.
  const blockingTransaction = db.transaction('store', 'readwrite');
  const blockingRequest = blockingTransaction.objectStore('store')
      .put('blocking', 'blocking-key');
  blockingRequest.onsuccess = () => { result('transactions registered'); };
  keepAlive(blockingTransaction, 'store');

  // A transaction with a put request on it that is blocked by the previous
  // transaction. We call an explicit commit on this transaction and so expect
  // its data to be committed after tab1 crashes and the blocking transaction is
  // aborted.
  const commitTransaction = db.transaction('store', 'readwrite');
  commitTransaction.objectStore('store').put('explicit', 'explicit-key');
  commitTransaction.commit();

  // A transaction with a put request on it that is blocked by the explicit
  // commit transaction. It is expected to be aborted and thus never commit
  // because it was not explicitly committed.
  db.transaction('store', 'readwrite').objectStore('store')
      .put('blocked', 'blocked-key');
}

async function checkForCommit() {
  const db = await promiseOpenDb('blocked-explicit-commit');
  const transaction = db.transaction('store', 'readonly');
  const objectStore = transaction.objectStore('store');

  const autoRequest = objectStore.get('auto-key');
  const blockingRequest = objectStore.get('blocking-key');
  const explicitRequest = objectStore.get('explicit-key');
  const blockedRequest = objectStore.get('blocked-key');

  for (const request of [autoRequest, blockingRequest, explicitRequest,
      blockedRequest]) {
    request.onerror = unexpectedErrorCallback;
    request.onblocked = unexpectedBlockedCallback;
  }

  transaction.oncomplete = () => {
    if (autoRequest.result == 'auto' && blockingRequest.result == undefined
        && explicitRequest.result == 'explicit'
        && blockedRequest.result == undefined) {
      result('transactions aborted and committed as expected');
    } else {
      result('fail - transactions did not abort and commit as expected');
    }
  };
}
