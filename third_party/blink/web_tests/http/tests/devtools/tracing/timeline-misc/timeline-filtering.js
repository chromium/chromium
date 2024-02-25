// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as Timeline from 'devtools/panels/timeline/timeline.js';

(async function() {
  TestRunner.addResult(`Test filtering in Timeline Tree View panel.\n`);
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
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
      'name': 'top level event name',
      'ts': 1000000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'toplevel',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1010000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar01'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1020000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar02'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1120000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar03'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1180000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1210000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar04'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1220000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo05'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1320000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar06'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1380000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 1410000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar07'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1420000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo08'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1520000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo09'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 1580000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'top level event name',
      'ts': 1990000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'toplevel',
      'args': {}
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
      'args': {'data': {'message': 'foo10'}}
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
      'name': 'TimeStamp',
      'ts': 2210000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo13'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2220000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo14'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2320000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'bar15'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2380000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'TimeStamp',
      'ts': 2410000,
      'ph': 'B',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo16'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2420000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo17'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2520000,
      'ph': 'I',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {'data': {'message': 'foo18'}}
    },
    {
      'name': 'TimeStamp',
      'ts': 2580000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    },
    {
      'name': 'Program',
      'ts': 2590000,
      'ph': 'E',
      'tid': mainThread,
      'pid': pid,
      'cat': 'disabled-by-default-devtools.timeline',
      'args': {}
    }
  ];

  var model = await PerformanceTestRunner.createPerformanceModelWithEvents(testData);
  const tabbedPane = Timeline.TimelinePanel.TimelinePanel.instance().flameChart.detailsView.tabbedPane;
  tabbedPane.selectTab(Timeline.TimelineDetailsView.Tab.EventLog);
  const view = tabbedPane.visibleView;

  view.setModel(model, PerformanceTestRunner.mainTrack());
  view.updateContents(Timeline.TimelineSelection.TimelineSelection.fromRange(
      model.timelineModel().minimumRecordTime(), model.timelineModel().maximumRecordTime()));
  function printEventMessage(event, level) {
    const text = event.args['data'] && event.args['data']['message'] || event.name;
    TestRunner.addResult(' '.repeat(level) + text);
  }

  async function dumpRecords() {
    await PerformanceTestRunner.walkTimelineEventTreeUnderNode(printEventMessage, view.currentTree);
    TestRunner.addResult('');
  }

  TestRunner.addResult('Initial:');
  await dumpRecords();

  TestRunner.addResult(`Filtered by 'bar':`);
  view.textFilterUI.setValue('bar', true);
  await dumpRecords();

  TestRunner.addResult(`Filtered by 'foo':`);
  view.textFilterUI.setValue('foo', true);
  await dumpRecords();

  TestRunner.completeTest();
})();
