
import {TestRunner} from 'test_runner';

import * as UI from 'devtools/ui/legacy/legacy.js';
(async function() {
  TestRunner.addResult('Test ListControl rendering and selection for equal height items case.');

  class Delegate {
    constructor() {
      this.height = 10;
    }

    createElementForItem(item) {
      TestRunner.addResult('Creating element for ' + item);
      var element = document.createElement('div');
      element.style.height = this.height + 'px';
      element.textContent = item;
      return element;
    }

    heightForItem(item) {
      return this.height;
    }

    isItemSelectable(item) {
      return (item % 5 === 0) || (item % 5 === 2);
    }

    selectedItemChanged(from, to, fromElement, toElement) {
      TestRunner.addResult('Selection changed from ' + from + ' to ' + to);
      if (fromElement)
        fromElement.classList.remove('selected');
      if (toElement)
        toElement.classList.add('selected');
    }

    updateSelectedItemARIA(fromElement, toElement) {
      return false;
    }
  }

  var delegate = new Delegate();
  var model = new UI.ListModel.ListModel();
  var list = new UI.ListControl.ListControl(model, delegate, UI.ListControl.ListMode.EqualHeightItems);
  list.element.style.height = '73px';
  UI.InspectorView.InspectorView.instance().element.appendChild(list.element);

  function dumpList()
  {
    var height = list.element.offsetHeight;
    TestRunner.addResult(`----list[length=${model.length}][height=${height}]----`);
    for (var child of list.element.children) {
      var offsetTop = child.getBoundingClientRect().top - list.element.getBoundingClientRect().top;
      var offsetBottom = child.getBoundingClientRect().bottom - list.element.getBoundingClientRect().top;
      var visible = (offsetBottom <= 0 || offsetTop >= height) ? ' ' :
          (offsetTop >= 0 && offsetBottom <= height ? '*' : '+');
      var selected = child.classList.contains('selected') ? ' (selected)' : '';
      var text = child === list.topElement ? 'top' : (child === list.bottomElement ? 'bottom' : child.textContent);
      TestRunner.addResult(`${visible}[${offsetTop}] ${text}${selected}`);
    }
    TestRunner.addResult('');
  }

  TestRunner.addResult('Adding 0, 1, 2');
  model.replaceAll([0, 1, 2]);
  dumpList();

  TestRunner.addResult('Scrolling to 0');
  list.scrollItemIntoView(0);
  dumpList();

  TestRunner.addResult('Scrolling to 2');
  list.scrollItemIntoView(2);
  dumpList();

  TestRunner.addResult('ArrowDown');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
  dumpList();

  TestRunner.addResult('Selecting 2');
  list.selectItem(2);
  dumpList();

  TestRunner.addResult('PageUp');
  list.onKeyDown(TestRunner.createKeyEvent('PageUp'));
  dumpList();

  TestRunner.addResult('PageDown');
  list.onKeyDown(TestRunner.createKeyEvent('PageDown'));
  dumpList();

  TestRunner.addResult('ArrowDown');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
  dumpList();

  TestRunner.addResult('Replacing 0 with 5, 6, 7');
  model.replaceRange(0, 1, [5, 6, 7]);
  dumpList();

  TestRunner.addResult('ArrowUp');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowUp'));
  dumpList();

  TestRunner.addResult('Pushing 10');
  model.insert(model.length, 10);
  dumpList();

  TestRunner.addResult('Selecting 10');
  list.selectItem(10);
  dumpList();

  TestRunner.addResult('Popping 10');
  model.remove(model.length - 1);
  dumpList();

  TestRunner.addResult('Removing 2');
  model.remove(4);
  dumpList();

  TestRunner.addResult('Inserting 8');
  model.insert(1, 8);
  dumpList();

  TestRunner.addResult('Replacing with 0...20');
  model.replaceAll([0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19]);
  dumpList();

  TestRunner.addResult('Resizing');
  list.element.style.height = '83px';
  list.viewportResized();
  dumpList();

  TestRunner.addResult('Scrolling to 19');
  list.scrollItemIntoView(19);
  dumpList();

  TestRunner.addResult('Scrolling to 5');
  list.scrollItemIntoView(5);
  dumpList();

  TestRunner.addResult('Scrolling to 12');
  list.scrollItemIntoView(12);
  dumpList();

  TestRunner.addResult('Scrolling to 13');
  list.scrollItemIntoView(13);
  dumpList();

  TestRunner.addResult('Changing the item height');
  delegate.height = 15;
  list.invalidateItemHeight();
  dumpList();

  TestRunner.addResult('Selecting 7');
  list.selectItem(7);
  dumpList();

  TestRunner.addResult('Replacing 7 with 27');
  model.replaceRange(7, 8, [27]);
  dumpList();

  TestRunner.addResult('Replacing 18, 19 with 28, 29');
  model.replaceRange(18, 20, [28, 29]);
  dumpList();

  TestRunner.addResult('PageDown');
  list.onKeyDown(TestRunner.createKeyEvent('PageDown'));
  dumpList();

  TestRunner.addResult('Replacing 1, 2, 3 with [31-43]');
  model.replaceRange(1, 4, [31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43]);
  dumpList();

  TestRunner.addResult('Scrolling to 13 (center)');
  list.scrollItemIntoView(13, true);
  dumpList();

  TestRunner.addResult('ArrowUp');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowUp'));
  dumpList();

  TestRunner.addResult('Selecting -1');
  list.selectItem(null);
  dumpList();

  TestRunner.addResult('ArrowUp');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowUp'));
  dumpList();

  TestRunner.addResult('Selecting -1');
  list.selectItem(null);
  dumpList();

  TestRunner.addResult('ArrowDown');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
  dumpList();

  TestRunner.addResult('Selecting -1');
  list.selectItem(null);
  dumpList();

  TestRunner.addResult('PageUp');
  list.onKeyDown(TestRunner.createKeyEvent('PageUp'));
  dumpList();

  TestRunner.addResult('Replacing all but 29 with []');
  model.replaceRange(0, 29, []);
  dumpList();

  TestRunner.addResult('ArrowDown');
  list.onKeyDown(TestRunner.createKeyEvent('ArrowDown'));
  dumpList();

  var newModel = new UI.ListModel.ListModel([5, 6, 7]);
  TestRunner.addResult('Replacing model with [5-7]');
  list.setModel(newModel);
  dumpList();

  TestRunner.addResult('Pushing 8');
  newModel.insert(newModel.length, 8);
  dumpList();

  TestRunner.addResult('Pushing 9 to old model');
  model.insert(model.length, 9);
  dumpList();

  TestRunner.completeTest();
})();
