
import {TestRunner} from 'test_runner';

import * as QuickOpen from 'devtools/ui/legacy/components/quick_open/quick_open.js';

(async function() {
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

  var filteredListWidget = new QuickOpen.FilteredListWidget.FilteredListWidget(null, []);
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
    var promise = TestRunner.addSnifferPromise(filteredListWidget, "itemsFilteredForTest").then(dump);
    filteredListWidget.setProvider(provider);
    return promise;
  }

  function dump() {
    if (filteredListWidget.bottomElementsContainer.classList.contains('hidden')) {
      TestRunner.addResult('Output: <hidden>');
      return;
    }
    if (filteredListWidget.list.element.classList.contains('hidden')) {
      TestRunner.addResult('Output: ' + filteredListWidget.notFoundElement.textContent);
      return;
    }
    var output = [];
    for (var item of filteredListWidget.items)
      output.push(filteredListWidget.provider.itemKeyAt(item));
    TestRunner.addResult('Output:' + JSON.stringify(output));
  }
})();
