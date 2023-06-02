// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CPUProfilerTestRunner} from 'cpu_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests self and total time calculation in CPU profiler.\n`);

  // Profile for 1070ms, 2140 samples.
  var profileAndExpectations = {
    'head': {
      'functionName': '(root)',
      'scriptId': '0',
      'url': '',
      'lineNumber': 0,
      'expectedTotalTime': 1070,
      'expectedSelfTime': 4,
      'hitCount': 8,
      'callUID': 4174152086,
      'children': [
        {
          'functionName': 'A',
          'scriptId': '40',
          'url': '',
          'lineNumber': 4642,
          'hitCount': 7,
          'expectedTotalTime': 1010,
          'expectedSelfTime': 3.5,
          'callUID': 1820492223,
          'children': [
            {
              'functionName': 'Aaa',
              'scriptId': '40',
              'url': '',
              'lineNumber': 5025,
              'hitCount': 2000,
              'expectedTotalTime': 1000,
              'expectedSelfTime': 1000,
              'callUID': 2901333737,
              'children': []
            },
            {
              'functionName': 'Bbb',
              'scriptId': '40',
              'url': '',
              'lineNumber': 505,
              'hitCount': 13,
              'expectedTotalTime': 6.5,
              'expectedSelfTime': 6.5,
              'callUID': 2901333737,
              'children': []
            }
          ]
        },
        {
          'functionName': 'C',
          'scriptId': '40',
          'url': '',
          'lineNumber': 4642,
          'hitCount': 5,
          'expectedTotalTime': 56,
          'expectedSelfTime': 2.5,
          'callUID': 1820492223,
          'children': [
            {
              'functionName': 'Ccc',
              'scriptId': '40',
              'url': '',
              'lineNumber': 525,
              'hitCount': 100,
              'expectedTotalTime': 50,
              'expectedSelfTime': 50,
              'callUID': 2901333737,
              'children': []
            },
            {
              'functionName': 'Ddd',
              'scriptId': '41',
              'url': '',
              'lineNumber': 55,
              'hitCount': 7,
              'expectedTotalTime': 3.5,
              'expectedSelfTime': 3.5,
              'callUID': 2901333737,
              'children': []
            }
          ]
        },
      ]
    },
    'idleTime': 202.88199791684747,
    'startTime': 1375445600.000847,
    'endTime': 1375445601.070847,
    'samples': [1, 2]
  };
  profileAndExpectations.root = profileAndExpectations.head;
  SDK.ProfileTreeModel.prototype.assignDepthsAndParents.call(profileAndExpectations);
  SDK.ProfileTreeModel.prototype.calculateTotals(profileAndExpectations.head);
  function checkExpectations(node) {
    if (Math.abs(node.selfTime - node.expectedSelfTime) > 0.0001) {
      TestRunner.addResult('totalTime: ' + node.totalTime + ', expected:' + node.expectedTotalTime);
      return false;
    }
    if (Math.abs(node.totalTime - node.expectedTotalTime) > 0.0001) {
      TestRunner.addResult('totalTime: ' + node.totalTime + ', expected:' + node.expectedTotalTime);
      return false;
    }
    for (var i = 0; i < node.children.length; i++) {
      if (!checkExpectations(node.children[i]))
        return false;
    }
    return true;
  }
  if (checkExpectations(profileAndExpectations.head))
    TestRunner.addResult('SUCCESS: all nodes have correct self and total times');
  else
    TestRunner.addResult('FAIL: incorrect node times\n' + JSON.stringify(profileAndExpectations, null, 4));
  TestRunner.completeTest();
})();
