// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test auto zoom feature of the timeline.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
  var pid = 100;

  var traceEvents = [
    {
      'args': {'data': {'sessionId': sessionId, 'frames': [
        {'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}
      ]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 100,
    },
    {
      'name': 'Program',
      'ts': 1000000,
      'dur': 10000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 1000000,
      'dur': 10000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'Program',
      'ts': 2000000,
      'dur': 500000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 2000000,
      'dur': 500000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'Program',
      'ts': 3000000,
      'dur': 400000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 3000000,
      'dur': 400000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'Program',
      'ts': 4000000,
      'dur': 200000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 4000000,
      'dur': 200000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'Program',
      'ts': 5000000,
      'dur': 1000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 5000000,
      'dur': 1000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'Program',
      'ts': 6000000,
      'dur': 1000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 6000000,
      'dur': 1000,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    }
  ];

  UI.panels.timeline.setModel(await PerformanceTestRunner.createPerformanceModelWithEvents(traceEvents));

  var overview = UI.panels.timeline.overviewPane;
  var startTime = overview.windowStartTime;
  var endTime = overview.windowEndTime;
  TestRunner.assertGreaterOrEqual(startTime, 1010);
  TestRunner.assertGreaterOrEqual(2000, startTime);
  TestRunner.assertGreaterOrEqual(endTime, 4200);
  TestRunner.assertGreaterOrEqual(5000, startTime);
  TestRunner.completeTest();
})();
