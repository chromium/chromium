// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test trace-specific implementation of timeline model\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
  var pid = 100;

  var commonMetadata = [
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
      'args': {'data': {'layerTreeId': 17, 'frame': 'frame1'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'SetLayerTreeId',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 101,
    }
  ];

  var traceEvents = [
    {
      'name': 'Program',
      'ts': 1000000,
      'dur': 9999,
      'ph': 'X',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 1000001,
      'dur': 9998,
      'ph': 'X',
      args: {'data': {}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'ResourceSendRequest',
      'ts': 1000002,
      'ph': 'I',
      args: {'data': {'requestId': 1, 'url': 'http://example.com', 'requestMethod': 'GET'}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'RecalculateStyles',
      'ts': 1001003,
      'dur': 997,
      'ph': 'X',
      args: {data: {}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'Layout',
      'ts': 1002001,
      'ph': 'B',
      args: {beginData: {}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'Layout',
      'ts': 1003000,
      'ph': 'E',
      args: {endData: {'layoutRoots':[]}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },

    {
      'name': 'Program',
      'ts': 2000000,
      'ph': 'B',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 2000001,
      'ph': 'B',
      args: {'data': {}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'ResourceSendRequest',
      'ts': 2000002,
      'ph': 'I',
      args: {'data': {'requestId': 1, 'url': 'http://example.com', 'requestMethod': 'GET'}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'RecalculateStyles',
      'ts': 2001003,
      'ph': 'B',
      args: {beginData: {}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'RecalculateStyles',
      'ts': 2002000,
      'ph': 'E',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'Layout',
      'ts': 2002101,
      'ph': 'B',
      args: {beginData: {}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'Layout',
      'ts': 2003001,
      'ph': 'E',
      args: {endData: {'layoutRoots':[]}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'ParseHTML',
      'ts': 2004000,
      'ph': 'B',
      args: {'beginData': {'url': 'http://example.com', 'startLine': 777}},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'ParseHTML',
      'ts': 2004100,
      'ph': 'E',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'FunctionCall',
      'ts': 2009999,
      'ph': 'E',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    },
    {
      'name': 'Program',
      'ts': 2009999,
      'ph': 'E',
      args: {},
      'tid': mainThread,
      'pid': 100,
      'cat': 'disabled-by-default.devtools.timeline'
    }
  ];

  await PerformanceTestRunner.createPerformanceModelWithEvents(commonMetadata.concat(traceEvents));
  await PerformanceTestRunner.forAllEvents(PerformanceTestRunner.mainTrackEvents(), async (event, stack) => {
    const prefix = Array(stack.length + 1).join('----') + (stack.length ? '> ' : '');
    const details = await Timeline.TimelineUIUtils.buildDetailsTextForTraceEvent(event, null) || '';
    TestRunner.addResult(`${prefix}${event.name}: ${details}`);
  });
  TestRunner.completeTest();
})();
