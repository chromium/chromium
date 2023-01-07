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
      'name': 'NonAscii',
      'ts': 10000,
      args: {'nonascii': '\u043b\u0435\u0442 \u043c\u0438 \u0441\u043f\u0438\u043a \u0444\u0440\u043e\u043c \u043c\u0430\u0439 \u0445\u0430\u0440\u0442'},
      'dur': 1000,
      'ph': 'X',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {
      'name': 'NonAsciiSnapshot',
      'ts': 20000,
      args: {'snapshot': '\u0442\u0435\u0441\u0442'},
      'dur': 1000,
      'ph': 'O',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {
      'name': 'ShortSnapshot',
      'ts': 20000,
      args: {'snapshot': 'short snapshot data'},
      'dur': 1000,
      'ph': 'O',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    },
    {
      'name': 'LongSnapshot',
      'ts': 20000,
      args: {'snapshot': 'abcdef'.repeat(10000)},
      'dur': 1000,
      'ph': 'O',
      'tid': mainThread,
      'pid': 100,
      'cat': 'test'
    }
  ];

  function getEventByName(name) {
    return thread.events().filter(function(event) {
      return event.name === name;
    })[0];
  }

  var model = PerformanceTestRunner.createTracingModel(testData);
  var process = model.sortedProcesses()[0];
  var thread = process.sortedThreads()[0];
  TestRunner.assertEquals('\u043b\u0435\u0442 \u043c\u0438 \u0441\u043f\u0438\u043a \u0444\u0440\u043e\u043c \u043c\u0430\u0439 \u0445\u0430\u0440\u0442', getEventByName('NonAscii').args['nonascii']);
  getEventByName('ShortSnapshot').requestObject(step1);
  function step1(object) {
    TestRunner.assertEquals('short snapshot data', object);
    getEventByName('LongSnapshot').requestObject(step2);
  }
  function step2(object) {
    TestRunner.assertEquals('abcdef'.repeat(10000), object);
    getEventByName('NonAsciiSnapshot').requestObject(step3);
  }
  function step3(object) {
    TestRunner.assertEquals('\u0442\u0435\u0441\u0442', object);
    TestRunner.addResult('DONE');
    TestRunner.completeTest();
  }
})();
