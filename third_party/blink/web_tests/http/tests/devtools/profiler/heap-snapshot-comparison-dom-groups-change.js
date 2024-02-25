// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {HeapProfilerTestRunner} from 'heap_profiler_test_runner';

import * as ProfilerModule from 'devtools/panels/profiler/profiler.js';

(async function() {
  TestRunner.addResult(
      `Tests that Comparison view of heap snapshots will contain added nodes even if their ids are less than the maximumm JS object id in the base snapshot.\n`);
  await TestRunner.showPanel('heap-profiler');

  function createHeapSnapshotA() {
    // Represents the following graph:
    //  root - 0 -> <class = A, id = 10>
    //  root - 1 -> <class = A, id = 20>
    //  root - 2 -> <class = A, id = 30>
    //
    return {
      snapshot: {
        meta: {
          node_fields: ['type', 'name', 'id', 'self_size', 'retained_size', 'dominator', 'edge_count'],
          node_types: [['hidden', 'object', 'native'], '', '', '', '', '', ''],
          edge_fields: ['type', 'name_or_index', 'to_node'],
          edge_types: [['shortcut'], '', '']
        },
        node_count: 4,
        edge_count: 3
      },
      nodes: [0, 0, 1, 0, 21, 0, 3, 2, 1, 20, 7, 7, 0, 0, 2, 1, 30, 7, 7, 0, 0, 2, 1, 40, 7, 7, 0, 0],
      edges: [0, 0, 7, 0, 1, 14, 0, 2, 21],
      strings: ['', 'A']
    };
  }

  function createHeapSnapshotB() {
    // Represents the following graph:
    //  compared to snasphot A node 10 was deleted, nodes 5, 15, 25, 35 were added
    //
    //  root - 1 -> <class = A, id = 20>
    //  root - 2 -> <class = A, id = 30>
    //  root - 3 -> <class = A, id = 5>
    //  root - 4 -> <class = A, id = 15>
    //  root - 5 -> <class = A, id = 25>
    //  root - 6 -> <class = A, id = 35>
    //
    return {
      snapshot: {
        meta: {
          node_fields: ['type', 'name', 'id', 'self_size', 'retained_size', 'dominator', 'edge_count'],
          node_types: [['hidden', 'object', 'native'], '', '', '', '', '', ''],
          edge_fields: ['type', 'name_or_index', 'to_node'],
          edge_types: [['shortcut'], '', '']
        },
        node_count: 7,
        edge_count: 6
      },
      nodes: [
        0, 0, 1, 0, 42, 0,  6, 2, 1, 20, 7, 7, 0,  0, 2, 1, 30, 7, 7, 0,  0, 2, 1, 5, 7,
        7, 0, 0, 2, 1,  15, 7, 7, 0, 0,  2, 1, 25, 7, 7, 0, 0,  2, 1, 35, 7, 7, 0, 0
      ],
      edges: [0, 1, 7, 0, 2, 14, 0, 3, 21, 0, 4, 28, 0, 5, 35, 0, 6, 42],
      strings: ['', 'A']
    };
  }

  HeapProfilerTestRunner.runHeapSnapshotTestSuite([function testShowAll(next) {
    // Make sure all nodes are visible.
    ProfilerModule.HeapSnapshotDataGrids.HeapSnapshotDiffDataGrid.prototype.defaultPopulateCount = function() {
      return 100;
    };

    HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshotA, createSnapshotB);
    function createSnapshotB() {
      HeapProfilerTestRunner.takeAndOpenSnapshot(createHeapSnapshotB, step1);
    }

    function step1() {
      HeapProfilerTestRunner.switchToView('Comparison', step2);
    }

    function step2() {
      var row = HeapProfilerTestRunner.findRow('A');
      TestRunner.assertEquals(true, !!row, '"A" row');
      HeapProfilerTestRunner.expandRow(row, step3);
    }

    function step3(row) {
      TestRunner.addResult('Delta: +' + row.addedCount + ' -' + row.removedCount);
      var added = [];
      var removed = [];
      for (var i = 0; i < row.children.length; i++) {
        var child = row.children[i];
        if (child.isDeletedNode)
          removed.push(child.snapshotNodeId);
        else
          added.push(child.snapshotNodeId);
      }

      TestRunner.addResult('Deleted node id(s): ' + removed.sort());
      TestRunner.addResult('Added node id(s): ' + added.sort());
      next();
    }
  }]);
})();
