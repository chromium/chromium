// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const DB = 'db';
const STORE = 'store';
const DATA = 'data';

// Regression test for crbug.com/392376370
// This test reads a blob out of IDB, creates a second blob that contains that
// blob, releases the IDB blob, and the looks up the same IDB blob again. This
// caused a crash because the containing blob held alive a reference to the
// BlobDataItemReader connection, and therefore the BlobReader. The living
// BlobReader would be reused for the second IDB lookup, but did not fully
// re-initialize itself with the blob registry.
function run() {
  Object.assign(indexedDB.open(DB), {
    onerror: unexpectedErrorCallback,
    onupgradeneeded(e) {
      debug('Created object store.');
      e.target.result.createObjectStore(STORE);
    },
    onsuccess(e) {
      debug('Opened database.');
      const idb = /** @type IDBDatabase */ e.target.result;
      const op = idb.transaction(STORE, 'readwrite')
                     .objectStore(STORE)
                     .put(new Blob([DATA]), 'foo');
      op.onerror = unexpectedErrorCallback;
      op.onsuccess = () => {
        debug('Wrote blob.');
        idb.close();
        setTimeout(verify);
      }
    },
  });
}

var blob;
var runs = 0;

function verify(e) {
  debug('Reading blob.');
  Object.assign(indexedDB.open(DB), {
    onerror: unexpectedErrorCallback,
    onsuccess(e) {
      const idb = /** @type IDBDatabase */ e.target.result;
      const op =
          idb.transaction(STORE, 'readonly').objectStore(STORE).get('foo');
      op.onerror = unexpectedErrorCallback;
      op.onsuccess = e => {
        debug('Got blob.');
        // Puts the returned IDB blob in another blob (as an array of length 1).
        blob = new Blob([e.target.result], {type: e.target.result.type});
        // The IDB blob needs to be GC'd to trigger the buggy path.
        setTimeout(gc);

        if (++runs < 2) {
          // Verify should be run twice, since the bug involved looking up the
          // IDB blob more than once.
          setTimeout(verify);
        } else {
          // No renderer crash = pass.
          done();
        }
      }
    },
  });
}
