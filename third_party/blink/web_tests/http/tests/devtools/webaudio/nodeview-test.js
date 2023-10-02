// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as WebAudioModule from 'devtools/panels/web_audio/web_audio.js';

(async function() {
  TestRunner.addResult(`Tests the node model.\n`);

  await TestRunner.showPanel('web-audio');

  const nodeData1 = {
    nodeId: 'node1',
    nodeType: 'Gain',
    numberOfInputs: 1,
    numberOfOutputs: 1
  };
  const node = new WebAudioModule.NodeView.NodeView(nodeData1, 'nodeLabel');

  TestRunner.addResult('Original lengths');
  dumpNumberOfPorts();

  TestRunner.addResult('\nTesting node size');
  const size = node.size;
  const height = size.height;
  TestRunner.addResult(`width greater than 0: ${size.width > 0}`);
  TestRunner.addResult(`height greater than 0: ${size.height > 0}`);

  TestRunner.addResult('\nTest adding param port');
  const paramId = 'paramId';
  const paramType = 'Gain';
  node.addParamPort(paramId, paramType);
  dumpNumberOfPorts();
  const newSize = node.size;
  TestRunner.addResult(`height increases: ${newSize.height > height}`);

  TestRunner.completeTest();

  function dumpNumberOfPorts() {
    TestRunner.addResult(`Number of ports: ${node.ports.size}`);
  }

})();