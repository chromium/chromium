// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for IndexedDB.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.evaluateInPagePromise(`
      var version = 1;
      var db;

      function testFunction()
      {
          setTimeout(openDB);
      }

      function onError()
      {
          console.error("FAIL: " + new Error().stack);
          debugger;
      }

      function openDB()
      {
          var request = indexedDB.open("async-callstack-indexed-db", version);
          request.onupgradeneeded = onUpgradeNeeded;
          request.onsuccess = onSuccessOpenDB;
          request.onerror = onError;
      }

      function onUpgradeNeeded(e)
      {
          debugger;
          var db = e.target.result;
          e.target.transaction.onerror = onError;

          if (db.objectStoreNames.contains("foo"))
              db.deleteObjectStore("foo");

          db.createObjectStore("foo", { keyPath: "id" });
      }

      function onSuccessOpenDB(e)
      {
          debugger;
          db = e.target.result;
          populateDB();
      }

      function populateDB()
      {
          var trans = db.transaction(["foo"], "readwrite");
          var store = trans.objectStore("foo");
          var request = store.put({ "id" : 1, "bar": "baz" });
          request.onsuccess = onSuccessStorePut;
          request.onerror = onError;
      }

      function onSuccessStorePut(e)
      {
          debugger;
          getAllItems();
      }

      function getAllItems()
      {
          var trans = db.transaction(["foo"], "readwrite");
          var store = trans.objectStore("foo");
          var keyRange = IDBKeyRange.lowerBound(0);
          var cursorRequest = store.openCursor(keyRange);
          cursorRequest.onsuccess = onSuccessCursorRequest;
          cursorRequest.onerror = onError;
      }

      function onSuccessCursorRequest(e)
      {
          debugger;
          deleteItem();
      }

      function deleteItem()
      {
          var trans = db.transaction(["foo"], "readwrite");
          var store = trans.objectStore("foo");
          var request = store.delete(1);
          request.onsuccess = onSuccessDeleteItem;
          request.onerror = onError;
      }

      function onSuccessDeleteItem()
      {
          debugger;
          deleteDB();
      }

      function deleteDB()
      {
          db.close();
          var request = indexedDB.deleteDatabase("async-callstack-indexed-db");
          request.onsuccess = onSuccessDeleteDatabase;
          request.onerror = onError;
      }

      function onSuccessDeleteDatabase()
      {
          debugger;
      }
  `);

  var totalDebuggerStatements = 6;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
