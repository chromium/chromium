// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {DataGridTestRunner} from 'data_grid_test_runner';

import * as DataGrid from 'devtools/ui/legacy/components/data_grid/data_grid.js';

(async function() {
  TestRunner.addResult(`Tests ViewportDataGrid.\n`);

  function attach(parent, child, index) {
    var parentName = parent === root ? 'root' : parent.data.id;
    if (typeof index === 'number')
      parent.insertChild(child, index);
    else
      parent.appendChild(child);
    TestRunner.addResult(
        'Attached ' + child.data.id + ' to ' + parentName + (typeof index === 'number' ? ' at ' + index : ''));
  }

  function detach(parent, child) {
    var parentName = parent === root ? 'root' : parent.data.id;
    TestRunner.addResult('Removing ' + child.data.id + ' from ' + parentName);
    parent.removeChild(child);
  }

  function dumpNodes() {
    TestRunner.addResult('');
    TestRunner.addResult('Tree:');
    DataGridTestRunner.dumpAndValidateDataGrid(dataGrid.rootNode());
    TestRunner.addResult('');
  }

  function expand(node) {
    node.expand();
    TestRunner.addResult('Expanded node ' + node.data.id);
  }

  function collapse(node) {
    node.collapse();
    TestRunner.addResult('Collapsed node ' + node.data.id);
  }

  var columns = [{id: 'id', sortable: false}];
  var dataGrid = new DataGrid.DataGrid.DataGridImpl({displayName: 'Test', columns});
  var a = new DataGrid.DataGrid.DataGridNode({id: 'a'});
  var aa = new DataGrid.DataGrid.DataGridNode({id: 'aa'});
  var aaa = new DataGrid.DataGrid.DataGridNode({id: 'aaa'});
  var aab = new DataGrid.DataGrid.DataGridNode({id: 'aab'});
  var ab = new DataGrid.DataGrid.DataGridNode({id: 'ab'});
  var b = new DataGrid.DataGrid.DataGridNode({id: 'b'});

  var root = dataGrid.rootNode();

  TestRunner.addResult(dataGrid.element.classList);
  dataGrid.setStriped(true);
  TestRunner.addResult(dataGrid.element.classList);
  dataGrid.setStriped(false);
  TestRunner.addResult(dataGrid.element.classList);

  TestRunner.addResult('Building tree.');

  // Appending to detached node.
  attach(aa, aaa);
  aaa.dataGrid = dataGrid;
  attach(aa, aab);
  aab.dataGrid = dataGrid;

  // Appending to tree.
  attach(root, a);
  attach(a, aa);
  attach(a, ab);
  attach(root, b);

  dumpNodes();

  expand(a);
  dumpNodes();

  expand(aa);
  dumpNodes();

  collapse(a);
  dumpNodes();

  expand(a);
  attach(aa, aaa);
  dumpNodes();
  attach(aa, aaa);
  attach(aa, aab);
  var aac = new DataGrid.DataGrid.DataGridNode({id: 'aac'});
  attach(aa, aac);
  dumpNodes();
  attach(aa, aac, 0);
  dumpNodes();
  attach(ab, aac);
  expand(ab);
  aac.select();
  dumpNodes();
  detach(ab, aac);
  dumpNodes();
  attach(ab, aac);
  aac.revealAndSelect();
  aa.removeChildren();
  dumpNodes();
  attach(ab, aac);
  aac.revealAndSelect();
  detach(a, aa);
  dumpNodes();
  detach(a, ab);
  dumpNodes();
  root.removeChildren();
  dumpNodes();

  var columns = [{id: 'id', sortable: false}];
  var dataGrid = new DataGrid.DataGrid.DataGridImpl({displayName: 'Test', columns});
  var a = new DataGrid.DataGrid.DataGridNode({id: 'TextData', secondCol: 'a foo'});
  var b = new DataGrid.DataGrid.DataGridNode({id: 'NullData', secondCol: null});
  var root = dataGrid.rootNode();
  attach(root, a);
  dumpNodes();
  dataGrid.addColumn({id: 'secondCol', sortable: false});
  TestRunner.addResult('Added secondCol');
  dumpNodes();
  attach(root, b);
  dataGrid.autoSizeColumns(20, 80);
  dumpNodes();
  dataGrid.removeColumn('secondCol');
  TestRunner.addResult('Removed secondCol');
  dumpNodes();

  TestRunner.completeTest();
})();
