(async function() {
  await TestRunner.loadModule('quick_open');
  await TestRunner.loadLegacyModule('quick_open');

  TestRunner.addResult(
      'Test that FilteredListWidget.setProvider changes the provider.');

  var StubProvider = class extends QuickOpen.FilteredListWidget.Provider {
    constructor(input) {
      super();
      this._input = input
    }
    itemKeyAt(itemIndex) { return this._input[itemIndex]; }
    itemScoreAt(itemIndex) { return 0; }
    itemCount() { return this._input.length; }
    shouldShowMatchingItems() { return true; }
    renderItem(item, query, titleElement, subtitleElement) {
      titleElement.textContent = this._input[item];
    }
  };

  var filteredListWidget = new QuickOpen.FilteredListWidget(null, []);
  filteredListWidget.showAsDialog();

  TestRunner.runTests([
    function providerWithOneItem() {
      return setProvider(new StubProvider(['One Item']));
    },
    function nullProvider() {
      return setProvider(null);
    },
    function providerWithTwoItems() {
      return setProvider(new StubProvider(['First Item', 'Second item']));
    },
    function providerWithNoItems() {
      return setProvider(new StubProvider([]));
    }
  ]);

  function setProvider(provider) {
    var promise = TestRunner.addSnifferPromise(filteredListWidget, "_itemsFilteredForTest").then(dump);
    filteredListWidget.setProvider(provider);
    return promise;
  }

  function dump() {
    if (filteredListWidget._bottomElementsContainer.classList.contains('hidden')) {
      TestRunner.addResult('Output: <hidden>');
      return;
    }
    if (filteredListWidget._list.element.classList.contains('hidden')) {
      TestRunner.addResult('Output: ' + filteredListWidget._notFoundElement.textContent);
      return;
    }
    var output = [];
    for (var item of filteredListWidget._items)
      output.push(filteredListWidget._provider.itemKeyAt(item));
    TestRunner.addResult('Output:' + JSON.stringify(output));
  }
})();
