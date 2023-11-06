
import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
(async function () {
  var items = [
    {
      title: "first",
      index: 0
    },
    {
      title: "second",
      index: 1
    },
    {
      title: "third",
      index: 2
    },
    {
      title: "fourth",
      index: 3
    },
    {
      title: "disabled 4.5",
      disabled: true,
      index: 4
    },
    {
      title: "fifth",
      index: 5
    },
    {
      title: "sixth",
      index: 6
    },
    {
      title: "seventh",
      index: 7
    },
    {
      title: "eighth",
      index: 8
    }
  ];

  class Delegate {
    titleFor(item) {
      return item.title;
    }

    createElementForItem(item) {
      var element = createElement("div");
      element.textContent = this.titleFor(item);
      return element;
    }

    isItemSelectable(item) {
      return !item.disabled;
    }

    itemSelected(item) {
      if (item !== null)
        TestRunner.addResult("Item selected: " + this.titleFor(item));
    }

    highlightedItemChanged(from, to, fromElement, toElement) {
      if (to !== null)
        TestRunner.addResult("Item highlighted: " + this.titleFor(to));
    }
  };

  function pressKey(key) {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (!element)
      return;
    TestRunner.addResult(key);
    element.dispatchEvent(TestRunner.createKeyEvent(key));
  }
  var model = new UI.ListModel.ListModel();
  var dropDown = new UI.SoftDropDown.SoftDropDown(model, new Delegate());
  for (var i = items.length - 1; i >= 0; i--)
    model.insertWithComparator(items[i], (a, b) => a.index - b.index);

  UI.InspectorView.InspectorView.instance().element.appendChild(dropDown.element);
  dropDown.selectItem(items[5]);
  TestRunner.addResult("Showing drop down");
  dropDown.element.dispatchEvent(new Event("mousedown"));
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  pressKey('ArrowUp');
  pressKey('ArrowUp');
  pressKey('ArrowUp');
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  pressKey('f');
  pressKey('f');
  pressKey('t');
  TestRunner.completeTest();
})();
