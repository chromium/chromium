
import {TestRunner} from 'test_runner';

import * as DataGrid from 'devtools/ui/legacy/components/data_grid/data_grid.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult("This tests viewport datagrid.");

  var div = document.createElement("div");
  UI.InspectorView.InspectorView.instance().element.appendChild(div);

  var columns = [{id: "id", width: "250px", sortable: false}];
  var dataGrid = new DataGrid.ViewportDataGrid.ViewportDataGrid({displayName: 'Test', columns});
  var widget = dataGrid.asWidget();
  widget.show(div);
  dataGrid.element.style.width = '100%';
  dataGrid.element.style.height = '150px';
  widget.element.style.width = '100%';
  widget.element.style.height = '100%';

  var rootNode = dataGrid.rootNode();
  var nodes = [];

  for (var i = 0; i <= 25; i++) {
    var node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + i});
    rootNode.appendChild(node);
    nodes.push(node);
  }

  TestRunner.addResult("Nodes 3 and 24 have children but should be collapsed initially");

  node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + 3 + "." + 0});
  nodes.push(node);
  nodes[3].appendChild(node);
  node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + 3 + "." + 1});
  nodes.push(node);
  nodes[3].appendChild(node);
  node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + 3 + "." + 2});
  nodes.push(node);
  nodes[3].appendChild(node);

  node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + 24 + "." + 0});
  nodes.push(node);
  nodes[24].appendChild(node);
  node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + 24 + "." + 1});
  nodes.push(node);
  nodes[24].appendChild(node);

  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Expanding Node 3 and Node 24");
  nodes[3].expand();
  nodes[24].expand();
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Collapsing Node 3");
  nodes[3].collapse();
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 220px");
  setScrollPosition(220);
  dataGrid.update();
  TestRunner.addResult("Expanding Node 3 while not in dom");
  nodes[3].expand();
  dataGrid.update();
  TestRunner.addResult("Scrolled back up to 0px");
  setScrollPosition(0);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Moving node 0 to be a child of node 3 to make sure attributes adjust (name does not change)");
  nodes[3].insertChild(nodes[0], 0);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Moving node that is attached to dom (node 0) to child of offscreen parent (node 24)");
  nodes[24].insertChild(nodes[0], 0);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolling down to 1000px - should be at bottom to make sure node 0 is attached properly to node 24");
  setScrollPosition(1000);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.completeTest();

  function setScrollPosition(yPosition) {
    dataGrid.scrollContainer.scrollTop = yPosition;
    dataGrid.onScroll();
  }

  function dumpVisibleNodes() {
    TestRunner.addResult("Nodes attached to dom:");
    for (var node of rootNode.flatChildren()) {
      var element = node.existingElement();
      if (!element)
        continue;
      if (div.contains(element)) {
        var attributes = [];
        if (node.hasChildren())
          attributes.push("has children");
        if (node.depth)
          attributes.push("depth: " + node.depth);
        if (node.expanded)
          attributes.push("expanded");
        TestRunner.addResult(node.data.id + " " + attributes.join(', '));
      }
    }
    TestRunner.addResult("");
  }
})();
