// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

const DB_NAME = 'large-values-db';
const STORE_NAME = 'store';

// Each record exceeds the threshold for inlining bytes in BigBuffer.
const kRecordSize = 64 * 1024;
const kRecordCount = 128 + 1;
const kRecords = [];

function createDb(db) {
  const store = db.createObjectStore(STORE_NAME);
  for (let i = 0; i < kRecordCount; i++) {
    const record = new Uint8Array(kRecordSize);
    crypto.getRandomValues(record);
    kRecords.push(record);
    store.put(record, i);
  }
}

async function test() {
  const db = await promiseDeleteThenOpenDb(DB_NAME, createDb);
  const request =
      db.transaction(STORE_NAME, 'readonly').objectStore(STORE_NAME).getAll();
  request.onerror = unexpectedErrorCallback;
  request.onsuccess = () => {
    if (request.result.length !== kRecordCount) {
      fail(
          'getAll() returned wrong number of records: ' +
          request.result.length + ' vs ' + kRecordCount);
      return;
    }
    if (request.result.toString() !== kRecords.toString()) {
      fail('getAll() returned wrong records');
      return;
    }
    done();
  };
}
