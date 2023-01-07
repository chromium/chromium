// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that aggregated summary in Timeline is properly computed.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var mainThread = 1;
  var pid = 100;
  var sessionId = '1';

  var testData = [
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 100,
      'args': {'data': {'sessionId': sessionId, 'frames': [
        {'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}
      ]}},
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'SetLayerTreeId',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 101,
      'args': {'data': {'frame': 'frame1', 'layerTreeId': 17}}
    },

    {
      'cat': 'toplevel',
      'name': 'Program',
      'ph': 'X',
      'ts': 100000,
      'dur': 3000,
      'tid': mainThread,
      'pid': pid,
      args: {}
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'X',
      'ts': 100500,
      dur: 1500,
      'tid': mainThread,
      'pid': pid,
      args: {}
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Layout',
      'ph': 'X',
      'ts': 101000,
      dur: 1000,
      'tid': mainThread,
      'pid': pid,
      args: {beginData: {}, endData: {'layoutRoots':[]}}
    },

    {
      'cat': 'toplevel',
      'name': 'Program',
      'ph': 'X',
      'ts': 104000,
      'dur': 4000,
      'tid': mainThread,
      'pid': pid,
      args: {}
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'X',
      'ts': 104000,
      'dur': 1000,
      'tid': mainThread,
      'pid': pid,
      args: {}
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'CommitLoad',
      'ph': 'X',
      'ts': 105000,
      'dur': 1000,
      'tid': mainThread,
      'pid': pid,
      args: {}
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Layout',
      'ph': 'X',
      'ts': 107000,
      'dur': 1000,
      'tid': mainThread,
      'pid': pid,
      args: {beginData: {}, endData: {'layoutRoots':[]}}
    },
  ];

  await PerformanceTestRunner.createPerformanceModelWithEvents(testData);
  for (var startTime = 100000; startTime <= 109000; startTime += 1000) {
    for (var endTime = startTime + 1000; endTime <= 109000; endTime += 1000) {
      dumpStats(
          startTime, endTime,
          Timeline.TimelineUIUtils.statsForTimeRange(PerformanceTestRunner.mainTrackEvents(), startTime / 1000, endTime / 1000));
    }
  }
  function dumpStats(t1, t2, obj) {
    var keys = Object.keys(obj).sort();
    var str = '';
    var total = 0;
    for (var k of keys) {
      total += obj[k];
      str += `${k}: ${obj[k]} `;
    }
    str += `total: ${total}`;
    TestRunner.addResult(`${t1}-${t2}: ${str}`);
  }
  TestRunner.completeTest();
})();
