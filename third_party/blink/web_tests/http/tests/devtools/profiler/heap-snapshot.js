// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

import * as ProfilerModule from 'devtools/panels/profiler/profiler.js';

(async function() {
  TestRunner.addResult(`This test checks HeapSnapshots module.\n`);
  await TestRunner.showPanel('heap-profiler');

  function createTestEnvironmentInWorker() {
    if (!this.TestRunner)
      TestRunner = {};

    if (!this.HeapProfilerTestRunner)
      HeapProfilerTestRunner = {};

    TestRunner.assertEquals = function(expected, found, message) {
      if (expected === found)
        return;

      var error;
      if (message)
        error = 'Failure (' + message + '):';
      else
        error = 'Failure:';
      throw new Error(error + ' expected <' + expected + '> found <' + found + '>');
    };
  }

  function runTestSuiteInWorker() {
    var testSuite = [
      function heapSnapshotNodeSimpleTest() {
        var snapshot = HeapProfilerTestRunner.createJSHeapSnapshotMockObject();
        var nodeRoot = snapshot.createNode(snapshot.rootNodeIndex);
        TestRunner.assertEquals('', nodeRoot.name(), 'root name');
        TestRunner.assertEquals('hidden', nodeRoot.type(), 'root type');
        TestRunner.assertEquals(2, nodeRoot.edgesCount(), 'root edges');
        var nodeE = snapshot.createNode(15);
        TestRunner.assertEquals('E', nodeE.name(), 'E name');
        TestRunner.assertEquals('object', nodeE.type(), 'E type');
        TestRunner.assertEquals(0, nodeE.edgesCount(), 'E edges');
      },

      function heapSnapshotNodeIteratorTest() {
        var snapshot = HeapProfilerTestRunner.createJSHeapSnapshotMockObject();
        var nodeRoot = snapshot.createNode(snapshot.rootNodeIndex);
        var iterator = new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotNodeIterator(nodeRoot);
        var names = [];
        for (; iterator.hasNext(); iterator.next())
          names.push(iterator.item().name());
        TestRunner.assertEquals(',A,B,C,D,E', names.join(','), 'node iterator');
      },

      function heapSnapshotEdgeSimpleTest() {
        var snapshot = HeapProfilerTestRunner.createJSHeapSnapshotMockObject();
        var nodeRoot = snapshot.createNode(snapshot.rootNodeIndex);
        var edgeIterator = new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotEdgeIterator(nodeRoot);
        TestRunner.assertEquals(true, edgeIterator.hasNext(), 'has edges');
        var edge = edgeIterator.item();
        TestRunner.assertEquals('shortcut', edge.type(), 'edge type');
        TestRunner.assertEquals('a', edge.name(), 'edge name');
        TestRunner.assertEquals('A', edge.node().name(), 'edge node name');

        var edgesCount = 0;
        for (; edgeIterator.hasNext(); edgeIterator.next())
          ++edgesCount;
        TestRunner.assertEquals(nodeRoot.edgesCount(), edgesCount, 'edges count');
      },

      function heapSnapshotEdgeIteratorTest() {
        var snapshot = HeapProfilerTestRunner.createJSHeapSnapshotMockObject();
        var nodeRoot = snapshot.createNode(snapshot.rootNodeIndex);
        var names = [];
        for (var iterator = nodeRoot.edges(); iterator.hasNext(); iterator.next())
          names.push(iterator.item().name());
        TestRunner.assertEquals('a,b', names.join(','), 'edge iterator');
        var nodeE = snapshot.createNode(15);
        TestRunner.assertEquals(false, nodeE.edges().hasNext(), 'empty edge iterator');
      },

      function heapSnapshotNodeAndEdgeTest() {
        var snapshotMock = HeapProfilerTestRunner.createJSHeapSnapshotMockObject();
        var nodeRoot = snapshotMock.createNode(snapshotMock.rootNodeIndex);
        var names = [];

        function depthFirstTraversal(node) {
          names.push(node.name());
          for (var edges = node.edges(); edges.hasNext(); edges.next()) {
            names.push(edges.item().name());
            depthFirstTraversal(edges.item().node());
          }
        }

        depthFirstTraversal(nodeRoot);
        var reference = ',a,A,1,B,bc,C,ce,E,bd,D,ac,C,ce,E,b,B,bc,C,ce,E,bd,D';
        TestRunner.assertEquals(reference, names.join(','), 'mock traversal');

        // Now check against a real HeapSnapshot instance.
        names = [];
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        depthFirstTraversal(snapshot.rootNode());
        TestRunner.assertEquals(reference, names.join(','), 'snapshot traversal');
      },

      function heapSnapshotSimpleTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        TestRunner.assertEquals(6, snapshot.nodeCount, 'node count');
        TestRunner.assertEquals(20, snapshot.totalSize, 'total size');
      },

      function heapSnapshotContainmentEdgeIndexesTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        var actual = snapshot.firstEdgeIndexes;
        var expected = [0, 6, 12, 18, 21, 21, 21];
        TestRunner.assertEquals(expected.length, actual.length, 'Edge indexes size');
        for (var i = 0; i < expected.length; ++i)
          TestRunner.assertEquals(expected[i], actual[i], 'Edge indexes');
      },

      function heapSnapshotDominatorsTreeTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        var dominatorsTree = snapshot.dominatorsTree;
        var expected = [0, 0, 0, 0, 2, 3];
        for (var i = 0; i < expected.length; ++i)
          TestRunner.assertEquals(expected[i], dominatorsTree[i], 'Dominators Tree');
      },

      function heapSnapshotLocations() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        const expected = new Map([
          [0, new HeapSnapshotModel.HeapSnapshotModel.Location(1, 2, 3)],
          [18, new HeapSnapshotModel.HeapSnapshotModel.Location(2, 3, 4)],
        ]);

        expected.forEach((expected_location, index) => {
          const location = snapshot.getLocation(index);
          TestRunner.assertEquals(expected_location.scriptId, location.scriptId, 'Locations scriptId');
          TestRunner.assertEquals(expected_location.lineNumber, location.lineNumber, 'Locations lineNumber');
          TestRunner.assertEquals(expected_location.columnNumber, location.columnNumber, 'Locations columnNumber');
        });
      },

      function heapSnapshotRetainedSizeTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        var actualRetainedSizes = new Array(snapshot.nodeCount);
        for (var nodeOrdinal = 0; nodeOrdinal < snapshot.nodeCount; ++nodeOrdinal)
          actualRetainedSizes[nodeOrdinal] = snapshot.retainedSizes[nodeOrdinal];
        var expectedRetainedSizes = [20, 2, 8, 10, 5, 6];
        TestRunner.assertEquals(
            JSON.stringify(expectedRetainedSizes), JSON.stringify(actualRetainedSizes), 'Retained sizes');
      },

      function heapSnapshotLargeRetainedSize(next) {
        var builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
        var node = builder.rootNode;

        var iterations = 6;
        var nodeSize = 1000 * 1000 * 1000;
        for (var i = 0; i < 6; i++) {
          var newNode = new HeapProfilerTestRunner.HeapNode('Node' + i, nodeSize);
          node.linkNode(newNode, HeapProfilerTestRunner.HeapEdge.Type.element);
          node = newNode;
        }

        var snapshot = builder.createJSHeapSnapshot();
        TestRunner.assertEquals(
            iterations * nodeSize, snapshot.rootNode().retainedSize(),
            'Ensure that root node retained size supports values exceeding 2^32 bytes.');
      },

      function heapSnapshotDominatedNodesTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());

        var expectedDominatedNodes = [21, 14, 7, 28, 35];
        var actualDominatedNodes = snapshot.dominatedNodes;
        TestRunner.assertEquals(expectedDominatedNodes.length, actualDominatedNodes.length, 'Dominated Nodes length');
        for (var i = 0; i < expectedDominatedNodes.length; ++i)
          TestRunner.assertEquals(expectedDominatedNodes[i], actualDominatedNodes[i], 'Dominated Nodes');

        var expectedDominatedNodeIndex = [0, 3, 3, 4, 5, 5, 5];
        var actualDominatedNodeIndex = snapshot.firstDominatedNodeIndex;
        TestRunner.assertEquals(
            expectedDominatedNodeIndex.length, actualDominatedNodeIndex.length, 'Dominated Nodes Index length');
        for (var i = 0; i < expectedDominatedNodeIndex.length; ++i)
          TestRunner.assertEquals(expectedDominatedNodeIndex[i], actualDominatedNodeIndex[i], 'Dominated Nodes Index');
      },

      function heapSnapshotPageOwnedTest(next) {
        var builder = new HeapProfilerTestRunner.HeapSnapshotBuilder();
        var rootNode = builder.rootNode;

        var debuggerNode = new HeapProfilerTestRunner.HeapNode('Debugger');
        rootNode.linkNode(debuggerNode, HeapProfilerTestRunner.HeapEdge.Type.element);

        var windowNode = new HeapProfilerTestRunner.HeapNode('Window');
        rootNode.linkNode(windowNode, HeapProfilerTestRunner.HeapEdge.Type.shortcut);

        var pageOwnedNode = new HeapProfilerTestRunner.HeapNode('PageOwnedNode');
        windowNode.linkNode(pageOwnedNode, HeapProfilerTestRunner.HeapEdge.Type.element);
        debuggerNode.linkNode(pageOwnedNode, HeapProfilerTestRunner.HeapEdge.Type.property, 'debugger2pageOwnedNode');

        var debuggerOwnedNode = new HeapProfilerTestRunner.HeapNode('debuggerOwnedNode');
        debuggerNode.linkNode(debuggerOwnedNode, HeapProfilerTestRunner.HeapEdge.Type.element);

        var snapshot = builder.createJSHeapSnapshot();
        snapshot.flags = new Array(snapshot.nodeCount);
        for (var i = 0; i < snapshot.nodeCount; ++i)
          snapshot.flags[i] = 0;
        snapshot.markPageOwnedNodes();

        var expectedFlags = [0, 0, 4, 4, 0];
        TestRunner.assertEquals(
            JSON.stringify(expectedFlags), JSON.stringify(snapshot.flags),
            'We are expecting that only window(third element) and PageOwnedNode(forth element) have flag === 4.');
      },

      function heapSnapshotRetainersTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        var expectedRetainers = {'': [], 'A': [''], 'B': ['', 'A'], 'C': ['A', 'B'], 'D': ['B'], 'E': ['C']};
        for (var nodes = snapshot.allNodes(); nodes.hasNext(); nodes.next()) {
          var names = [];
          for (var retainers = nodes.item().retainers(); retainers.hasNext(); retainers.next())
            names.push(retainers.item().node().name());
          names.sort();
          TestRunner.assertEquals(
              expectedRetainers[nodes.item().name()].join(','), names.join(','),
              'retainers of "' + nodes.item().name() + '"');
        }
      },

      function heapSnapshotAggregatesTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        var expectedAggregates = {
          'A': {count: 1, self: 2, maxRet: 2, type: 'object', name: 'A'},
          'B': {count: 1, self: 3, maxRet: 8, type: 'object', name: 'B'},
          'C': {count: 1, self: 4, maxRet: 10, type: 'object', name: 'C'},
          'D': {count: 1, self: 5, maxRet: 5, type: 'object', name: 'D'},
          'E': {count: 1, self: 6, maxRet: 6, type: 'object', name: 'E'}
        };
        var aggregates = snapshot.getAggregatesByClassName(false);
        for (var name in aggregates) {
          var aggregate = aggregates[name];
          var expectedAggregate = expectedAggregates[name];
          for (var parameter in expectedAggregate)
            TestRunner.assertEquals(
                expectedAggregate[parameter], aggregate[parameter], 'parameter ' + parameter + ' of "' + name + '"');
        }
        var expectedIndexes = {
          // Index of corresponding node in the raw snapshot:
          'A': [7],   // 14
          'B': [14],  // 27
          'C': [21],  // 40
          'D': [28],  // 50
          'E': [35]   // 57
        };
        var indexes = snapshot.getAggregatesByClassName(true);
        for (var name in aggregates) {
          var aggregate = aggregates[name];
          var expectedIndex = expectedIndexes[name];
          TestRunner.assertEquals(expectedIndex.join(','), aggregate.idxs.join(','), 'indexes of "' + name + '"');
        }
      },

      function heapSnapshotFlagsTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMockWithDOM(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());
        var expectedCanBeQueried = {
          '': false,
          'A': true,
          'B': true,
          'C': true,
          'D': true,
          'E': false,
          'F': false,
          'G': false,
          'H': false,
          'M': false,
          'N': false,
          'Window': true
        };
        for (var nodes = snapshot.allNodes(); nodes.hasNext(); nodes.next()) {
          var node = nodes.item();
          TestRunner.assertEquals(
              expectedCanBeQueried[node.name()], node.canBeQueried(), 'canBeQueried of "' + node.name() + '"');
        }
      },

      function heapSnapshotNodesProviderTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());

        var allNodeIndexes = [];
        for (var i = 0; i < snapshot.nodes.length; i += snapshot.nodeFieldCount)
          allNodeIndexes.push(i);
        var provider = new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotNodesProvider(snapshot, allNodeIndexes);
        // Sort by names in reverse order.
        provider.sortAndRewind({fieldName1: 'name', ascending1: false, fieldName2: 'id', ascending2: false});
        var range = provider.serializeItemsRange(0, 6);
        TestRunner.assertEquals(6, range.totalLength, 'Node range total length');
        TestRunner.assertEquals(0, range.startPosition, 'Node range start position');
        TestRunner.assertEquals(6, range.endPosition, 'Node range end position');
        var names = range.items.map(item => item.name);
        TestRunner.assertEquals('E,D,C,B,A,', names.join(','), 'nodes provider names');
      },

      function heapSnapshotEdgesProviderTest() {
        var snapshot = new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
            HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress());

        function edgeFilter(edge) {
          return edge.name() === 'b';
        }

        var provider = snapshot.createEdgesProviderForTest(snapshot.rootNodeIndex, edgeFilter);
        provider.sortAndRewind({fieldName1: '!edgeName', ascending1: false, fieldName2: 'id', ascending2: false});
        var range = provider.serializeItemsRange(0, 10);
        TestRunner.assertEquals(1, range.totalLength, 'Edge range total length');
        TestRunner.assertEquals(0, range.startPosition, 'Edge range start position');
        TestRunner.assertEquals(1, range.endPosition, 'Edge range end position');
        var names = range.items.map(function(item) {
          return item.name;
        });
        TestRunner.assertEquals('b', names.join(','), 'edges provider names');
      },

      async function heapSnapshotLoaderTest() {
        var source = HeapProfilerTestRunner.createHeapSnapshotMockRaw();
        var sourceStringified = JSON.stringify(source);
        var partSize = sourceStringified.length >> 3;

        var loader = new HeapSnapshotWorker.HeapSnapshotLoader.HeapSnapshotLoader();
        for (var i = 0, l = sourceStringified.length; i < l; i += partSize)
          loader.write(sourceStringified.slice(i, i + partSize));
        loader.close();
        await 0;  // Make sure loader parses the input.
        var result = loader.buildSnapshot(false);
        result.nodes = new Uint32Array(result.nodes);
        result.containmentEdges = new Uint32Array(result.containmentEdges);
        function assertSnapshotEquals(reference, actual) {
          TestRunner.assertEquals(JSON.stringify(reference), JSON.stringify(actual));
        }
        assertSnapshotEquals(
            new HeapSnapshotWorker.HeapSnapshot.JSHeapSnapshot(
                HeapProfilerTestRunner.createHeapSnapshotMock(), new HeapSnapshotWorker.HeapSnapshot.HeapSnapshotProgress(), false),
            result);
      },
    ];

    var result = [];
    for (var i = 0; i < testSuite.length; i++) {
      var test = testSuite[i];
      result.push('');
      result.push('Running: ' + test.name);
      try {
        test();
      } catch (e) {
        result.push('FAIL: ' + e);
      }
    }
    return result.join('\n');
  }

  var proxy = new ProfilerModule.HeapSnapshotProxy.HeapSnapshotWorkerProxy(function(eventName, arg) {
    TestRunner.addResult('Unexpected event from worker: ' + eventName);
  });
  var source = '(' + createTestEnvironmentInWorker + ')();' +
      '(' + HeapProfilerTestRunner.createHeapSnapshotMockFactories + ')();' +
      '(' + runTestSuiteInWorker + ')();';
  proxy.evaluateForTest(source, function(result) {
    TestRunner.addResult(result);
    TestRunner.completeTest();
  });
})();
