// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests asynchronous call stacks for Web SQL.\n`);
  await TestRunner.loadLegacyModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.loadHTML(`
      <input type="button" onclick="testFunction()" value="Test">
    `);
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          setTimeout(openDB);
      }

      function onSuccess()
      {
          debugger;
      }

      function onError()
      {
          console.error("FAIL: " + new Error().stack);
          debugger;
      }

      function openDB()
      {
          var dbSize = 1 * 1024 * 1024; // 1 MB
          var db = openDatabase("async-callstack-web-sql-db", "1.0", "Test DB", dbSize, onOpenDB);
          db.transaction(onCreateTableSQLTransactionCallback, onError, onSuccess);
      }

      function onOpenDB()
      {
          // This will be called only once when the database is created.
          // There is no way to delete a database in WebSQL from JavaScript,
          // so test async call stacks in this callback manually.
      }

      function onCreateTableSQLTransactionCallback(tx)
      {
          debugger;
          tx.executeSql("CREATE TABLE IF NOT EXISTS tmp(ID INTEGER PRIMARY KEY ASC, added_on DATETIME)", [], onSuccess, onError);
          tx.executeSql("INSERT INTO tmp(added_on) VALUES (?)", [new Date()], onSuccess, onError);
          tx.executeSql("DROP TABLE tmp", [], onDropTable, onError);
      }

      function onDropTable()
      {
          debugger;
      }
  `);

  var totalDebuggerStatements = 5;
  SourcesTestRunner.runAsyncCallStacksTest(totalDebuggerStatements);
})();
