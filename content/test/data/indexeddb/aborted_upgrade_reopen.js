// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Regression test for crbug.com/545736196.
//
// Tests connection deduplication behavior when multiple concurrent callers
// probe whether a database exists by opening it without a version, aborting the
// versionchange transaction in `onupgradeneeded`, immediately closing the
// connection, and then reopening the database with an explicit version in
// `onerror`.
//
// Multiple concurrent callers (5) are launched simultaneously so that while the
// first caller's upgrade transaction is being aborted and closed, the
// subsequent callers are queued as shared requests. This verifies that:
// 1. Aborting the primary request cleanly promotes the next pending shared
//    request without leaking an uncommitted connection to the shared cache.
// 2. The subsequent reopen requests do not hang by attempting to attach to a
//    closed/aborted connection.

'use strict';

const DB_NAME = 'aborted_upgrade_reopen_db';

function runCaller() {
  return new Promise((resolve, reject) => {
    const probe = indexedDB.open(DB_NAME);
    probe.onupgradeneeded = () => {
      const db = probe.result;
      probe.transaction.abort();
      db.close();
    };
    probe.onerror = () => {
      reopen();
    };
    probe.onsuccess = () => {
      probe.result.close();
      reopen();
    };

    function reopen() {
      const req = indexedDB.open(DB_NAME, 1);
      req.onupgradeneeded = () => {
        if (!req.result.objectStoreNames.contains('store')) {
          req.result.createObjectStore('store', {keyPath: 'id'});
        }
      };
      req.onsuccess = () => {
        req.result.close();
        resolve();
      };
      req.onerror = (e) => {
        reject(new Error(req.error ? req.error.name : 'UnknownError'));
      };
    }
  });
}

function test() {
  const req = indexedDB.deleteDatabase(DB_NAME);
  req.onsuccess = req.onerror = async () => {
    try {
      const callers = [];
      for (let i = 0; i < 5; i++) {
        callers.push(runCaller());
      }
      await Promise.all(callers);
      done();
    } catch (e) {
      fail('Failed: ' + e);
    }
  };
}
