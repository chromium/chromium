
import {TestRunner} from 'test_runner';

import * as DataGrid from 'devtools/ui/legacy/components/data_grid/data_grid.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult("This tests viewport datagrid.");

  var div = document.createElement("div");
  UI.InspectorView.InspectorView.instance().element.appendChild(div);

  var columns = [{id: "id", title: "ID column", width: "250px"}];
  var dataGrid = new DataGrid.ViewportDataGrid.ViewportDataGrid({displayName: 'Test', columns});
  var widget = dataGrid.asWidget();
  widget.show(div);
  dataGrid.element.style.width = '100%';
  dataGrid.element.style.height = '150px';
  widget.element.style.width = '100%';
  widget.element.style.height = '100%';

  var rootNode = dataGrid.rootNode();
  var nodes = [];

  for (var i = 0; i < 30; i++) {
    var node = new DataGrid.ViewportDataGrid.ViewportDataGridNode({id: "a" + i});
    rootNode.appendChild(node);
    nodes.push(node);
  }

  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 133px");
  setScrollPosition(133);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 312px");
  setScrollPosition(312);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 1000px - should be at bottom");
  setScrollPosition(1000);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled up to 0px");
  setScrollPosition(0);
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Testing removal of some nodes in viewport");
  nodes[0].remove();
  nodes[1].remove();
  nodes[3].remove();
  nodes[5].remove();

  // TODO(allada) Removal of nodes is not throttled in ViewportDataGrid yet.
  // TestRunner.addResult("Nodes should be the same as previous dump because of throttling:");
  // dumpVisibleNodes();

  TestRunner.addResult("Should be missing node 0, 1, 3, 5 from dom:");
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.addResult("Testing adding of some nodes back into viewport");
  rootNode.insertChild(nodes[0], 0);
  rootNode.insertChild(nodes[1], 1);
  rootNode.insertChild(nodes[3], 3);
  rootNode.insertChild(nodes[5], 5);

  TestRunner.addResult("Nodes should be the same as previous dump because of throttling:");
  dumpVisibleNodes();

  TestRunner.addResult("Should have nodes 0, 1, 3, 5 back in dom and previously added nodes removed:");
  dataGrid.update();
  dumpVisibleNodes();

  TestRunner.completeTest();



  function setScrollPosition(yPosition) {
    dataGrid.scrollContainer.scrollTop = yPosition;
    dataGrid.onScroll();
  }

  function dumpVisibleNodes() {
    TestRunner.addResult("Nodes attached to dom:");
    for (var node of nodes) {
      var element = node.existingElement();
      if (!element)
        continue;
      if (div.contains(element))
        TestRunner.addResult(node.data.id);
    }
    TestRunner.addResult("");
  }
})();
