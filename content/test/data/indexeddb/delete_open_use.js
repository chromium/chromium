// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function doTest() {
  // Delete the database if it's left over from a previous test run.
  const dbname = 'delete-open-use-test';
  indexedDB.deleteDatabase(dbname);

  // Create the database.
  let openRequest = indexedDB.open(dbname, 1);
  openRequest.onerror = unexpectedErrorCallback;
  openRequest.onupgradeneeded = (event) => {
    const connection = event.target.result;
    // Close the connection when the delete is requested.
    connection.onversionchange = () => connection.close();
  };
  openRequest.onblocked = unexpectedBlockedCallback;

  // Request delete of the database.
  let deleteRequest = indexedDB.deleteDatabase(dbname);
  deleteRequest.onerror = unexpectedErrorCallback;
  deleteRequest.onblocked = unexpectedBlockedCallback;
  deleteRequest.onsuccess =
    () => {
      indexedDB.databases().then((dbs) => {
        if (dbs.length !== 0) {
          fail('Expected no databases, but found ' + dbs.length);
        }
    });
    }

  // Create the database again. Crucially, this must happen synchronously after
  // requesting the delete to cause the open() to be enqueued behind the delete,
  // in order to trigger https://crbug.com/429974682
  let connection;
  let recreateRequest = indexedDB.open(dbname, 1);
  recreateRequest.onupgradeneeded = (event) => {
    connection = event.target.result;
  };

  recreateRequest.onsuccess = (event) => {
    // Verify success via database enumeration.
    indexedDB.databases().then((dbs) => {
      if (dbs.length !== 1) {
        fail('Expected 1 database, but found ' + dbs.length);
      }
    });

    connection.close();
    // Re-open existing database --- from "scratch", since the other connection
    // has been closed.
    const reopenRequest = indexedDB.open(dbname, 1);
    // onupgradeneeded should not be called because the database should already
    // exist.
    reopenRequest.onupgradeneeded = unexpectedUpgradeNeededCallback;
    reopenRequest.onblocked = unexpectedBlockedCallback;
    reopenRequest.onsuccess = done;
  };
}
