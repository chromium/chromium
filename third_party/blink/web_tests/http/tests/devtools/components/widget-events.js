// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`This tests that events are properly propagated through Widget hierarchy.\n`);


  var TestWidget = class extends UI.Widget.Widget {
    constructor(widgetName) {
      super();
      this.widgetName = widgetName;
      this.processWillShowCount = 0;
      this.processWasHiddenCount = 0;
      TestRunner.addResult(this.widgetName + '()');
    }

    processWillShow() {
      TestRunner.assertEquals(this.processWillShowCount, this.processWasHiddenCount);
      super.processWillShow();
      ++this.processWillShowCount;
    }

    processWasHidden() {
      super.processWasHidden();
      ++this.processWasHiddenCount;
      TestRunner.assertEquals(this.processWillShowCount, this.processWasHiddenCount);
    }

    show(parentElement) {
      TestRunner.addResult(this.widgetName + '.show()');
      super.show(parentElement);
    }

    detach() {
      TestRunner.addResult(this.widgetName + '.detach()');
      super.detach();
    }

    doResize() {
      TestRunner.addResult(this.widgetName + '.doResize()');
      super.doResize();
    }

    wasShown() {
      TestRunner.addResult('  ' + this.widgetName + '.wasShown()');
      if (this.showOnWasShown)
        this.showOnWasShown.show(this.showRoot || this.element);
      if (this.detachOnWasShown)
        this.detachOnWasShown.detach();
      if (this.resizeOnWasShown)
        this.resizeOnWasShown.doResize();
    }

    willHide() {
      TestRunner.addResult('  ' + this.widgetName + '.willHide()');
      if (this.showOnWillHide)
        this.showOnWillHide.show(this.element);
      if (this.detachOnWillHide)
        this.detachOnWillHide.detach();
    }

    onResize() {
      TestRunner.addResult('  ' + this.widgetName + '.onResize()');
    }
  };

  TestRunner.runTestSuite([
    function testShowWidget(next) {
      var widget = new TestWidget('Widget');
      widget.show(UI.InspectorView.InspectorView.instance().element);
      widget.detach();
      next();
    },

    function testAppendViaDOM(next) {
      try {
        var widget = new TestWidget('Widget');
        document.body.appendChild(widget.element);
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testInsertViaDOM(next) {
      try {
        var widget = new TestWidget('Widget');
        document.body.insertBefore(widget.element, null);
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testAttachToOrphanNode(next) {
      try {
        var widget = new TestWidget('Widget');
        var div = document.createElement('div');
        widget.show(div);
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testImmediateParent(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      childWidget.show(parentWidget.element);
      if (childWidget.parentWidget() === parentWidget)
        TestRunner.addResult('OK');
      else
        TestRunner.addResult('FAILED');
      next();
    },

    function testDistantParent(next) {
      var parentWidget = new TestWidget('Parent');
      var div = document.createElement('div');
      parentWidget.element.appendChild(div);
      var childWidget = new TestWidget('Child');
      childWidget.show(div);

      if (childWidget.parentWidget() === parentWidget)
        TestRunner.addResult('OK');
      else
        TestRunner.addResult('FAILED');
      next();
    },

    function testEvents(next) {
      var parentWidget = new TestWidget('Parent');
      parentWidget.markAsRoot();
      var childWidget = new TestWidget('Child');
      parentWidget.show(document.body);

      parentWidget.doResize();
      childWidget.show(parentWidget.element);
      parentWidget.doResize();
      parentWidget.detach();
      parentWidget.show(document.body);
      childWidget.detach();
      parentWidget.detach();
      next();
    },

    function testEventsHideOnDetach(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      childWidget.setHideOnDetach();
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);

      parentWidget.doResize();
      childWidget.show(parentWidget.element);
      parentWidget.doResize();
      parentWidget.detach();
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      childWidget.detach();
      parentWidget.detach();
      next();
    },

    function testRemoveChild(next) {
      var parentWidget = new TestWidget('Parent');
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);

      var childWidget = new TestWidget('Child');
      childWidget.show(parentWidget.element);
      try {
        parentWidget.element.removeChild(childWidget.element);
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testImplicitRemoveChild(next) {
      var parentWidget = new TestWidget('Parent');
      var div = document.createElement('div');
      parentWidget.element.appendChild(div);

      var childWidget = new TestWidget('Child');
      childWidget.show(div);

      try {
        parentWidget.element.removeChild(div);
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testRemoveChildren(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      childWidget.show(parentWidget.element);
      parentWidget.element.appendChild(document.createElement('div'));
      try {
        parentWidget.element.removeChildren();
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testImplicitRemoveChildren(next) {
      var parentWidget = new TestWidget('Parent');
      var div = document.createElement('div');
      parentWidget.element.appendChild(div);

      var childWidget = new TestWidget('Child');
      childWidget.show(div);

      try {
        parentWidget.element.removeChildren();
      } catch (e) {
        TestRunner.addResult(e);
      }
      next();
    },

    function testShowOnWasShown(next) {
      var parentWidget = new TestWidget('Parent');
      parentWidget.showOnWasShown = new TestWidget('Child');
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      parentWidget.detach();
      next();
    },

    function testShowNestedOnWasShown(next) {
      var topWidget = new TestWidget('Top');
      var middleWidget = new TestWidget('Middle');
      var bottomWidget = new TestWidget('Bottom');
      middleWidget.show(topWidget.element);
      topWidget.showOnWasShown = bottomWidget;
      topWidget.showRoot = middleWidget.element;
      topWidget.show(UI.InspectorView.InspectorView.instance().element);
      topWidget.detach();
      next();
    },

    function testDetachOnWasShown(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      childWidget.show(parentWidget.element);
      parentWidget.detachOnWasShown = childWidget;
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      parentWidget.detach();
      next();
    },

    function testShowOnWillHide(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      childWidget.show(parentWidget.element);
      parentWidget.showOnWillHide = childWidget;
      parentWidget.detach();
      next();
    },

    function testDetachOnWillHide(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      childWidget.show(parentWidget.element);
      parentWidget.detachOnWillHide = childWidget;
      parentWidget.detach();
      next();
    },

    function testShowDetachesFromPrevious(next) {
      var parentWidget1 = new TestWidget('Parent1');
      var parentWidget2 = new TestWidget('Parent2');
      var childWidget = new TestWidget('Child');
      parentWidget1.show(UI.InspectorView.InspectorView.instance().element);
      parentWidget2.show(UI.InspectorView.InspectorView.instance().element);
      childWidget.show(parentWidget1.element);
      childWidget.show(parentWidget2.element);
      next();
    },

    function testResizeOnWasShown(next) {
      var parentWidget = new TestWidget('Parent');
      var childWidget = new TestWidget('Child');
      childWidget.show(parentWidget.element);
      parentWidget.resizeOnWasShown = childWidget;
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      parentWidget.detach();
      next();
    },

    function testReparentWithinWidget(next) {
      var parentWidget = new TestWidget('Parent');
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      var childWidget = new TestWidget('Child');
      var container1 = parentWidget.element.createChild('div');
      var container2 = parentWidget.element.createChild('div');
      childWidget.show(container1);
      childWidget.show(container2);
      next();
    },

    function testDetachChildWidgetsRemovesHiddenChildren(next) {
      var parentWidget = new TestWidget('Parent');
      var visibleChild = new TestWidget('visibleChild');
      var hiddenChild = new TestWidget('hiddenChild');
      parentWidget.show(UI.InspectorView.InspectorView.instance().element);
      visibleChild.show(parentWidget.element);
      hiddenChild.show(parentWidget.element);
      hiddenChild.hideWidget();
      parentWidget.detachChildWidgets();
      var count = parentWidget.element.childElementCount;
      TestRunner.addResult(`Parent element has ${count} child elements`);
      next();
    }
  ]);
})();
