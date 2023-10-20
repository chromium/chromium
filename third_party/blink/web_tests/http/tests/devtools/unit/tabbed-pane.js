
import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';
import * as UI from 'devtools/ui/legacy/legacy.js';
(async function() {
  TestRunner.addResult("This tests if the TabbedPane is keyboard navigable.");

  class FocusableWidget extends UI.Widget.Widget {
    constructor(name) {
      super();
      this.element.tabIndex = -1;
      this.element.textContent = name;
      this.setDefaultFocusedElement(this.element);
    }
  }

  var tabbedPane = new UI.TabbedPane.TabbedPane();
  tabbedPane.show(UI.InspectorView.InspectorView.instance().element);
  TestRunner.addSnifferPromise(tabbedPane, 'innerUpdateTabElements').then(tabsAdded);
  for (var i = 0; i < 10; i++)
    tabbedPane.appendTab(i.toString(), 'Tab ' + i, new FocusableWidget('Widget ' + i));

  function tabsAdded() {
    tabbedPane.currentTab.tabElement.focus();
    dumpFocus();
    TestRunner.addResult('Moving right and wrapping around');
    for (var i = 0; i < 20; i++)
      right();
    TestRunner.addResult('Moving left and focusing widgets')
    for (var i = 0; i < 10; i++) {
      left();
      enter();
      tabbedPane.currentTab.tabElement.focus();
    }
    TestRunner.completeTest();
  }

  function right() {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (element)
      element.dispatchEvent(TestRunner.createKeyEvent('ArrowRight'));
    dumpFocus();
  }

  function left() {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (element)
      element.dispatchEvent(TestRunner.createKeyEvent('ArrowLeft'));
    dumpFocus();
  }

  function enter() {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (element)
      element.dispatchEvent(TestRunner.createKeyEvent('Enter'));
    dumpFocus();
  }


  function dumpFocus() {
    var element = Platform.DOMUtilities.deepActiveElement(document);
    if (!element) {
      TestRunner.addResult("null");
      return;
    }
    TestRunner.addResult(element.textContent);
  }

})();
