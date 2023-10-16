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

  function dumpFlattenChildren() {
    var nodes = dataGrid.rootNode().flatChildren();
    TestRunner.addResult('Checking flatChildren():');
    for (var node of nodes)
      TestRunner.addResult('  ' + node.data.id);
    TestRunner.addResult('');
  }

  function revealChildAndDumpClassAndVisibleNodes(index) {
    root.children[index].revealAndSelect();
    dataGrid.updateInstantly();

    TestRunner.addResult(`Class list: ${dataGrid.element.classList}`);

    for (var node of dataGrid.visibleNodes)
      TestRunner.addResult(node.data.id);
  }

  var columns = [{id: 'id', width: '250px', sortable: false}];
  var dataGrid = new DataGrid.ViewportDataGrid.ViewportDataGrid({displayName: 'Test', columns});
  var a = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'a'});
  var aa = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'aa'});
  var aaa = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'aaa'});
  var aab = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'aab'});
  var ab = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'ab'});
  var b = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'b'});

  var root = dataGrid.rootNode();
  var widget = dataGrid.asWidget();
  widget.markAsRoot();

  var containerElement = document.body.createChild('div');
  containerElement.style.position = 'absolute';
  containerElement.style.width = '300px';
  containerElement.style.height = '300px';
  containerElement.style.overflow = 'hidden';
  widget.show(containerElement);
  dataGrid.element.style.width = '100%';
  dataGrid.element.style.height = '100%';
  widget.element.style.width = '100%';
  widget.element.style.height = '100%';

  TestRunner.addResult('Building tree.');

  // Appending to detached node.
  attach(aa, aaa);
  aaa.dataGrid = dataGrid;
  attach(aa, aab);
  aab.dataGrid = dataGrid;

  // Appending to tree.
  attach(root, a);
  // a is collapsed.
  attach(a, aa);
  expand(a);
  dumpFlattenChildren();
  attach(a, ab);
  dumpFlattenChildren();
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
  var aac = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'aac'});
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
  dumpFlattenChildren();
  TestRunner.addResult('Removed children of aa');
  aa.removeChildren();
  dumpFlattenChildren();
  dumpNodes();
  attach(ab, aac);
  aac.revealAndSelect();
  dumpFlattenChildren();
  detach(a, aa);
  dumpFlattenChildren();
  dumpNodes();
  detach(a, ab);
  dumpNodes();
  root.removeChildren();
  dumpNodes();

  // crbug.com/542553 -- the below should not produce exceptions.
  dataGrid.setStickToBottom(true);
  for (var i = 0; i < 500; ++i) {
    var xn = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'x' + i});
    root.appendChild(xn);
    if (i + 1 === 500) {
      dataGrid.updateInstantly();
      xn.revealAndSelect();
      xn.refresh();
    }
  }
  root.removeChildren();
  dataGrid.updateInstantly();

  // The below should not crash either.
  for (var i = 0; i < 40; ++i) {
    var xn = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: 'x' + i});
    root.appendChild(xn);
  }
  dataGrid.updateInstantly();
  dataGrid.setStickToBottom(false);
  var children = root.children.slice();
  root.removeChildren();
  // Assure wheelTarget is anything but null, otherwise it happily bypasses crashing code.
  dataGrid.wheelTarget = children[children.length - 1].element;
  for (var i = 0; i < 40; ++i) {
    children[i].refresh();
    root.appendChild(children[i]);
  }
  dataGrid.updateInstantly();

  dataGrid.setStriped(true);
  TestRunner.addResult('Scrolling to the top');
  revealChildAndDumpClassAndVisibleNodes(0);
  TestRunner.addResult('Scrolling 1 node down');
  revealChildAndDumpClassAndVisibleNodes(dataGrid.visibleNodes.length);
  TestRunner.addResult('Disabling the stripes');
  dataGrid.setStriped(false);
  TestRunner.addResult(`Class list: ${dataGrid.element.classList}`);

  TestRunner.completeTest();
})();
