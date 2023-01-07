// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that tracing model correctly processes trace events.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');

  var mainThread = 1;
  var pid = 100;

  var testData = [
    {
      'cat': 'blink.console',
      'name': 'simple1',
      'ph': 'S',
      'ts': 100000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'simple1',
      'ph': 'F',
      'ts': 101000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'simple2',
      'ph': 'S',
      'ts': 110000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'simple2',
      'ph': 'F',
      'ts': 111000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'global-vs-old',
      'ph': 'S',
      'ts': 120000,
      'args': {},
      'id': 2,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'global-vs-old',
      'ph': 'S',
      'ts': 120010,
      'args': {},
      'id2': {global: 2},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'global-vs-old',
      'ph': 'F',
      'ts': 130000,
      'args': {},
      'id': 2,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'global-vs-old',
      'ph': 'F',
      'ts': 130010,
      'args': {},
      'id2': {global: 2},
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'global-x-process',
      'ph': 'S',
      'ts': 140000,
      'args': {},
      'id2': {global: 3},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'global-x-process',
      'ph': 'F',
      'ts': 150010,
      'args': {},
      'id2': {global: 3},
      'tid': mainThread,
      'pid': pid + 2
    },

    {
      'cat': 'blink.console',
      'name': 'local-x-process',
      'ph': 'S',
      'ts': 160000,
      'args': {},
      'id2': {local: 4},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-x-process',
      'ph': 'S',
      'ts': 160010,
      'args': {},
      'id2': {local: 4},
      'tid': mainThread,
      'pid': pid + 2
    },
    {
      'cat': 'blink.console',
      'name': 'local-x-process',
      'ph': 'F',
      'ts': 170000,
      'args': {},
      'id2': {local: 4},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-x-process',
      'ph': 'F',
      'ts': 170010,
      'args': {},
      'id2': {local: 4},
      'tid': mainThread,
      'pid': pid + 2
    },

    {
      'cat': 'blink.console',
      'name': 'local-vs-global',
      'ph': 'S',
      'ts': 180000,
      'args': {},
      'id2': {local: 5},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-vs-global',
      'ph': 'S',
      'ts': 180010,
      'args': {},
      'id2': {global: 5},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-vs-global',
      'ph': 'F',
      'ts': 190000,
      'args': {},
      'id2': {local: 5},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-vs-global',
      'ph': 'F',
      'ts': 190010,
      'args': {},
      'id2': {global: 5},
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'local-scope',
      'ph': 'S',
      'ts': 200000,
      'args': {},
      'scope': 's1',
      'id2': {local: 5},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-scope',
      'ph': 'S',
      'ts': 200010,
      'args': {},
      'scope': 's2',
      'id2': {local: 5},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-scope',
      'ph': 'F',
      'ts': 210000,
      'args': {},
      'scope': 's1',
      'id2': {local: 5},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'local-scope',
      'ph': 'F',
      'ts': 210010,
      'args': {},
      'scope': 's2',
      'id2': {local: 5},
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'global-scope',
      'ph': 'S',
      'ts': 210000,
      'args': {},
      'scope': 's1',
      'id2': {global: 6},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'global-scope',
      'ph': 'S',
      'ts': 210010,
      'args': {},
      'scope': 's2',
      'id2': {global: 6},
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'global-scope',
      'ph': 'F',
      'ts': 220000,
      'args': {},
      'scope': 's1',
      'id2': {global: 6},
      'tid': mainThread,
      'pid': pid + 2
    },
    {
      'cat': 'blink.console',
      'name': 'global-scope',
      'ph': 'F',
      'ts': 220010,
      'args': {},
      'scope': 's2',
      'id2': {global: 6},
      'tid': mainThread,
      'pid': pid + 2
    },
  ];

  var model = PerformanceTestRunner.createTracingModel(testData);
  var events = model.sortedProcesses()[0].threadById(mainThread).asyncEvents();
  for (var i = 0; i < events.length; ++i) {
    var stepString = `${events[i].name} ${events[i].startTime}-${events[i].endTime}: ` +
        events[i].steps.map(s => `${s.phase} ${s.startTime}`).join(', ');
    TestRunner.addResult(stepString);
  }
  TestRunner.completeTest();
})();
