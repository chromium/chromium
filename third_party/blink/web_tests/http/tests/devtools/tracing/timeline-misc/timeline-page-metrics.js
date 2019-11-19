// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Test timeline page metrics.`);
  await TestRunner.loadModule('performance_test_runner');
  await TestRunner.showPanel('timeline');

  function processTraceEvents(title, traceEvents) {
    TestRunner.addResult('\n\n' + title);
    const timeline = UI.panels.timeline;
    timeline._setModel(PerformanceTestRunner.createPerformanceModelWithEvents(traceEvents));
    const flamechart = timeline._flameChart._mainFlameChart;

    TestRunner.addResult(`Entries:`);
    const data = flamechart._timelineData();
    for (let i = 0; i < data.entryStartTimes.length; ++i) {
      TestRunner.addResult(
          `${data.entryStartTimes[i]} ${data.entryTotalTimes[i]} ${flamechart._dataProvider.entryTitle(i)}`);
    }

    TestRunner.addResult(`\nMarkers:`);
    for (const marker of data.markers)
      TestRunner.addResult(`${marker._startTime} ${marker._startOffset} ${marker._style.title}`);
  }

  processTraceEvents("Only main frame", [
    {
      'args': {'name': 'CrBrowserMain'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17800,
      'tid': 123,
      'ts': 0
    },
    {'args':{'name':'Renderer'},'cat':'__metadata','name':'process_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {'args':{'name':'CrRendererMain'},'cat':'__metadata','name':'thread_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {
      'args': {
        "data": {
          "frameTreeNodeId":550,
          "persistentIds":true,
          "frames": [
            { "frame":"853DD8D6CA3B85CA78375EF189B779F6", "url":"https://www.example.com/", "name":"main",
              "processId":17850 },
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

    {"pid":17850,"tid":775,"ts":100500,"ph":"I","cat":"devtools.timeline","name":"FrameStartedLoading","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6"},"tts":606660},
    {"pid":17850,"tid":775,"ts":101000,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606700},
    {"pid":17850,"tid":775,"ts":101200,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstContentfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606750},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaintCandidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
  ]);

  processTraceEvents("Multiple frames", [
    {
      'args': {'name': 'CrBrowserMain'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17800,
      'tid': 123,
      'ts': 0
    },
    {'args':{'name':'Renderer'},'cat':'__metadata','name':'process_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {'args':{'name':'Renderer'},'cat':'__metadata','name':'process_name','ph':'M','pid':17851,'tid':231,'ts':0},
    {'args':{'name':'Renderer'},'cat':'__metadata','name':'process_name','ph':'M','pid':17852,'tid':232,'ts':0},
    {'args':{'name':'CrRendererMain'},'cat':'__metadata','name':'thread_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {'args':{'name':'CrRendererMain'},'cat':'__metadata','name':'thread_name','ph':'M','pid':17851,'tid':231,'ts':0},
    {'args':{'name':'CrRendererMain'},'cat':'__metadata','name':'thread_name','ph':'M','pid':17852,'tid':232,'ts':0},
    {
      'args': {
        "data": {
          "frameTreeNodeId":550,
          "persistentIds":true,
          "frames": [
            { "frame":"853DD8D6CA3B85CA78375EF189B779F6", "url":"https://www.example.com/", "name":"main",
              "processId":17850 },
            { "frame":"9B72B5F1A83B2DE0F8843B3D22878B81", "url":"https://www.example1.com/frame1.html", "name":"frame1",
              "processId":17851, "parent":"853DD8D6CA3B85CA78375EF189B779F6"},
            { "frame":"5D83B01045AD652BE04EA9A444221149", "url":"https://www.example2.com/frame2.html", "name":"frame2",
              "processId":17852, "parent":"853DD8D6CA3B85CA78375EF189B779F6"}
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

    {"pid":17850,"tid":775,"ts":100500,"ph":"I","cat":"devtools.timeline","name":"FrameStartedLoading","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6"},"tts":606660},
    {"pid":17850,"tid":775,"ts":101000,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606700},
    {"pid":17850,"tid":775,"ts":101200,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstContentfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606750},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaintCandidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},

    {"pid":17851,"tid":775,"ts":100990,"ph":"I","cat":"devtools.timeline","name":"FrameStartedLoading","args":{"frame":"9B72B5F1A83B2DE0F8843B3D22878B81"},"tts":606660},
    {"pid":17851,"tid":775,"ts":101100,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstPaint","args":{"frame":"9B72B5F1A83B2DE0F8843B3D22878B81","data":{}},"tts":606700},
    {"pid":17851,"tid":775,"ts":101210,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstContentfulPaint","args":{"frame":"9B72B5F1A83B2DE0F8843B3D22878B81","data":{}},"tts":606750},
    {"pid":17851,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaintCandidate","args":{"frame":"9B72B5F1A83B2DE0F8843B3D22878B81","data":{}},"tts":606800},
    {"pid":17851,"tid":775,"ts":101310,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaint","args":{"frame":"9B72B5F1A83B2DE0F8843B3D22878B81","data":{}},"tts":606800},

    {"pid":17852,"tid":775,"ts":100880,"ph":"I","cat":"devtools.timeline","name":"FrameStartedLoading","args":{"frame":"5D83B01045AD652BE04EA9A444221149"},"tts":606660},
    {"pid":17852,"tid":775,"ts":101200,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstPaint","args":{"frame":"5D83B01045AD652BE04EA9A444221149","data":{}},"tts":606700},
    {"pid":17852,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstContentfulPaint","args":{"frame":"5D83B01045AD652BE04EA9A444221149","data":{}},"tts":606750},
    {"pid":17852,"tid":775,"ts":101400,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaintCandidate","args":{"frame":"5D83B01045AD652BE04EA9A444221149","data":{}},"tts":606800},
    {"pid":17852,"tid":775,"ts":101500,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaint","args":{"frame":"5D83B01045AD652BE04EA9A444221149","data":{}},"tts":606800},

    {"pid":17850,"tid":775,"ts":101600,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":100,"candidateIndex":1,"type":"image","size":1440,"isMainFrame":true}},"tts":606800},
    {"pid":17852,"tid":775,"ts":101700,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"5D83B01045AD652BE04EA9A444221149","data":{"nodeId":120,"candidateIndex":2,"type":"image","size":2000,"isMainFrame":false}},"tts":606800},
  ]);

 processTraceEvents("LCP invalidation 1", [
    {
      'args': {'name': 'CrBrowserMain'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17800,
      'tid': 123,
      'ts': 0
    },
    {'args':{'name':'Renderer'},'cat':'__metadata','name':'process_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {'args':{'name':'CrRendererMain'},'cat':'__metadata','name':'thread_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {
      'args': {
        "data": {
          "frameTreeNodeId":550,
          "persistentIds":true,
          "frames": [
            { "frame":"853DD8D6CA3B85CA78375EF189B779F6", "url":"https://www.example.com/", "name":"main",
              "processId":17850 },
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

    {"pid":17850,"tid":775,"ts":100500,"ph":"I","cat":"devtools.timeline","name":"FrameStartedLoading","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6"},"tts":606660},
    {"pid":17850,"tid":775,"ts":101000,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606700},
    {"pid":17850,"tid":775,"ts":101200,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstContentfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606750},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaintCandidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101600,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":100,"candidateIndex":1,"type":"image","size":1440,"isMainFrame":true}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101700,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":120,"candidateIndex":2,"type":"image","size":2000,"isMainFrame":true}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101800,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":140,"candidateIndex":3,"type":"image","size":3000,"isMainFrame":true}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101900,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Invalidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"candidateIndex":4,"isMainFrame":true}},"tts":606800},
  ]);

  processTraceEvents("LCP invalidation 2", [
    {
      'args': {'name': 'CrBrowserMain'},
      'cat': '__metadata',
      'name': 'process_name',
      'ph': 'M',
      'pid': 17800,
      'tid': 123,
      'ts': 0
    },
    {'args':{'name':'Renderer'},'cat':'__metadata','name':'process_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {'args':{'name':'CrRendererMain'},'cat':'__metadata','name':'thread_name','ph':'M','pid':17850,'tid':230,'ts':0},
    {
      'args': {
        "data": {
          "frameTreeNodeId":550,
          "persistentIds":true,
          "frames": [
            { "frame":"853DD8D6CA3B85CA78375EF189B779F6", "url":"https://www.example.com/", "name":"main",
              "processId":17850 },
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

    {"pid":17850,"tid":775,"ts":100500,"ph":"I","cat":"devtools.timeline","name":"FrameStartedLoading","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6"},"tts":606660},
    {"pid":17850,"tid":775,"ts":101000,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606700},
    {"pid":17850,"tid":775,"ts":101200,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstContentfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606750},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaintCandidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101300,"ph":"R","cat":"loading,rail,devtools.timeline","name":"firstMeaningfulPaint","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101600,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":100,"candidateIndex":1,"type":"image","size":1440,"isMainFrame":true}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101700,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":120,"candidateIndex":2,"type":"image","size":3000,"isMainFrame":true}},"tts":606800},
    {"pid":17850,"tid":775,"ts":101900,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Invalidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"candidateIndex":3,"isMainFrame":true}},"tts":606800},
    {"pid":17850,"tid":775,"ts":102000,"ph":"R","cat":"loading,rail,devtools.timeline","name":"largestContentfulPaint::Candidate","args":{"frame":"853DD8D6CA3B85CA78375EF189B779F6","data":{"nodeId":140,"candidateIndex":4,"type":"image","size":4000,"isMainFrame":true}},"tts":606800},
  ]);

  TestRunner.completeTest();

})();
