(async function() {
  await TestRunner.loadModule('data_grid');

  TestRunner.addResult("This tests long text in datagrid.");

  var columns = [
    {id: "key", title: "Key column", editable: true, longText: false},
    {id: "value", title: "Value column", editable: true, longText: true}
  ];
  var dataGrid = new DataGrid.DataGrid({displayName: 'Test', columns, editCallback: onEdit});
  UI.inspectorView.element.appendChild(dataGrid.element);

  var rootNode = dataGrid.rootNode();
  var node = new DataGrid.DataGridNode({key: "k".repeat(1500), value: "v".repeat(1500)});
  rootNode.appendChild(node);

  var keyElement = dataGrid.element.querySelector(".data .key-column");
  var valueElement = dataGrid.element.querySelector(".data .value-column");

  TestRunner.addResult("Original lengths");
  dumpKeyLength();
  dumpValueLength();

  TestRunner.addResult("\nTest committing a long key");
  dataGrid._startEditing(keyElement);
  keyElement.textContent = "k".repeat(3000);
  dumpKeyLength();
  TestRunner.addResult("Blurring the key");
  keyElement.blur();
  dumpKeyLength();

  TestRunner.addResult("\nTest no-op editing the key");
  dataGrid._startEditing(keyElement);
  dumpKeyLength();
  TestRunner.addResult("Blurring the key");
  keyElement.blur();
  dumpKeyLength();

  TestRunner.addResult("\nTest committing a long value");
  dataGrid._startEditing(valueElement);
  valueElement.textContent = "v".repeat(3000);
  dumpValueLength();
  TestRunner.addResult("Blurring the value");
  valueElement.blur();
  dumpValueLength();

  TestRunner.addResult("\nTest no-op editing the value");
  dataGrid._startEditing(valueElement);
  dumpValueLength();
  TestRunner.addResult("Blurring the value");
  valueElement.blur();
  dumpValueLength();


  TestRunner.completeTest();

  function dumpKeyLength() {
    if (keyElement.classList.contains("being-edited"))
      TestRunner.addResult("key element is being edited");
    TestRunner.addResult("key text length: " + keyElement.textContent.length);
  }

  function dumpValueLength() {
    if (valueElement.classList.contains("being-edited"))
      TestRunner.addResult("value element is being edited");
    TestRunner.addResult("value text length: " + valueElement.textContent.length);
  }

  function onEdit() {
    TestRunner.addResult("Editor value committed.");
  }
})();