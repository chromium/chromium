(async function() {
  await TestRunner.loadModule('data_grid');

  TestRunner.addResult("This tests viewport datagrid.");

  var div = document.createElement("div");
  UI.inspectorView.element.appendChild(div);

  var columns = [{id: "id", title: "ID column", width: "250px"}];
  var dataGrid = new DataGrid.ViewportDataGrid({displayName: 'Test', columns});
  div.appendChild(dataGrid.element);
  dataGrid.element.style.height = '150px';

  var rootNode = dataGrid.rootNode();
  var nodes = [];

  for (var i = 0; i <= 25; i++) {
    var node = new DataGrid.ViewportDataGridNode({id: "a" + i});
    rootNode.appendChild(node);
    nodes.push(node);
  }

  TestRunner.addResult("Nodes 3 and 24 have children but should be collapsed initially");

  node = new DataGrid.ViewportDataGridNode({id: "a" + 3 + "." + 0});
  nodes.push(node);
  nodes[3].appendChild(node);
  node = new DataGrid.ViewportDataGridNode({id: "a" + 3 + "." + 1});
  nodes.push(node);
  nodes[3].appendChild(node);
  node = new DataGrid.ViewportDataGridNode({id: "a" + 3 + "." + 2});
  nodes.push(node);
  nodes[3].appendChild(node);

  node = new DataGrid.ViewportDataGridNode({id: "a" + 24 + "." + 0});
  nodes.push(node);
  nodes[24].appendChild(node);
  node = new DataGrid.ViewportDataGridNode({id: "a" + 24 + "." + 1});
  nodes.push(node);
  nodes[24].appendChild(node);

  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Expanding Node 3 and Node 24");
  nodes[3].expand();
  nodes[24].expand();
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Collapsing Node 3");
  nodes[3].collapse();
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 220px");
  setScrollPosition(220);
  dataGrid._update();
  TestRunner.addResult("Expanding Node 3 while not in dom");
  nodes[3].expand();
  dataGrid._update();
  TestRunner.addResult("Scrolled back up to 0px");
  setScrollPosition(0);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Moving node 0 to be a child of node 3 to make sure attributes adjust (name does not change)");
  nodes[3].insertChild(nodes[0], 0);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Moving node that is attached to dom (node 0) to child of offscreen parent (node 24)");
  nodes[24].insertChild(nodes[0], 0);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolling down to 1000px - should be at bottom to make sure node 0 is attached properly to node 24");
  setScrollPosition(1000);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.completeTest();

  function setScrollPosition(yPosition) {
    dataGrid._scrollContainer.scrollTop = yPosition;
    dataGrid._onScroll();
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