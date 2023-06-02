// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';

(async function() {
  TestRunner.addResult(`Tests how table names are escaped in database table view.\n`);
  await TestRunner.loadLegacyModule('console');

  var tableName = 'table-name-with-dashes-and-"quotes"';
  var escapedTableName = Resources.DatabaseTableView.prototype.escapeTableName(tableName, '', true);
  TestRunner.addResult('Original value: ' + tableName);
  TestRunner.addResult('Escaped value: ' + escapedTableName);
  TestRunner.completeTest();
})();
