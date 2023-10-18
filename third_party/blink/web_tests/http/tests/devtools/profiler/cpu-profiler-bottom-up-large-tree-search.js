// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CPUProfilerTestRunner} from 'cpu_profiler_test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';
import * as ProfilerModule from 'devtools/panels/profiler/profiler.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that search works for large bottom-up view of CPU profile.\n`);

  var nodesCount = 200;
  function buildTree(startId, count) {
    // Build a call tree of a chain form: foo1 -> foo2 -> foo3 -> ...
    // This should give a O(n^2) nodes in bottom-up tree.
    var nodes = [];
    for (var i = 1; i <= count; ++i) {
      nodes.push({
        'id': startId + i - 1,
        'callFrame': {'functionName': 'foo' + i, 'scriptId': '0', 'url': 'a.js', 'lineNumber': i, 'columnNumber': 0},
        'hitCount': 10,
        'children': i < count ? [startId + i] : []
      });
    }
    return nodes;
  }
  var profileAndExpectations = {
    'title': 'profile1',
    'target': function() {
      return SDK.TargetManager.TargetManager.instance().targets()[0];
    },
    'profileModel': () => new SDK.CPUProfileDataModel.CPUProfileDataModel({
      'nodes': [
        {
          'id': 0,
          'callFrame': {
            'functionName': '(root)',
            'scriptId': '0',
            'url': 'a.js',
            'lineNumber': 0,
            'columnNumber': 0,
          },
          'hitCount': 1,
          'children': [1, 2]
        },
        {
          'id': 1,
          'callFrame': {'functionName': '(idle)', 'scriptId': '0', 'url': 'a.js', 'lineNumber': 1, 'columnNumber': 1},
          'hitCount': 2,
          'children': []
        }
      ].concat(buildTree(2, nodesCount)),
      'startTime': 0,
      'endTime': nodesCount * 10e3 + 3e3
    })
  };
  var view = new ProfilerModule.CPUProfileView.CPUProfileView(profileAndExpectations);
  view.viewSelectComboBox.setSelectedIndex(1);
  view.changeView();
  var tree = view.profileDataGridTree;
  if (!tree)
    TestRunner.addResult('no tree');
  tree.performSearch(new UIModule.SearchableView.SearchConfig('foo12', true, false), false);
  for (var item of tree.searchResults) {
    var node = item.profileNode;
    TestRunner.addResult(`${node.callUID}: ${node.functionName} ${node.self} ${node.total}`);
  }
  CPUProfilerTestRunner.completeProfilerTest();
})();
