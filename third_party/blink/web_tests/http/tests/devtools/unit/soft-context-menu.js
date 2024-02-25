
import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
(async function() {
  var menu = new UI.SoftContextMenu.SoftContextMenu([{
    type: 'item',
    label: 'First',
    enabled: true
  },
  {
    type: 'subMenu',
    label: 'Second',
    enabled: true,
    subItems: [
      {type: 'subMenu', label: 'Child 1', enabled: true, subItems: [{type: 'item', label: 'Grandchild', id: 'Grandchild', enabled: true}]},
      {type: 'item', label: 'Child 2', enabled: true},
      {type: 'item', label: 'Child 3', enabled: true},
      {type: 'item', label: 'Child 4', enabled: true}
    ]
  },
  {
    type: 'separator',
  },{
    type: 'item',
    label: 'Third',
    enabled: true
  }], item => TestRunner.addResult('Item Selected: ' + item));

  var initialFocusedElement = UI.InspectorView.InspectorView.instance().element.createChild('div');
  initialFocusedElement.textContent = 'Initial Focused Element';
  initialFocusedElement.tabIndex = -1;
  initialFocusedElement.focus();

  dumpContextMenu();
  menu.show(document, new AnchorBox(50, 50, 0, 0));
  dumpContextMenu();
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  pressKey('ArrowUp');
  pressKey('ArrowUp');
  pressKey('ArrowUp');
  pressKey('ArrowDown');
  TestRunner.addResult('Enter Submenu');
  pressKey('ArrowRight');
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  pressKey('ArrowDown');
  TestRunner.addResult('Leave Submenu ArrowLeft');
  pressKey('ArrowLeft');
  pressKey('ArrowRight');
  TestRunner.addResult('Leave Submenu Escape');
  pressKey('Escape');
  TestRunner.addResult('Enter Sub-Submenu');
  pressKey(' ');
  pressKey('Enter');
  pressKey('Enter');
  TestRunner.completeTest();

  function pressKey(key) {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (!element)
      return;
    element.dispatchEvent(TestRunner.createKeyEvent(key));
    if (key === ' ')
      key = 'Space';
    TestRunner.addResult(key);
    dumpContextMenu();
  }

  function dumpContextMenu() {
    if (initialFocusedElement.hasFocus()) {
      TestRunner.addResult('Initial focused element has focus');
      return;
    }
    var selection = '';
    var subMenu = menu;
    var activeElement = Platform.DOMUtilities.deepActiveElement(document);
    do {
      if (selection)
        selection += ' -> ';
      const focused = (subMenu.highlightedMenuItemElement || subMenu.contextMenuElement) === activeElement;
      if (focused)
        selection += '[';
      if (subMenu.highlightedMenuItemElement)
        selection += subMenu.highlightedMenuItemElement.textContent.replace(/[^A-z0-9 ]/g, '');
      else
        selection += 'null'
      if (focused)
        selection += ']';
    }
    while (subMenu = subMenu.subMenu)
    TestRunner.addResult(selection);
  }
})();
