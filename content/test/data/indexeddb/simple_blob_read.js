// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DB = 'db';
const STORE = 'store';
const DATA = 'blob value';

function run() {
  Object.assign(indexedDB.open(DB), {
    onerror: unexpectedErrorCallback,
    onupgradeneeded(e) {
      debug("Created object store.");
      e.target.result.createObjectStore(STORE, {
        keyPath: 'id',
      });
    },
    onsuccess(e) {
      debug("Opened database.");
      const idb = /** @type IDBDatabase */ e.target.result;
      const op = idb
        .transaction(STORE, 'readwrite')
        .objectStore(STORE)
        .put({
          id: 'foo',
          blob: new Blob([DATA]),
        });
      op.onerror = unexpectedErrorCallback;
      op.onsuccess = () => {
        debug("Wrote blob.");
        idb.close();
        setTimeout(verify);
      }
    },
  });
}

function verify(e) {
  debug("Reading blob.");
  Object.assign(indexedDB.open(DB), {
    onerror: unexpectedErrorCallback,
    onsuccess(e) {
      const idb = /** @type IDBDatabase */ e.target.result;
      const op = idb
        .transaction(STORE, 'readonly')
        .objectStore(STORE)
        .get('foo');
      op.onerror = unexpectedErrorCallback;
      op.onsuccess = async e => {
        debug("Got blob.");
        idb.close();
        const entry = e.target.result;
        if (!entry) {
          fail('BAD: nothing was written');
        } else {
          const text = await (
              await fetch(URL.createObjectURL(entry.blob))).text();
          if (text === DATA) {
            done();
          } else {
            fail(`BAD: written "${DATA}", got "${text}"`)
          }
        }
      }
    },
  });
}
