// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {DataGridTestRunner} from 'data_grid_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests SegmentedRange\n`);

  function testCase(testName, data, merge, expectSameBackwards) {
    TestRunner.addResult('Test case: ' + testName);
    TestRunner.addResult('Input Segments: ' + JSON.stringify(data));

    var forwardRange = new Common.SegmentedRange.SegmentedRange(merge);
    data.map(entry => new Common.SegmentedRange.Segment(entry[0], entry[1], entry[2])).forEach(forwardRange.append, forwardRange);
    var forward = forwardRange.segments();

    var backwardRange = new Common.SegmentedRange.SegmentedRange(merge);
    data.reverse()
        .map(entry => new Common.SegmentedRange.Segment(entry[0], entry[1], entry[2]))
        .forEach(backwardRange.append, backwardRange);
    var backward = backwardRange.segments();

    // Only do reverse if we merge, otherwise result is order-dependent.
    if (expectSameBackwards && forward.length !== backward.length) {
      TestRunner.addResult(
          `FAIL: mismatch between forward and backward results, ${forward.length} vs. ${backward.length}`);
      expectSameBackwards = false;
    }
    TestRunner.addResult('Result:');
    for (var i = 0; i < forward.length; ++i) {
      var f = forward[i], b = backward[i];
      TestRunner.addResult(`${f.begin} - ${f.end}: ${f.data}`);
      if (expectSameBackwards && b && (f.begin !== b.begin || f.end !== b.end || f.data !== b.data))
        TestRunner.addResult(`FAIL: Forward/backward mismatch, reverse segment is ${b.begin} - ${b.end}: ${b.data}`);
    }
    if (!expectSameBackwards) {
      TestRunner.addResult('Result backwards:');
      for (var b of backward)
        TestRunner.addResult(`${b.begin} - ${b.end}: ${b.data}`);
    }
  }

  function merge(first, second) {
    if (first.begin > second.begin)
      TestRunner.addResult(
          `FAIL: merge() callback called with arguments in wrong order, ${first.begin} vs. ${second.begin}`);
    return first.end >= second.begin && first.data === second.data ? first : null;
  }

  testCase('one', [[0, 1, 'a']], merge, true);
  testCase('two adjacent', [[0, 1, 'a'], [1, 2, 'a']], merge, true);
  testCase('two apart', [[0, 1, 'a'], [2, 3, 'a']], merge, true);
  testCase('two overlapping', [[0, 2, 'a'], [2, 3, 'a']], merge, true);
  testCase('two overlapping no merge ', [[0, 2, 'a'], [2, 3, 'b']], null, true);
  testCase('one inside another', [[0, 3, 'a'], [1, 2, 'a']], merge, true);
  testCase('one inside another, no merge', [[0, 3, 'a'], [1, 2, 'b']]);
  testCase('one between two others', [[0, 2, 'a'], [3, 5, 'a'], [2, 3, 'a']], merge, true);
  testCase('one between two others, no merge', [[0, 2, 'a'], [3, 5, 'b'], [2, 3, 'a']], null, true);
  testCase('one overlapping two others', [[0, 2, 'a'], [3, 5, 'a'], [1, 4, 'a']], merge, true);
  testCase('one overlapping two others, no merge', [[0, 2, 'a'], [3, 5, 'b'], [1, 4, 'a']]);
  testCase('one consuming many:', [[0, 1, 'a'], [2, 3, 'a'], [4, 5, 'a'], [6, 7, 'a'], [2, 6, 'a']], merge, true);
  testCase('one consuming many, no merge:', [[0, 1, 'a'], [2, 3, 'a'], [4, 5, 'a'], [6, 7, 'a'], [2, 6, 'a']]);
  TestRunner.completeTest();
})();
