// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function finalTransactionCompleted()
{
  debug('The final transaction completed.');
  done();
}

function finalTransactionAborted()
{
  fail('The final transaction should not abort.');
}

function employeeNotFound()
{
  debug('Employee not found.');
  shouldBe("event.target.result", "undefined");
}

function newTransactionAborted()
{
  debug('The transaction was aborted.');

  var finalTransaction = db.transaction(['employees'],
                                        'readonly');
  finalTransaction.oncomplete = finalTransactionCompleted;
  finalTransaction.onabort = finalTransactionAborted;

  var request = finalTransaction.objectStore('employees').get(0);
  request.onsuccess = employeeNotFound;
  request.onerror = unexpectedErrorCallback;
}

function newTransactionCompleted()
{
  fail('The new transaction should not complete.');
}

function employeeAdded()
{
  debug('Added an employee inside the transaction.');
  newTransaction.abort();
}

function onSetVersionComplete()
{
  debug('Creating new transaction.');
  window.newTransaction = db.transaction(['employees'],
                                         'readwrite');
  newTransaction.oncomplete = newTransactionCompleted;
  newTransaction.onabort = newTransactionAborted;

  var request = newTransaction.objectStore('employees').put(
      {id: 0, name: 'John Doe', desk: 'LON-BEL-123'});
  request.onsuccess = employeeAdded;
  request.onerror = unexpectedErrorCallback;
}

function onSetVersion()
{
  // We are now in a set version transaction.
  window.db = event.target.result;
  debug('Creating object store.');
  deleteAllObjectStores(db);
  db.createObjectStore('employees', {keyPath: 'id'});
}

function test()
{
  indexedDBTest(onSetVersion, onSetVersionComplete);
}
