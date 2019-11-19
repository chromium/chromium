// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests the graph model.\n`);

  await TestRunner.showPanel('web-audio');

  const contextId = 'contextId';
  const graph = new WebAudio.GraphVisualizer.GraphView(contextId);

  TestRunner.addResult('Original lengths');
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest adding node');
  const nodeData1 = {
    nodeId: 'node1',
    nodeType: 'Gain',
    numberOfInputs: 1,
    numberOfOutputs: 1
  };
  graph.addNode(nodeData1);
  // TestRunner.addResult(`Graph is dirty: ${graph.isDirty()}`);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest removing node with no edges');
  graph.removeNode(nodeData1.nodeId);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest adding node-to-node connection');
  const nodeData2 = {
    nodeId: 'node2',
    nodeType: 'BiquadFilter',
    numberOfInputs: 1,
    numberOfOutputs: 1
  };
  graph.addNode(nodeData1);
  graph.addNode(nodeData2);
  const edgeData1 = {
    sourceId: nodeData1.nodeId,
    destinationId: nodeData2.nodeId,
    sourceOutputIndex: 0,
    destinationInputIndex: 0,
  };
  graph.addNodeToNodeConnection(edgeData1);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest removing node-to-node connection');
  graph.removeNodeToNodeConnection({
    sourceId: nodeData1.nodeId,
    destinationId: nodeData2.nodeId,
  });
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest removing node with edges');
  graph.addNodeToNodeConnection(edgeData1);
  graph.removeNode(nodeData1.nodeId);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();
  TestRunner.addResult('Add the node back for other tests.')
  graph.addNode(nodeData1);

  TestRunner.addResult('\nTest adding param');
  const paramData1 = {
    paramId: 'param1',
    paramType: 'detune',
    nodeId: nodeData2.nodeId,
  };
  graph.addParam(paramData1);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest adding node-to-param connection');
  const edgeData2 = {
    sourceId: nodeData1.nodeId,
    destinationId: nodeData2.nodeId,
    sourceOutputIndex: 0,
    destinationParamId: paramData1.paramId,
  };
  graph.addNodeToParamConnection(edgeData2);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.addResult('\nTest removing node-to-param connection');
  graph.removeNodeToParamConnection(edgeData2);
  dumpNodeEdgeSize();
  dumpInOutBoundingMapSize();

  TestRunner.completeTest();

  function dumpNodeEdgeSize() {
    TestRunner.addResult(`Number of nodes: ${graph.getNodes().size}`);
    TestRunner.addResult(`Number of edges: ${graph.getEdges().size}`);
  }

  function dumpInOutBoundingMapSize() {
    TestRunner.addResult(`Number of nodes with out-bound edges: ${graph._outboundEdgeMap.size}`);
    TestRunner.addResult(`Number of nodes with in-bound edges: ${graph._inboundEdgeMap.size}`);
  }

})();