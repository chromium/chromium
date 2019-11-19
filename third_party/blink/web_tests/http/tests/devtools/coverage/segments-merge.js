// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the merge of disjoint segment lists in CoverageModel.\n`);
  await TestRunner.loadModule('coverage');

  testAndDump([], []);
  testAndDump([{end: 10, count: 1, stamp: 100}], []);
  testAndDump([{end: 10, count: 1, stamp: 100}], [{end: 10, count: 1, stamp: 100}]);
  testAndDump([{end: 10, count: 1, stamp: 100}], [{end: 20, count: 1, stamp: 100}]);
  testAndDump([{end: 10, count: 1, stamp: 100}, {end: 20, count: 1, stamp: 100}], []);
  testAndDump([{end: 30, count: 1, stamp: 100}], [{end: 10, count: undefined, stamp: 100}, {end: 20, count: 2, stamp: 100}]);
  testAndDump([{end: 30, count: undefined, stamp: 100}], [{end: 10, count: undefined, stamp: 100}, {end: 20, count: 2, stamp: 100}]);

  TestRunner.addResult(`Merging different stamps should result in the minimum timestamp`);
  testAndDump([{end: 10, count: 1, stamp: 100}], [{end: 10, count: 1, stamp: 200}]);
  testAndDump([{end: 10, count: 1, stamp: 100}], [{end: 20, count: 1, stamp: 200}]);
  testAndDump([{end: 10, count: 1, stamp: 100}, {end: 20, count: 1, stamp: 200}], []);
  testAndDump([{end: 30, count: 1, stamp: 100}], [{end: 10, count: undefined, stamp: 100}, {end: 20, count: 2, stamp: 200}]);

  TestRunner.completeTest();

  function testAndDump(a, b) {
    dumpSegments('A: ', a);
    dumpSegments('B: ', b);

    var mergedAB = Coverage.CoverageInfo._mergeCoverage(a, b);
    dumpSegments('merged: ', mergedAB);
    var mergedBA = Coverage.CoverageInfo._mergeCoverage(b, a);
    if (!rangesEqual(mergedAB, mergedBA))
      dumpSegments('FAIL, merge(b, a) != merge(a, b): ', mergedBA);
  }

  function dumpSegments(prefix, arr) {
    TestRunner.addResult((prefix || '') + JSON.stringify(arr));
  }

  function rangesEqual(a, b) {
    if (a.length !== b.length)
      return false;

    for (var i = 0; i < a.length; i++) {
      if (a[i].end !== b[i].end)
        return false;
      if (a[i].count !== b[i].count)
        return false;
    }
    return true;
  }
})();
