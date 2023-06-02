// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {CPUProfilerTestRunner} from 'cpu_profiler_test_runner';

(async function() {
  TestRunner.addResult(`Tests filtering of native nodes.
    Also tests loading of a legacy nodes format, where nodes were represented as a tree.\n`);

  var profile = {
    'head': {
      'callFrame': {'functionName': '(root)', 'scriptId': '0', 'url': '', 'lineNumber': 0},
      'id': 1,
      'hitCount': 100000,
      'children': [
        {
          'callFrame': {'functionName': 'holder', 'scriptId': '0', 'url': '', 'lineNumber': 0},
          'id': 10,
          'hitCount': 10000,
          'children': [{
            'callFrame': {'functionName': 'nativeA', 'scriptId': '40', 'url': 'native 1.js', 'lineNumber': 4642},
            'id': 2,
            'hitCount': 1000,
            'children': [
              {
                'callFrame': {'functionName': 'nativeAaa', 'scriptId': '40', 'url': 'native 2.js', 'lineNumber': 5025},
                'id': 3,
                'hitCount': 100,
                'children': [
                  {
                    'callFrame': {'functionName': 'Aaa', 'scriptId': '40', 'url': 'native 3.js', 'lineNumber': 5025},
                    'id': 4,
                    'hitCount': 10,
                    'children': []
                  },
                  {
                    'callFrame': {'functionName': 'Aaaaaa', 'scriptId': '40', 'url': '', 'lineNumber': 505},
                    'id': 5,
                    'hitCount': 20,
                    'children': []
                  }
                ]
              },
              {
                'callFrame': {'functionName': 'Bbb', 'scriptId': '40', 'url': '', 'lineNumber': 505},
                'id': 6,
                'hitCount': 200,
                'children': []
              }
            ]
          }]
        },
        {
          'callFrame': {'functionName': 'C', 'scriptId': '40', 'url': '', 'lineNumber': 4642},
          'id': 7,
          'hitCount': 20000,
          'children': [
            {
              'callFrame': {'functionName': 'nativeDdd', 'scriptId': '41', 'url': 'native 3.js', 'lineNumber': 55},
              'id': 8,
              'hitCount': 2000,
              'children': []
            },
            {
              'callFrame': {'functionName': 'Ccc', 'scriptId': '40', 'url': '', 'lineNumber': 525},
              'id': 9,
              'hitCount': 1000,
              'children': []
            }
          ]
        }
      ]
    },
    'idleTime': 202.88199791684747,
    'startTime': 100000,
    'endTime': 100000 + 111.110 + 22.220 + 1.000,
    'samples': [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
  };
  Common.settingForTest('showNativeFunctionsInJSProfile').set(false);
  var model = new SDK.CPUProfileDataModel(profile);
  printTree('', model.profileHead);
  function printTree(padding, node) {
    TestRunner.addResult(
        `${padding}${node.functionName} id:${node.id} total:${node.total} self:${node.self} depth:${node.depth}`);
    node.children.sort((a, b) => a.id - b.id).forEach(printTree.bind(null, padding + '  '));
  }
  TestRunner.addResult(model.samples.join(', '));
  TestRunner.completeTest();
})();
