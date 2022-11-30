// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Checks Ui DevTools performance panel does not use generic tracing.\n`);
  Root.Runtime.experiments.enableForTest('timelineShowAllEvents');
  await TestRunner.loadLegacyModule('timeline'); await TestRunner.loadTestModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  const rawTraceEvents = [
    {
      'args': {'name': 'CrBrowserMain'},
      'cat': '_metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 10000,
      'tid': 123,
      'ts': 0
    },
    {
      'args': {
        "data": {
          "persistentIds":true,
          "frames": [
            { "frame":"ui_devtools_browser_frame", "name":"Browser",
              "processId":10000 },
            { "frame":"ui_devtools_gpu_frame", "name":"Gpu",
              "processId":20000 },
          ]
        }
      },
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInBrowser',
      'ph': 'I',
      'pid': 17800,
      'tid': 123,
      'ts': 100000,
      'tts': 606543
    },
    {"pid":10000,"tid":210657,"ts":382830364662,"ph":"X","cat":"viz,benchmark","name":"DirectRenderer::DrawFrame","dur":895,"tdur":895,"tts":1341607,"args":{}},
    {"pid":10000,"tid":210683,"ts":382830893114,"ph":"B","cat":"cc,disabled-by-default-devtools.timeline","name":"RasterTask","tts":28550,"args":{"tileData":{"tileId":{"id_ref":"0x298e7bb83210"},"tileResolution":"HIGH_RESOLUTION","sourceFrameNumber":37,"layerId":82}}},
    {"pid":10000,"tid":10000,"ts":100500,"ph":"I","cat":"disabled-by-default-devtools.timeline","name":"SetLayerTreeId","s":"t","tts":606560,"args":{"data":{"frame":"ui_devtools_browser_frame","layerTreeId":1}}},
    {"pid":10000,"tid":10000,"ts":101000,"ph":"I","cat":"disabled-by-default-devtools.timeline.frame","name":"BeginFrame","s":"t","tts":606660,"args":{"layerTreeId":1,"frameSeqId":100}},
    {"pid":10000,"tid":10000,"ts":101100,"ph":"I","cat":"disabled-by-default-devtools.timeline.frame","name":"BeginFrame","s":"t","tts":606760,"args":{"layerTreeId":1,"frameSeqId":101}},
    {"pid":10000,"tid":10000,"ts":101200,"ph":"I","cat":"disabled-by-default-devtools.timeline.frame","name":"DrawFrame","s":"t","tts":606860,"args":{"layerTreeId":1,"frameSeqId":100}},
    {"pid":10000,"tid":10000,"ts":101200,"ph":"I","cat":"disabled-by-default-devtools.timeline.frame","name":"BeginFrame","s":"t","tts":606860,"args":{"layerTreeId":1,"frameSeqId":102}},
    {"pid":10000,"tid":10000,"ts":101300,"ph":"I","cat":"disabled-by-default-devtools.timeline.frame","name":"DrawFrame","s":"t","tts":606960,"args":{"layerTreeId":1,"frameSeqId":101}},
    {"pid":10000,"tid":10000,"ts":101400,"ph":"I","cat":"disabled-by-default-devtools.timeline.frame","name":"DrawFrame","s":"t","tts":607060,"args":{"layerTreeId":1,"frameSeqId":102}},
  ];

  const timeline = UI.panels.timeline;
  const model = await PerformanceTestRunner.createPerformanceModelWithEvents(rawTraceEvents);
  timeline.setModel(model);

  TestRunner.addResult(`isGenericTrace: ${model.timelineModel().isGenericTrace()}\n`);
  const frames = timeline.flameChart.mainDataProvider.performanceModel.frames();
  TestRunner.addResult(`Number of frames: ${frames.length}\n`);

  TestRunner.completeTest();
})();
