// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that tracing model correctly processes trace events.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');

  var mainThread = 1;
  var pid = 100;

  var testData = [
    {'name': 'Outer', 'ts': 10000, args: {}, 'dur': 1000, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 10000, args: {}, 'dur': 1000, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 20000, args: {}, 'dur': 1000, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 20000, args: {}, 'dur': 999, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 30000, args: {}, 'dur': 1000, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 30000, args: {}, 'dur': 999, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 40000, args: {}, 'ph': 'B', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 40000, args: {}, 'dur': 1000, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Outer', 'ts': 41000, args: {}, 'ph': 'E', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 50000, args: {}, 'ph': 'B', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 50000, args: {}, 'dur': 999, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Outer', 'ts': 51000, args: {}, 'ph': 'E', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 60000, args: {}, 'ph': 'B', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 60001, args: {}, 'dur': 999, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Outer', 'ts': 61000, args: {}, 'ph': 'E', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 70000, args: {}, 'dur': 1000, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 70000, args: {}, 'ph': 'B', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Inner', 'ts': 71000, args: {}, 'ph': 'E', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 80000, args: {}, 'dur': 0, 'ph': 'X', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Other', 'ts': 80000, args: {}, 'ph': 'B', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {'name': 'Other', 'ts': 80000, args: {}, 'ph': 'E', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 90000, args: {}, 'ph': 'B', 'tid': mainThread, 'pid': 100, 'cat': 'test'},
    {
      'name': 'Inner',
      'ts': 90000,
      args: {},
      'ph': 'X',
      'dur': 0,
      'ph': 'X',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {'name': 'Outer', 'ts': 90000, args: {}, 'ph': 'E', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {
      'name': 'Outer',
      'ts': 100000,
      args: {},
      'ph': 'X',
      'dur': 1000,
      'ph': 'X',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {'name': 'Inner', 'ts': 100000, args: {}, 'ph': 'I', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {
      'name': 'Outer',
      'ts': 110000,
      args: {},
      'ph': 'X',
      'dur': 1000,
      'ph': 'X',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {'name': 'Other', 'ts': 111000, args: {}, 'ph': 'I', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {
      'name': 'Outer',
      'ts': 120000,
      args: {},
      'ph': 'X',
      'dur': 1000,
      'ph': 'X',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {'name': 'Inner', 'ts': 120999, args: {}, 'ph': 'I', 'tid': mainThread, 'pid': 100, 'cat': 'test'},

    {'name': 'Outer', 'ts': 130000, args: {}, 'ph': 'I', 'tid': mainThread, 'pid': 100, 'cat': 'test'}
  ];

  var model = PerformanceTestRunner.createTracingModel(testData);
  var events = model.sortedProcesses()[0].threadById(mainThread).events();
  for (var i = 0; i < events.length; ++i) {
    var event = events[i];
    TestRunner.addResult(event.phase + ' ' + event.name + ' ' + event.startTime + ' - ' + event.endTime);
  }
  TestRunner.completeTest();
})();
