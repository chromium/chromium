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

  for (var i = 0; i < 30; i++) {
    var node = new DataGrid.ViewportDataGridNode({id: "a" + i});
    rootNode.appendChild(node);
    nodes.push(node);
  }

  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 133px");
  setScrollPosition(133);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 312px");
  setScrollPosition(312);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled down to 1000px - should be at bottom");
  setScrollPosition(1000);
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Scrolled up to 0px");
  setScrollPosition(0);
  dataGrid._update();
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
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.addResult("Testing adding of some nodes back into viewport");
  rootNode.insertChild(nodes[0], 0);
  rootNode.insertChild(nodes[1], 1);
  rootNode.insertChild(nodes[3], 3);
  rootNode.insertChild(nodes[5], 5);

  TestRunner.addResult("Nodes should be the same as previous dump because of throttling:");
  dumpVisibleNodes();

  TestRunner.addResult("Should have nodes 0, 1, 3, 5 back in dom and previously added nodes removed:");
  dataGrid._update();
  dumpVisibleNodes();

  TestRunner.completeTest();



  function setScrollPosition(yPosition) {
    dataGrid._scrollContainer.scrollTop = yPosition;
    dataGrid._onScroll();
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
