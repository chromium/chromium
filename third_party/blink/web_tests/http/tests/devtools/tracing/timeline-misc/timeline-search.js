// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test search in Timeline FlameChart View.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
  var rasterThread = 2;
  var pid = 100;

  var testData = [
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
      'args': {'name': 'Renderer'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': pid,
      'tid': mainThread,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '_metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': pid,
      'tid': mainThread,
      'ts': 0
    },
    {
      'args': {'name': 'CompositorTileWorker'},
      'cat': '_metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': pid,
      'tid': rasterThread,
      'ts': 0
    },

    {
      'name': 'Program',
      'ts': 2000000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 2010000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'Painting'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2020000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar11'}}
    },
    {
      'name': 'Painting',
      'ts': 2100000,
      'ph': 'S',
      'id': 100,
      'tid': mainThread,
      'pid': pid,
      'cat': 'blink.console',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 2120000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar12'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2180000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'Layout',
      'ts': 2190000,
      'ph': 'X',
      'dur': 100000,
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'beginData': {'frame': 0x12345678}, 'endData': {'layoutRoots':[]}}
    },
    {
      'name': 'Paint',
      'ts': 2210000,
      'ph': 'X',
      'dur': 100000,
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo13', 'clip': [0,0,2,0,2,2,0,2]}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2380000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar13'}}
    },
    {
      'name': 'Painting',
      'ts': 2382000,
      'ph': 'F',
      'id': 100,
      'tid': mainThread,
      'pid': pid,
      'cat': 'blink.console',
      'args': {}
    },
    {
      'name': 'Program',
      'ts': 2400000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },

    {
      'name': 'RasterTask',
      'ts': 2011000,
      'ph': 'X',
      'dur': 1000,
      'tid': rasterThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'RasterTask',
      'ts': 2260000,
      'ph': 'X',
      'dur': 1000,
      'tid': rasterThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'DecodeImage',
      'ts': 2270000,
      'ph': 'X',
      'dur': 1000,
      'tid': rasterThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
    {
      'name': 'RasterTask',
      'ts': 2280000,
      'ph': 'X',
      'dur': 1000,
      'tid': rasterThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {}}
    },
  ];

  var timeline = UI.panels.timeline;
  var model = await PerformanceTestRunner.createPerformanceModelWithEvents(testData);
  timeline.setModel(model);
  var flameChartView = timeline.flameChart;
  var searchConfig = new UI.SearchableView.SearchConfig('Paint', false, false);
  flameChartView.performSearch(searchConfig, true, false);
  TestRunner.addResult(`Count: ${flameChartView.searchResults.length}`);
  for (var i = 0; i <= flameChartView.searchResults.length; ++i) {
    var selection = timeline.selection;
    if (!selection || selection.type() !== Timeline.TimelineSelection.Type.TraceEvent) {
      TestRunner.addResult(`Invalid selection type: ${selection && selection.type()}`);
      continue;
    }
    var event = selection.object();
    TestRunner.addResult(`${event.startTime}: ${event.phase} ${event.name} ${event.thread.name()}`);
    flameChartView.jumpToNextSearchResult();
  }
  TestRunner.completeTest();
})();
