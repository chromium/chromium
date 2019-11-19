// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test CPU profile timestamps are properly sorted.\n`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '6.23';
  var rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '__metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'sessionId': sessionId, 'page': '0x4age111'},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 100000,
      'tts': 606543
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 200000,
      'tts': 5612442
    },
    {
      'args': {
        'data': {
          'frame': '0x2f7b63884000',
          'scriptId': '52',
          'scriptLine': 539,
          'scriptName': 'devtools://devtools/bundled/ui/UIUtils.js'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 50000,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tdur': 74,
      'tid': 23,
      'ts': 420000,
      'tts': 1769136
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 17851,
      'tid': 23,
      'ts': 1500000,
    },
  ];

  var cpuProfile = {
    startTime: 420e3,
    endTime: 440e3,
    nodes: [
      {callFrame: {functionName: '(root)'}, id: 1, children: [2]},
      {callFrame: {functionName: 'foo'}, id: 2, children: [3, 4]}, {callFrame: {functionName: 'bar'}, id: 3},
      {callFrame: {functionName: 'baz'}, id: 4}
    ],
    timeDeltas: [...new Array(8).fill(2000), -1000],
    samples: [2, 2, 3, 3, 3, 4, 4, 2, 2]
  };

  var timelineController = PerformanceTestRunner.createTimelineController();
  timelineController._addCpuProfile(SDK.targetManager.mainTarget().id(), cpuProfile);
  timelineController.traceEventsCollected(rawTraceEvents);
  await timelineController._finalizeTrace();
  var events = UI.panels.timeline._performanceModel.timelineModel().inspectedTargetEvents();
  events.forEach(
      e => TestRunner.addResult(
          `${e.name}: ${e.startTime} ${(e.selfTime || 0).toFixed(2)}/${(e.duration || 0).toFixed(2)}`));
  TestRunner.completeTest();
})();
