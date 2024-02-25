
import {TestRunner} from 'test_runner';

import * as DataGrid from 'devtools/ui/legacy/components/data_grid/data_grid.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult("This tests viewport datagrid.");

  var div = document.createElement("div");
  UI.InspectorView.InspectorView.instance().element.appendChild(div);

  var columns = [{id: "id", width: "250px", sortable: false}];
  var dataGrid = new DataGrid.DataGrid.DataGridImpl({displayName: 'Test', columns});
  div.appendChild(dataGrid.element);
  dataGrid.element.style.height = '150px';

  var rootNode = dataGrid.rootNode();
  var nodes = [];

  for (var i = 0; i < 15; i++) {
    var node = new DataGrid.DataGrid.DataGridNode({id: "a" + i});
    rootNode.appendChild(node);
    nodes.push(node);
  }

  dumpVisibleNodes();

  TestRunner.addResult("Testing removal of some nodes");
  nodes[0].remove();
  nodes[1].remove();
  nodes[3].remove();
  nodes[5].remove();

  TestRunner.addResult("Should be missing node 0, 1, 3, 5 from dom:");
  dumpVisibleNodes();

  TestRunner.addResult("Testing adding of some nodes back");
  rootNode.insertChild(nodes[0], 0);
  rootNode.insertChild(nodes[1], 1);
  rootNode.insertChild(nodes[3], 3);
  rootNode.insertChild(nodes[5], 5);

  TestRunner.addResult("Should have nodes 0, 1, 3, 5 back in dom and previously added nodes removed:");
  dumpVisibleNodes();

  TestRunner.completeTest();

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
