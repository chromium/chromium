// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {PerformanceTestRunner} from 'performance_test_runner';

import * as TimelineModule from 'devtools/panels/timeline/timeline.js';

(async function() {
  TestRunner.addResult(`Test the frames are correctly built based on trace events\n`);
  await TestRunner.showPanel('timeline');

  var sessionId = '4.20';
  var mainThread = 1;
  var implThread = 2;
  var rasterThread = 3;
  var pid = 100;

  var commonMetadata = [
    {
      'args': {'data': {'sessionId': sessionId, 'frames': [
        {'frame': 'frame1', 'url': 'frameurl', 'name': 'frame-name'}
      ]}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'TracingStartedInPage',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 100,
    },
    {
      'args': {'data': {'frame': 'frame1', 'layerTreeId': 17}},
      'cat': 'disabled-by-default-devtools.timeline',
      'name': 'SetLayerTreeId',
      'ph': 'I',
      'pid': pid,
      'tid': mainThread,
      'ts': 101,
    }
  ];

  var testData = {
    'main thread only': [
      {
        'name': 'Program',
        'ts': 1000000,
        args: {},
        'dur': 4999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1000001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1000002,
        args: {},
        'dur': 3997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1004000,
        args: {'layerTreeId': 17},
        'dur': 999,
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1004001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1004002,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1016000,
        args: {},
        'dur': 10999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1016001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1016002,
        args: {},
        'dur': 2997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'StyleRecalculate',
        'ts': 1019000,
        args: {},
        'dur': 1999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Layout',
        'ts': 1021000,
        args: {'beginData': {'frame': 0x12345678},
       'endData': {'layoutRoots': [{'nodeId': 1, 'depth': 1, 'quads': []}]}},
        'dur': 5999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1027000,
        args: {'layerTreeId': 17},
        'dur': 999,
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1027001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1027002,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1032000,
        args: {},
        'dur': 1999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1032001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1032002,
        args: {'layerTreeId': 17},
        'dur': 999,
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1032003,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1032004,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1048000,
        args: {},
        'dur': 1999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1048001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1048002,
        args: {'layerTreeId': 17},
        'dur': 999,
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1048003,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1048004,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
    'impl-side only': [
      {
        'name': 'BeginFrame',
        'ts': 1000000,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1016000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1030000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1032000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1046000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1048000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1064000,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RasterTask',
        'ts': 1065001,
        args: {},
        'ph': 'X',
        'dur': 999,
        'tid': rasterThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RasterTopLevel',
        'ts': 1066000,
        args: {},
        'ph': 'X',
        'dur': 1000,
        'tid': rasterThread,
        'pid': 100,
        'cat': 'toplevel'
      },
      {
        'name': 'RasterTask',
        'ts': 1066001,
        args: {},
        'ph': 'X',
        'dur': 999,
        'tid': rasterThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RasterTask',
        'ts': 1067001,
        args: {},
        'ph': 'X',
        'dur': 999,
        'tid': rasterThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1078000,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1080000,
        args: {'layerTreeId': 17, 'frameSeqId': 105},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1081000,
        args: {'layerTreeId': 17, 'frameSeqId': 105},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Program',
        'ts': 10820000,
        args: {},
        'dur': 100,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
    'impl-side with commits': [
      {
        'name': 'BeginFrame',
        'ts': 1000000,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1000001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1014000,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1016000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1030000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1032000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1001000,
        args: {},
        'dur': 32999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1001001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1001002,
        args: {},
        'dur': 17997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'StyleRecalculate',
        'ts': 1019000,
        args: {},
        'dur': 1999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Layout',
        'ts': 1021000,
        args: {'beginData': {'frame': 0x12345678},
       'endData': {'layoutRoots': [{'nodeId': 1, 'depth': 1, 'quads': []}]}},
        'dur': 11999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1032000,
        args: {'layerTreeId': 17},
        'dur': 1999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1034000,
        args: {},
        'dur': 5999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1034002,
        args: {},
        'dur': 5997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'ActivateLayerTree',
        'ts': 1045001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1046000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1048001,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1060001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1062000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1049000,
        args: {},
        'dur': 11999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1049001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1050002,
        args: {'layerTreeId': 17},
        'dur': 10.997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1064000,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1064001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1065000,
        args: {},
        'dur': 13999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1065001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],

    'impl-side with interleaving commits': [
      {
        'name': 'BeginFrame',
        'ts': 984000,
        args: {'layerTreeId': 17, 'frameSeqId': 99 },
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1000000,
        args: {'layerTreeId': 17, 'frameSeqId': 100 },
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1000001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1014000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1014001,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1001000,
        args: {},
        'dur': 12999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1001001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1001002,
        args: {'layerTreeId': 17},
        'dur': 12.997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1016000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1016001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1030000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1030001,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1017000,
        args: {},
        'dur': 12999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1017001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1017002,
        args: {},
        'dur': 8997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1026000,
        args: {'layerTreeId': 17},
        'dur': 3999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1032000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1032001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1046000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1046001,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1033000,
        args: {},
        'dur': 12999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1033001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1033002,
        args: {'layerTreeId': 17},
        'dur': 7999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1041002,
        args: {},
        'dur': 4997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1048000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Program',
        'ts': 1049000,
        args: {},
        'dur': 999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
    'pre-frame time accounting': [
      {
        'name': 'BeginFrame',
        'ts': 1000000,
        args: {'layerTreeId': 17, 'frameSeqId': 99},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Program',
        'ts': 1000000,
        args: {},
        'dur': 29999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1000002,
        args: {},
        'dur': 28997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ScheduleStyleRecalculation',
        'ts': 1001001,
        args: {'data': {}},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1032000,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1032001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1034001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1035001,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1033000,
        args: {},
        'dur': 2999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1033001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RecalculateStyles',
        'ts': 1033002,
        args: {'layerTreeId': 17, 'beginData': {}},
        'dur': 1398,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1033401,
        args: {'layerTreeId': 17},
        'dur': 598,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1048000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1050000,
        args: {},
        'dur': 14999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1050002,
        args: {},
        'dur': 8997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'InvalidateLayout',
        'ts': 1059000,
        args: {'data': {}},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'DrawFrame',
        'ts': 1063001,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1064000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1064001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1071001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1071002,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1065000,
        args: {},
        'dur': 5999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1065001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Layout',
        'ts': 1065002,
        args: {'beginData': {'frame': 0x12345678},
       'endData': {'layoutRoots': [{'nodeId': 1, 'depth': 1, 'quads': []}]}},
        'dur': 2998,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1068001,
        args: {'layerTreeId': 17},
        'dur': 2998,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1080000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1080001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1081001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1081002,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1073000,
        args: {},
        'dur': 7999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ScrollLayer',
        'ts': 1073002,
        args: {'data': {'nodeId': 1}},
        'dur': 2998,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1096000,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1096001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1096002,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1081000,
        args: {},
        'dur': 999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1081001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1081001,
        args: {'layerTreeId': 17},
        'dur': 998,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
    'record processing order': [
      {
        'name': 'BeginFrame',
        'ts': 984000,
        args: {'layerTreeId': 17, 'frameSeqId': 99},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1000001,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1000002,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1013002,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1013005,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1016000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1001000,
        args: {},
        'dur': 15999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1001001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1001002,
        args: {},
        'dur': 11997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1013002,
        args: {'layerTreeId': 17},
        'dur': 3997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'ActivateLayerTree',
        'ts': 1030000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1030001,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1031000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1031002,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1061000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1062001,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1064000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Program',
        'ts': 1032000,
        args: {},
        'dur': 31999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1032001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1032002,
        args: {},
        'dur': 10997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1043002,
        args: {'layerTreeId': 17},
        'dur': 19997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'DrawFrame',
        'ts': 1080001,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
    'commits without activation': [
      {
        'name': 'BeginFrame',
        'ts': 984000,
        args: {'layerTreeId': 17, 'frameSeqId': 99},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1000000,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1000001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1014001,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1001000,
        args: {},
        'dur': 12999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1001001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1001002,
        args: {'layerTreeId': 17},
        'dur': 12.997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1016000,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1016001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1030001,
        args: {'layerTreeId': 17, 'frameSeqId': 101},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1017000,
        args: {},
        'dur': 12999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1017001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1017002,
        args: {},
        'dur': 8997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1026000,
        args: {'layerTreeId': 17},
        'dur': 3999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1032000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1032001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1046001,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1033000,
        args: {},
        'dur': 12999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1033001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1033002,
        args: {'layerTreeId': 17},
        'dur': 7999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1041002,
        args: {},
        'dur': 4997,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'BeginFrame',
        'ts': 1048000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'Program',
        'ts': 1049000,
        args: {},
        'dur': 999,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
    'Idle frames': [
      {
        'name': 'BeginFrame',
        'ts': 984000,
        args: {'layerTreeId': 17, 'frameSeqId': 99},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1000000,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1014000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1014001,
        args: {'layerTreeId': 17, 'frameSeqId': 100},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'NeedsBeginFrameChanged',
        'ts': 1015000,
        args: {'layerTreeId': 17, 'data': {'needsBeginFrame': false}},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'NeedsBeginFrameChanged',
        'ts': 1215000,
        args: {'layerTreeId': 17, 'data': {'needsBeginFrame': true}},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1231000,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1247000,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginFrame',
        'ts': 1263000,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'RequestMainThreadFrame',
        'ts': 1263001,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },

      {
        'name': 'Program',
        'ts': 1264000,
        args: {},
        'dur': 24000,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'BeginMainThreadFrame',
        'ts': 1264001,
        args: {},
        'ph': 'I',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'FunctionCall',
        'ts': 1264002,
        args: {},
        'dur': 23000,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'CompositeLayers',
        'ts': 1287003,
        args: {'layerTreeId': 17},
        'dur': 500,
        'ph': 'X',
        'tid': mainThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1270000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1270001,
        args: {'layerTreeId': 17, 'frameSeqId': 102},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'ActivateLayerTree',
        'ts': 1296000,
        args: {'layerTreeId': 17},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1296001,
        args: {'layerTreeId': 17, 'frameSeqId': 103},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
      {
        'name': 'DrawFrame',
        'ts': 1312000,
        args: {'layerTreeId': 17, 'frameSeqId': 104},
        'ph': 'I',
        'tid': implThread,
        'pid': 100,
        'cat': 'disabled-by-default-devtools.timeline'
      },
    ],
  };

  for (var testName in testData) {
    var data = testData[testName];
    var performanceModel = await PerformanceTestRunner.createPerformanceModelWithEvents(commonMetadata.concat(data));
    TestRunner.addResult('Test: ' + testName);

    for (var frame of performanceModel.frameModel().getFrames()) {
      TestRunner.addResult(TimelineModule.TimelineUIUtils.TimelineUIUtils.frameDuration(frame).textContent);
      PerformanceTestRunner.dumpFrame(frame);
    }
  }
  TestRunner.completeTest();
})();
