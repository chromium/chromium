// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

(async function() {
  TestRunner.addResult(`Tests streaming CPU profile within trace log.\n`);
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
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 100000,
      'dur': 140000,
      'tts': 5612442
    },
    {
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 110000,
      'tts': 5610000,
      'dur': 90000,
      'args': {}
    },
    {
      'cat': 'disabled-by-default-v8.cpu_profile2',
      'name': 'Profile',
      'ph': 'P',
      'id': '0xa1f',
      'pid': 17851,
      'tid': 23,
      'ts': 110000,
      'tts': 5610000,
      'args': {
        'data': {
          'startTime': 110000,
          'cpuProfile': {
            'nodes': [
              {'callFrame': {'functionName': '(root)', 'scriptId': 0}, 'id': 1}, {
                'callFrame': {
                  'columnNumber': 26,
                  'functionName': 'foo',
                  'lineNumber': 1306,
                  'scriptId': 260,
                  'url': 'http://example.com/www-embed-player.js'
                },
                'id': 2,
                'parent': 1
              }
            ]
          }
        }
      },
    },
    {
      'cat': 'disabled-by-default-v8.cpu_profile2',
      'name': 'ProfileChunk',
      'ph': 'P',
      'id': '0xa1f',
      'pid': 17851,
      'tid': 24,
      'ts': 120000,
      'tts': 5620000,
      'args': {'data': {'cpuProfile': {'samples': [2, 2, 2]}, 'timeDeltas': [1000, 1500, 1000]}}
    },
    {
      'cat': 'disabled-by-default-v8.cpu_profile2',
      'name': 'ProfileChunk',
      'ph': 'P',
      'id': '0xa1f',
      'pid': 17851,
      'tid': 24,
      'ts': 120000,
      'tts': 5630000,
      'args': {
        'data': {
          'cpuProfile': {
            'nodes': [{
              'callFrame': {
                'columnNumber': 2,
                'functionName': 'bar',
                'lineNumber': 1400,
                'scriptId': 260,
                'url': 'http://example.com/www-embed-player.js'
              },
              'id': 3,
              'parent': 2
            }],
            'samples': [2, 3, 3, 3, 2, 1]
          },
          'timeDeltas': [500, 500, 1000, 500, 1000, 500]
        }
      }
    },
    {
      'cat': 'disabled-by-default-v8.cpu_profile2',
      'name': 'ProfileChunk',
      'ph': 'P',
      'id': '0xa1f',
      'pid': 17851,
      'tid': 24,
      'ts': 120000,
      'tts': 5630000,
      'args': {'data': {}}
    }
  ];

  await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents);
  var events = PerformanceTestRunner.mainTrackEvents();
  events.filter(e => e.name === 'JSFrame').forEach(e => {
    TestRunner.addResult(
        `${e.name}: ${e.startTime.toFixed(3)} / ${(e.duration || 0).toFixed(3)} ${e.args.data.functionName}`);
  });

  TestRunner.completeTest();
})();
