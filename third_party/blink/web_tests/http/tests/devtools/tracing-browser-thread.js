// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test that tracing model correctly finds the main browser thread in the trace.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');

  var sessionId = 1;

  var testCases = {
    'chrome': [
      {
        'cat': '__metadata',
        'name': 'process_name',
        'ph': 'M',
        'pid': 1,
        'tid': 2,
        'ts': 0,
        'args': {'name': 'Renderer'}
      },
      {
        'cat': '__metadata',
        'name': 'thread_name',
        'ph': 'M',
        'pid': 1,
        'tid': 2,
        'ts': 0,
        'args': {'name': 'CrRendererMain'}
      },
      {
        'cat': 'disabled-by-default-devtools.timeline',
        'ph': 'I',
        'name': 'TracingStartedInPage',
        'pid': 1,
        'tid': 2,
        'ts': 100000,
        'tts': 606543,
        'args': {'sessionId': sessionId, 'page': '0x4age111'}
      },
      {
        'cat': '__metadata',
        'name': 'process_name',
        'ph': 'M',
        'pid': 3,
        'tid': 4,
        'ts': 0,
        'args': {'name': 'Browser'}
      },
      {
        'cat': '__metadata',
        'name': 'thread_name',
        'ph': 'M',
        'pid': 3,
        'tid': 4,
        'ts': 0,
        'args': {'name': 'CrBrowserMain'}
      },
      {
        'cat': 'disabled-by-default-devtools.timeline',
        'ph': 'I',
        'name': 'TracingStartedInPage',
        'pid': 1,
        'tid': 2,
        'ts': 100000,
        'tts': 606543,
        'args': {'sessionId': sessionId, 'page': '0x4age111'}
      },
    ],
    'headless': [
      {
        'cat': '__metadata',
        'name': 'process_name',
        'ph': 'M',
        'pid': 1,
        'tid': 2,
        'ts': 0,
        'args': {'name': 'Renderer'}
      },
      {
        'cat': '__metadata',
        'name': 'thread_name',
        'ph': 'M',
        'pid': 1,
        'tid': 2,
        'ts': 0,
        'args': {'name': 'CrRendererMain'}
      },
      {
        'cat': 'disabled-by-default-devtools.timeline',
        'ph': 'I',
        'name': 'TracingStartedInPage',
        'pid': 1,
        'tid': 2,
        'ts': 100000,
        'tts': 606543,
        'args': {'sessionId': sessionId, 'page': '0x4age111'}
      },
      {
        'cat': '__metadata',
        'name': 'process_name',
        'ph': 'M',
        'pid': 3,
        'tid': 4,
        'ts': 0,
        'args': {'name': 'HeadlessBrowser'}
      },
      {
        'cat': '__metadata',
        'name': 'thread_name',
        'ph': 'M',
        'pid': 3,
        'tid': 4,
        'ts': 0,
        'args': {'name': 'CrBrowserMain'}
      },
      {
        'cat': 'disabled-by-default-devtools.timeline',
        'ph': 'I',
        'name': 'TracingStartedInPage',
        'pid': 1,
        'tid': 2,
        'ts': 100000,
        'tts': 606543,
        'args': {'sessionId': sessionId, 'page': '0x4age111'}
      },
    ],
    'weird-names': [
      {
        'cat': '__metadata',
        'name': 'process_name',
        'ph': 'M',
        'pid': 1,
        'tid': 2,
        'ts': 0,
        'args': {'name': 'Renderer'}
      },
      {
        'cat': '__metadata',
        'name': 'thread_name',
        'ph': 'M',
        'pid': 1,
        'tid': 2,
        'ts': 0,
        'args': {'name': 'CrRendererMain'}
      },
      {
        'cat': 'disabled-by-default-devtools.timeline',
        'ph': 'I',
        'name': 'TracingStartedInPage',
        'pid': 1,
        'tid': 2,
        'ts': 100000,
        'tts': 606543,
        'args': {'sessionId': sessionId, 'page': '0x4age111'}
      },
      {'cat': '__metadata', 'name': 'process_name', 'ph': 'M', 'pid': 3, 'tid': 4, 'ts': 0, 'args': {'name': 'Foo'}},
      {'cat': '__metadata', 'name': 'thread_name', 'ph': 'M', 'pid': 3, 'tid': 4, 'ts': 0, 'args': {'name': 'Bar'}},
      {
        'cat': 'disabled-by-default-devtools.timeline',
        'ph': 'I',
        'name': 'TracingStartedInBrowser',
        'pid': 3,
        'tid': 4,
        'ts': 100000,
        'tts': 606543,
        'args': {'frameTreeNodeId': 1234}
      },
    ]
  };
  for (var testCase of Object.keys(testCases)) {
    var model = PerformanceTestRunner.createTracingModel(testCases[testCase]);
    var browserMain = SDK.TracingModel.browserMainThread(model);
    if (!browserMain) {
      TestRunner.addResult(`${testCase} failed, no browser main thread found`);
      continue;
    }
    TestRunner.addResult(`${testCase}: main browser thread is ${browserMain.name()} (#${browserMain.id()})`);
  }
  TestRunner.completeTest();
})();
