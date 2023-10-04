// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as TimelineModel from 'devtools/models/timeline_model/timeline_model.js';

(async function() {
  TestRunner.addResult(`Checks the FunctionCall with no closing event processed properly.\n`);
  await TestRunner.showPanel('timeline');

  var sessionId = '6.23';
  var rawTraceEvents = [
    {
      'args': {'name': 'Renderer'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '_metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 17851,
      'tid': 23,
      'ts': 0
    },
    {
      'args': {'data': {'sessionId': sessionId, 'frames': [
        {'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}
      ]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 100000,
      'tts': 606543
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 101000,
      'args': {}
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSSample',
      'ph': 'I',
      'pid': 17851,
      'tid': 23,
      'ts': 142000,
      'args': {'data': {'stackTrace': []}}
    }
  ];

  await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents);
  const event = PerformanceTestRunner.mainTrackEvents().find(
      e => e.name === TimelineModel.TimelineModel.RecordType.FunctionCall);
  TestRunner.addResult(`${event.name} ${event.startTime}`);
  TestRunner.completeTest();
})();
