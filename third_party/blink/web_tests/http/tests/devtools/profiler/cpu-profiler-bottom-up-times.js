// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CPUProfilerTestRunner} from 'cpu_profiler_test_runner';

import * as ProfilerModule from 'devtools/panels/profiler/profiler.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests bottom-up view self and total time calculation in CPU profiler.\n`);

  var profileAndExpectations = {
    'title': 'profile1',
    'target': function() {
      return SDK.TargetManager.TargetManager.instance().targets()[0];
    },
    'profileModel': () => new SDK.CPUProfileDataModel.CPUProfileDataModel({
      'nodes': [
        {
          'id': 0,
          'callFrame': {'functionName': '(root)', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 0, 'columnNumber': 0},
          'hitCount': 350
        },
        {
          'id': 1,
          'callFrame': {'functionName': '(idle)', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 1, 'columnNumber': 0},
          'hitCount': 1000,
          'parent': 0
        },
        {
          'id': 2,
          'callFrame': {'functionName': 'A', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 4642, 'columnNumber': 0},
          'hitCount': 250,
          'parent': 0
        },
        {
          'id': 3,
          'callFrame': {'functionName': 'C', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 525, 'columnNumber': 0},
          'hitCount': 100,
          'parent': 2
        },
        {
          'id': 4,
          'callFrame': {'functionName': 'D', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 425, 'columnNumber': 0},
          'hitCount': 20,
          'parent': 3
        },
        {
          'id': 5,
          'callFrame': {'functionName': 'B', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 4662, 'columnNumber': 0},
          'hitCount': 150,
          'parent': 0
        },
        {
          'id': 6,
          'callFrame': {'functionName': 'C', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 525, 'columnNumber': 0},
          'hitCount': 100,
          'parent': 5
        },
        {
          'id': 7,
          'callFrame': {'functionName': 'D', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 425, 'columnNumber': 0},
          'hitCount': 20,
          'parent': 6
        },
        {
          'id': 8,
          'callFrame': {'functionName': 'D', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 425, 'columnNumber': 222},
          'hitCount': 10,
          'parent': 6
        }
      ],
      'startTime': 0,
      'endTime': 1e6
    })
  };
  var view = new ProfilerModule.CPUProfileView.CPUProfileView(profileAndExpectations);
  view.viewSelectComboBox.setSelectedIndex(1);
  view.changeView();
  var tree = view.profileDataGridTree;
  if (!tree)
    TestRunner.addResult('no tree');
  var node = tree.children[0];
  if (!node)
    TestRunner.addResult('no node');
  while (node) {
    TestRunner.addResult(
        node.callUID + ': ' + node.functionName + ' ' + node.self + ' ' + node.total + ' ' +
        node.element().textContent);
    node = node.traverseNextNode(true, null, true);
  }
  CPUProfilerTestRunner.completeProfilerTest();
})();
