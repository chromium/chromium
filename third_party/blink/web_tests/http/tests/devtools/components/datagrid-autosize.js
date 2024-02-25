// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as DataGrid from 'devtools/ui/legacy/components/data_grid/data_grid.js';

(async function() {
  TestRunner.addResult(`Tests DataGrid column auto size calculation.\n`);

  function testAutoSize(widths, minPercent, maxPercent) {
    TestRunner.addResult(
        'Auto sizing ' + JSON.stringify(widths) + ', minPercent=' + minPercent + ', maxPercent=' + maxPercent);
    var result = DataGrid.DataGrid.DataGridImpl.prototype.autoSizeWidths(widths, minPercent, maxPercent);
    TestRunner.addResult('    ' + JSON.stringify(result));
  }

  testAutoSize([198, 2, 400], 90);
  testAutoSize([1000], 5);
  testAutoSize([10], 5);
  testAutoSize([1000, 1000], 5);
  testAutoSize([30, 30, 30, 30], 5);
  testAutoSize([1, 100, 100, 100], 25);
  testAutoSize([100, 100, 100, 100], 25);
  testAutoSize([1, 1, 1, 100], 25);
  testAutoSize([1, 100, 100], 25, 40);
  testAutoSize([100, 100, 100], 25, 40);
  testAutoSize([1, 1, 100], 25, 40);

  // https://bugs.webkit.org/show_bug.cgi?id=101363
  testAutoSize([3, 10, 7, 7, 13, 13, 9, 10, 15, 15, 20, 20, 14, 14, 12, 12, 12, 10, 9, 14, 10, 6, 7, 10, 18], 5);
  TestRunner.completeTest();
})();
