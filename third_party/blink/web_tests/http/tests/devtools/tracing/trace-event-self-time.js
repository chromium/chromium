// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test trace event self time.\n`);
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
      'args': {'data': {'frame': '0x2f7b63884000'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'InvalidateLayout',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 210000,
      'tts': 5612442
    },
    {
      'args':
          {'beginData': {'dirtyObjects': 10, 'frame': '0x2f7b63884000', 'partialLayout': true, 'totalObjects': 179}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Layout',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 220000,
      'tts': 1758056
    },
    {
      'args': {'endData': {'root': [0, 286, 1681, 286, 1681, 1371, 0, 1371], 'rootNode': 9}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Layout',
      'ph': 'E',
      'pid': 17851,
      'tid': 23,
      'ts': 270000,
      'tts': 1758430
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 17851,
      'tid': 23,
      'ts': 300000,
      'tts': 5612451
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 400000,
      'tts': 5612504
    },
    {
      'args': {'type': 'blur'},
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 60000,
      'name': 'EventDispatch',
      'ph': 'X',
      'pid': 17851,
      'tdur': 60,
      'tid': 23,
      'ts': 410000,
      'tts': 1769084
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
      'dur': 10000,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tdur': 74,
      'tid': 23,
      'ts': 420000,
      'tts': 1769136
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
      'dur': 10000,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tdur': 74,
      'tid': 23,
      'ts': 440000,
      'tts': 1769136
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Animation',
      'ph': 'b',
      'pid': 17851,
      'tid': 23,
      'ts': 445000,
    },
    {
      'args': {'data': {'page': '0x4age222'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'CommitLoad',
      'ph': 'X',
      'dur': 1000,
      'pid': 17851,
      'tid': 23,
      'ts': 446000,
    },
    {
      'args': {},
      'cat': 'webkit.console',
      'name': 'timestamp',
      'ph': 'S',
      'pid': 17851,
      'tid': 23,
      'ts': 450000,
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 17851,
      'tid': 23,
      'ts': 500000,
      'tts': 5612506
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 1200000,
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Animation',
      'ph': 'e',
      'pid': 17851,
      'tid': 23,
      'ts': 1245000,
    },
    {
      'args': {},
      'cat': 'webkit.console',
      'name': 'timestamp',
      'ph': 'F',
      'pid': 17851,
      'tid': 23,
      'ts': 1345000,
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
    endTime: 430e3,
    nodes: [
      {callFrame: {functionName: '(root)'}, id: 1, children: [2]},
      {callFrame: {functionName: 'foo'}, id: 2, children: [3, 4]}, {callFrame: {functionName: 'bar'}, id: 3},
      {callFrame: {functionName: 'baz'}, id: 4}
    ],
    timeDeltas: new Array(9).fill(1000),
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
