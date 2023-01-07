// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests JS ignore list for timeline\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  const sessionId = '6.23';
  const rawTraceEvents = [
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
      'ph': 'B',
      'pid': 17851,
      'tid': 23,
      'ts': 200000,
      'tts': 5612442
    },
    {
      'args': {'data': {'functionName': 'level1', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 208000,
      'dur': 10000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level2', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 208000,
      'dur': 10000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level3', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 209000,
      'dur': 1000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level3ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 211000,
      'dur': 6000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level4user', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 211000,
      'dur': 1000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level4ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level5ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level6user', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level7ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level8user', 'url': 'user_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 213000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level1ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 230000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level1ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 235000,
      'dur': 3000,
      'tts': 1758056
    },
    {
      'args': {'data': {'functionName': 'level2ignoreListed', 'url': 'lib_script.js'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'JSFrame',
      'ph': 'X',
      'pid': 17851,
      'tid': 23,
      'ts': 236000,
      'dur': 1000,
      'tts': 1758056
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
    }
  ];

  function printTimelineData(dataProvider) {
    dataProvider.reset();
    const timelineData = dataProvider.timelineData();
    for (let i = 0; i < timelineData.entryStartTimes.length; ++i) {
      const name = dataProvider.entryTitle(i);
      const padding = '  '.repeat(timelineData.entryLevels[i]);
      TestRunner.addResult(`${padding}${name}: ${timelineData.entryTotalTimes[i]} @ ${timelineData.entryStartTimes[i]}`);
    }
  }

  Root.Runtime.experiments.enableForTest('ignoreListJSFramesOnTimeline');
  const dataProvider = new Timeline.TimelineFlameChartDataProvider();
  dataProvider.setModel(await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents));

  TestRunner.addResult('\nIgnore listed url: lib_script.js');
  Bindings.ignoreListManager.ignoreListURL('lib_script.js');
  printTimelineData(dataProvider);

  TestRunner.addResult('\nUnignored url: lib_script.js');
  Bindings.ignoreListManager.unIgnoreListURL('lib_script.js');
  printTimelineData(dataProvider);
  TestRunner.completeTest();
})();
