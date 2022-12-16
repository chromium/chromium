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
      'name': 'nested',
      'ph': 'S',
      'ts': 120000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nested',
      'ph': 'S',
      'ts': 121001,
      'args': {},
      'id': 42,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nested',
      'ph': 'F',
      'ts': 126100,
      'args': {},
      'id': 42,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nested',
      'ph': 'F',
      'ts': 126999,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'stepInto',
      'ph': 'S',
      'ts': 130000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'stepInto',
      'ph': 'T',
      'ts': 130200,
      'args': {'step': 's1'},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'stepInto',
      'ph': 'T',
      'ts': 130800,
      'args': {'step': 's2'},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'stepInto',
      'ph': 'F',
      'ts': 131000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'stepPast',
      'ph': 'S',
      'ts': 140000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'stepPast',
      'ph': 'p',
      'ts': 140220,
      'args': {'step': 's1'},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'stepPast',
      'ph': 'p',
      'ts': 140800,
      'args': {'step': 's2'},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'stepPast',
      'ph': 'F',
      'ts': 141000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping1',
      'ph': 'S',
      'ts': 150000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping2',
      'ph': 'S',
      'ts': 151000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping3',
      'ph': 'S',
      'ts': 152000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping1',
      'ph': 'F',
      'ts': 153000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping4',
      'ph': 'S',
      'ts': 153500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping2',
      'ph': 'F',
      'ts': 154000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping5',
      'ph': 'S',
      'ts': 154000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping6',
      'ph': 'S',
      'ts': 154500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping4',
      'ph': 'F',
      'ts': 154500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping3',
      'ph': 'F',
      'ts': 155000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping5',
      'ph': 'F',
      'ts': 155000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping7',
      'ph': 'S',
      'ts': 155500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping6',
      'ph': 'F',
      'ts': 155500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping7',
      'ph': 'F',
      'ts': 156500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping8',
      'ph': 'S',
      'ts': 157500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestedOverlapping8',
      'ph': 'F',
      'ts': 158500,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'unterminated',
      'ph': 'S',
      'ts': 160000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'multithread',
      'ph': 'S',
      'ts': 160000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'multithread',
      'ph': 'T',
      'ts': 161000,
      'args': {'step': 'step'},
      'id': 1,
      'tid': 101,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'multithread',
      'ph': 'F',
      'ts': 162000,
      'args': {},
      'id': 1,
      'tid': 102,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'nestableNested1',
      'ph': 'b',
      'ts': 170000,
      'args': {},
      'id': 123,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestableNested2',
      'ph': 'b',
      'ts': 171000,
      'args': {'step': 'n1'},
      'id': 123,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestableNested2',
      'ph': 'e',
      'ts': 177000,
      'args': {'step': 'n2'},
      'id': 123,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestableNested1',
      'ph': 'e',
      'ts': 179000,
      'args': {},
      'id': 123,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'nestableEndWithoutBegin',
      'ph': 'e',
      'ts': 181000,
      'args': {},
      'id': 124,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'nestableUnterminated',
      'ph': 'b',
      'ts': 182000,
      'args': {},
      'id': 124,
      'tid': mainThread,
      'pid': pid
    },

    {
      'cat': 'blink.console',
      'name': 'crossProcess',
      'ph': 'S',
      'ts': 190000,
      'args': {},
      'id': 1,
      'tid': mainThread,
      'pid': pid
    },
    {
      'cat': 'blink.console',
      'name': 'crossProcess',
      'ph': 'T',
      'ts': 190800,
      'args': {'step': 's2'},
      'id': 1,
      'tid': mainThread + 10,
      'pid': pid + 1
    },
    {
      'cat': 'blink.console',
      'name': 'crossProcess',
      'ph': 'F',
      'ts': 191000,
      'args': {},
      'id': 1,
      'tid': mainThread + 100,
      'pid': pid + 2
    },
    {
      'cat': 'devtools.timeline',
      'name': 'RunTask',
      'ph': 'E',
      'ts': 191000,
      'args': {},
      'id': 1,
      'tid': mainThread + 100,
      'pid': pid + 2
    },
  ];

  var model = PerformanceTestRunner.createTracingModel(testData);
  var events = model.sortedProcesses()[0].threadById(mainThread).asyncEvents();
  for (var i = 0; i < events.length; ++i) {
    var stepString = events[i].name + ' ' + events[i].startTime + '-' + events[i].endTime + ': ';
    var steps = events[i].steps;
    for (var j = 0; j < steps.length; ++j) {
      var step = steps[j];
      if (j)
        stepString += ', ';
      stepString += step.phase + ' ' + step.startTime;
    }
    TestRunner.addResult(stepString);
  }
  TestRunner.completeTest();
})();
