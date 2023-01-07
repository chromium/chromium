// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests tracing based Timeline save/load functionality.\n`);
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  TestRunner.TestTimelineLoaderClient = function() {
    this.completePromise = new Promise(resolve => this.resolve = resolve);
  };

  TestRunner.TestTimelineLoaderClient.prototype = {
    loadingStarted: function() {
      TestRunner.addResult('TimelineLoaderClient.loadingStarted()');
    },

    loadingProgress: function() {},

    processingStarted: function() {
      TestRunner.addResult('TimelineLoaderClient.processingStarted()');
    },

    loadingComplete: function(model) {
      TestRunner.addResult(`TimelineLoaderClient.loadingComplete(${!!model})`);
      this.resolve(model);
    },

    modelPromise: function() {
      return this.completePromise;
    }
  };

  async function runTestWithDataAndCheck(input, expectedOutput, callback) {
    function checkSaveData(output) {
      var saveData = JSON.parse(output);
      TestRunner.addResult(
          'Saved data is equal to restored data: ' + (JSON.stringify(expectedOutput) === JSON.stringify(saveData)));
      callback();
    }

    var client = new TestRunner.TestTimelineLoaderClient();
    var blob = new Blob([input], {type: 'text/pain'});
    var loader = Timeline.TimelineLoader.loadFromFile(blob, client);
    var stream = new TestRunner.StringOutputStream(TestRunner.safeWrap(checkSaveData));
    var model = await client.modelPromise();
    var storage = model.backingStorage();
    await stream.open('');
    storage.writeToStream(stream);
  }

  async function runTestOnMalformedInput(input, callback) {
    var client = new TestRunner.TestTimelineLoaderClient();
    var blob = new Blob([input], {type: 'text/pain'});
    var loader = Timeline.TimelineLoader.loadFromFile(blob, client);
    var model = await client.modelPromise();
    TestRunner.addResult('Model is empty: ' + (!model || (model.minimumRecordTime() === Infinity && (model.maximumRecordTime() === -Infinity || model.maximumRecordTime() === 0))));
    callback();
  }

  var data = [
    {'args': {'number': 32}, 'cat': '_metadata', 'name': 'num_cpus', 'ph': 'M', 'pid': 32127, 'tid': 0, 'ts': 0},
    {
      'args': {'sort_index': -5},
      'cat': '_metadata',
      'name': 'process_sort_index',
      'ph': 'M',
      'pid': 32127,
      'tid': 12,
      'ts': 0
    },
    {
      'args': {'name': 'Renderer'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 32127,
      'tid': 12,
      'ts': 0
    },
    {
      'args': {'sort_index': -1},
      'cat': '_metadata',
      'name': 'thread_sort_index',
      'ph': 'M',
      'pid': 32127,
      'tid': 11,
      'ts': 0
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 32120,
      'tid': 9,
      'ts': 95904702436,
      'tts': 1161841
    },
    {'args': {'number': 32}, 'cat': '_metadata', 'name': 'num_cpus', 'ph': 'M', 'pid': 32120, 'tid': 0, 'ts': 0},
    {
      'args': {'sort_index': -5},
      'cat': '_metadata',
      'name': 'process_sort_index',
      'ph': 'M',
      'pid': 32120,
      'tid': 10,
      'ts': 0
    },
    {
      'args': {'name': 'Renderer'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 32120,
      'tid': 10,
      'ts': 0
    },
    {
      'args': {'sort_index': -1},
      'cat': '_metadata',
      'name': 'thread_sort_index',
      'ph': 'M',
      'pid': 32120,
      'tid': 9,
      'ts': 0
    },
    {
      'args': {'name': 'CrRendererMain'},
      'cat': '_metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 32120,
      'tid': 9,
      'ts': 0
    },
    {
      'args': {
        'data': {
          'frame': '0x1cfa1f6a4000',
          'scriptId': '52',
          'scriptLine': 664,
          'scriptName': 'http://example.com/foo.js'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 21,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 32169,
      'tdur': 20,
      'tid': 35,
      'ts': 95904848776,
      'tts': 2613659
    },
    {
      'args': {'stack': []},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32169,
      's': 'g',
      'tid': 35,
      'ts': 95904848783,
      'tts': 2613665
    },
    {
      'args': {
        'data': {
          'frame': '0x1cfa1f6a4000',
          'scriptId': '52',
          'scriptLine': 664,
          'scriptName': 'http://example.com/foo.js'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 20,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 32169,
      'tdur': 18,
      'tid': 35,
      'ts': 95904848821,
      'tts': 2613704
    },
    {
      'args': {'stack': []},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32169,
      's': 'g',
      'tid': 35,
      'ts': 95904848827,
      'tts': 2613710
    },
    {
      'args': {
        'data': {
          'frame': '0x1cfa1f6a4000',
          'scriptId': '52',
          'scriptLine': 664,
          'scriptName': 'http://example.com/foo.js'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 19,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 32169,
      'tdur': 18,
      'tid': 35,
      'ts': 95904848866,
      'tts': 2613749
    },
    {
      'args': {'stack': []},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32169,
      's': 'g',
      'tid': 35,
      'ts': 95904848872,
      'tts': 2613755
    },
    {
      'args': {
        'data': {
          'frame': '0x1cfa1f6a4000',
          'scriptId': '52',
          'scriptLine': 664,
          'scriptName': 'http://example.com/foo.js'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 19,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 32169,
      'tdur': 19,
      'tid': 35,
      'ts': 95904848909,
      'tts': 2613791
    },
    {
      'args': {'stack': []},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32169,
      's': 'g',
      'tid': 35,
      'ts': 95904848915,
      'tts': 2613797
    },
    {
      'args': {
        'data': {
          'frame': '0x1cfa1f6a4000',
          'scriptId': '52',
          'scriptLine': 664,
          'scriptName': 'http://example.com/foo.js'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 21,
      'name': 'FunctionCall',
      'ph': 'X',
      'pid': 32169,
      'tdur': 19,
      'tid': 35,
      'ts': 95904848954,
      'tts': 2613837
    },
    {
      'args': {'stack': []},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32169,
      's': 'g',
      'tid': 35,
      'ts': 95904848961,
      'tts': 2613843
    },
    {
      'args': {'data': {'sessionId': '26.5', 'frames': [
        {'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}
      ]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904694459,
      'tts': 1432596
    },
    {
      'args': {'data': {'layerTreeId': 1, 'frame': 'frame1'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'SetLayerTreeId',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904694693,
      'tts': 1432692
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 32157,
      'tid': 26,
      'ts': 95904694731,
      'tts': 1432729
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 32157,
      'tid': 26,
      'ts': 95904694789,
      'tts': 1432787
    },
    {
      'args': {'data': {'type': 'beforeunload'}},
      'cat': 'disabled-by-default-devtools.timeline',
      'dur': 16,
      'name': 'EventDispatch',
      'ph': 'X',
      'pid': 32157,
      'tdur': 13,
      'tid': 26,
      'ts': 95904695027,
      'tts': 1433025
    },
    {
      'args': {
        'data': {
          'frame': '0x30acf4ca4000',
          'requestId': '26.422',
          'requestMethod': 'GET',
          'url': 'http://localhost/bar.html?ws=E16865E8B9D1'
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'ResourceSendRequest',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904695434,
      'tts': 1433433
    },
    {
      'args': {'stack': null},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904695455,
      'tts': 1433453
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 32157,
      'tid': 26,
      'ts': 95904695551,
      'tts': 1433549
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'B',
      'pid': 32157,
      'tid': 26,
      'ts': 95904696695,
      'tts': 1433692
    },
    {
      'args': {},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'Program',
      'ph': 'E',
      'pid': 32157,
      'tid': 26,
      'ts': 95904696737,
      'tts': 1433733
    },
    {
      'args': {'data': {'frame': '0x30acf4ca4000', 'mimeType': 'text/html', 'requestId': '26.422', 'statusCode': 200}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'ResourceReceiveResponse',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904699823,
      'tts': 1433961
    },
    {
      'args': {'data': {'frame': '0x30acf4ca4000', 'identifier': 406}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'WebSocketDestroy',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904701483,
      'tts': 1435612
    },
    {
      'args': {'stack': null},
      'cat': 'disabled-by-default-devtools.timeline.stack',
      'name': 'CallStack',
      'ph': 'I',
      'pid': 32157,
      's': 'g',
      'tid': 26,
      'ts': 95904701489,
      'tts': 1435618
    },
    {'args': {'number': 32}, 'cat': '_metadata', 'name': 'num_cpus', 'ph': 'M', 'pid': 32072, 'tid': 0, 'ts': 0},
    {
      'args': {'sort_index': -6},
      'cat': '_metadata',
      'name': 'process_sort_index',
      'ph': 'M',
      'pid': 32072,
      'tid': 32096,
      'ts': 0
    },
    {
      'args': {'name': 'Browser'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 32072,
      'tid': 32096,
      'ts': 0
    },
    {
      'args': {'name': 'CrBrowserMain'},
      'cat': '_metadata',
      'name': 'thread_name',
      'ph': 'M',
      'pid': 32072,
      'tid': 32072,
      'ts': 0
    }
  ];

  TestRunner.runTestSuite([
    function testNormal(next) {
      var input = JSON.stringify(data);
      runTestWithDataAndCheck(input, data, next);
    },

    function testJSONObjectFormat(next) {
      var json = JSON.stringify(data);
      var input = '{"traceEvents":' + json + '}';
      runTestWithDataAndCheck(input, data, next);
    },

    function testJSONObjectFormatWithMetadata(next) {
      var json = JSON.stringify(data);
      var input = '{"traceEvents":' + json + ', metadata: {"foo": "bar", "baz": "quz"}';
      runTestWithDataAndCheck(input, data, next);
    },

    function testBroken(next) {
      var data = [{
        'args': {'number': 32},
        'cat': '_metadata',
        'name': 'num_cpus',
        'ph': 'M',
        'pid': 32127,
        'tid': 0,
        'ts': 0,
        't"y}p}e\\': 'UnknownRecordType'
      }];
      runTestOnMalformedInput(JSON.stringify(data), next);
    },

    function testMalformedJSON(next) {
      runTestOnMalformedInput(']', next);
    }
  ]);
})();
