// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests sidebars in WebInspector extensions API\n`);

  TestRunner.dumpSidebarContent = function(panelName, callback) {
    var sidebar = TestRunner._extensionSidebar(panelName);
    TestRunner.deprecatedRunAfterPendingDispatches(function() {
      TestRunner.addResult(panelName + " sidebar content: " + TestRunner.textContentWithoutStyles(sidebar.element));
      callback();
    });
  }

  TestRunner.expandSidebar = function(panelName, callback) {
    var sidebar = TestRunner._extensionSidebar(panelName);
    TestRunner.deprecatedRunAfterPendingDispatches(function() {
      sidebar.revealView();
      callback();
    });
  }

  TestRunner._extensionSidebar = function(panelName) {
    var sidebarPanes = Extensions.extensionServer.sidebarPanes();
    var result;
    for (var i = 0; i < sidebarPanes.length; ++i) {
      if (sidebarPanes[i].panelName() === panelName)
        result = sidebarPanes[i];
    }
    return result;
  }

  await ExtensionsTestRunner.runExtensionTests([
    function extension_sidebarSetPage(panelName, nextTest) {
      function onSidebarCreated(sidebar) {
        output("Sidebar created");
        dumpObject(sidebar);
        async function onShown(win) {
          while (win.document.documentElement.getBoundingClientRect().height <
                 10) {
            await new Promise(resolve => setTimeout(resolve, 10));
          }

          if (panelName !== 'elements')
            output(
                'sidebar height ' +
                win.document.documentElement.getBoundingClientRect().height);
          sidebar.onShown.removeListener(onShown);
          nextTest();
        }
        sidebar.onShown.addListener(onShown);
        var basePath = location.pathname.replace(/\/[^/]*$/, "/");
        sidebar.setPage(basePath + "extension-sidebar.html");
        extension_showPanel(panelName, extension_expandSidebar.bind(null, panelName));
      }
      output("Call createSidebarPane for " + panelName);
      webInspector.panels[panelName].createSidebarPane("Test Sidebar", onSidebarCreated);
    },

    function extension_testElementsSidebarSetPage(nextTest) {
      extension_sidebarSetPage("elements", nextTest);
    },

    function extension_testSourcesSidebarSetPage(nextTest) {
      extension_sidebarSetPage("sources", nextTest);
    },

    function extension_dumpSidebarContent(panelName, nextTest) {
      evaluateOnFrontend("TestRunner.dumpSidebarContent(\"" + panelName + "\", reply);", nextTest);
    },

    function extension_expandSidebar(panelName, callback) {
      evaluateOnFrontend("TestRunner.expandSidebar(\"" + panelName + "\", reply);", callback);
    },

    function extension_testElementsSidebarSetObject(nextTest) {
      extension_sideBarSetObject("elements", nextTest);
    },

    function extension_testSourcesSidebarSetObject(nextTest) {
      extension_sideBarSetObject("sources", nextTest);
    },

    function extension_sideBarSetObject (panelName, nextTest) {
      function onSidebarCreated(sidebar) {
        output("Watch sidebar created, callback arguments dump follows:");
        dumpObject(Array.prototype.slice.call(arguments));
        sidebar.setObject({
          f0: "object",
          f1: undefined,
          f2: null,
          f3: {},
          f4: [],
          f5: ["aa", "bb", "cc"],
          f6: { f60: 42, f61: "foo", f62: [] },
          f7: 42
        }, null, extension_dumpSidebarContent.bind(this, panelName, nextTest));
      }
      webInspector.panels[panelName].createSidebarPane("Watch Test: Object", onSidebarCreated);
    },

    function extension_testSourcesSidebarSetExpressionOnShown(nextTest) {
      function onSidebarCreated(sidebar) {
        function onShown(frame) {
          output("setExpression onShown frame: " + frame);
          sidebar.onShown.removeListener(onShown);
          nextTest();
        }
        sidebar.onShown.addListener(onShown);
        function onSetExpression() {
          extension_showPanel("elements", extension_expandSidebar.bind(null, "elements"));
        }
        sidebar.setExpression("(window.nothing_here)", "title", onSetExpression);
      }
      webInspector.panels.elements.createSidebarPane("OnShown without setExpression", onSidebarCreated);
    },

    function extension_testElementsSidebarSetExpression(nextTest) {
      var panelName = "elements";
      function onSidebarCreated(sidebar) {
        function expression() {
          document.body.testProperty = 'foo';
          function Foo() {};
          var fooInstance = new Foo();
          fooInstance.bar = 1;
          return {
            f0: 'expression',
            f1: undefined,
            f2: null,
            f3: {},
            f4: [],
            f5: ["aa", "bb", "cc"],
            f6: { f60: 42, f61: "foo", f62: [] },
            f7: 42,
            f8: fooInstance,
            f9: document.body.children,
            f10: function() {},
            f11: $0.testProperty
          };
        }
        // Do an extra round-trip to the inspected page to assure inspect()'s round-trip to
        // front-end is complete and $0 is properly updated with currently inspected node.
        webInspector.inspectedWindow.eval("undefined", function() {
          sidebar.setExpression("(" + expression.toString() + ")();", "title", extension_dumpSidebarContent.bind(this, panelName, nextTest));
        });
      }
      webInspector.inspectedWindow.eval("inspect(document.body)", function() {
        webInspector.panels.elements.createSidebarPane("Watch Test: Expression", onSidebarCreated);
      });
    },

    function extension_testSourcesSidebarSetExpression(nextTest) {
      var panelName = "sources";

      function onSidebarExpanded(sidebar) {
        function expression() {
          function Foo() {};
          var fooInstance = new Foo();
          fooInstance.bar = 1;
          return {
            f0: 'expression',
            f1: undefined,
            f2: null,
            f3: {},
            f4: [],
            f5: ["aa", "bb", "cc"],
            f6: { f60: 42, f61: "foo", f62: [] },
            f7: 42,
            f8: fooInstance,
            f9: document.body.children,
            f10: function() {},
          };
        }
        sidebar.setExpression("(" + expression.toString() + ")();", "title", extension_dumpSidebarContent.bind(this, panelName, nextTest));
      }

      function onSidebarCreated(sidebar) {
        extension_showPanel(panelName, extension_expandSidebar.bind(this, panelName, onSidebarExpanded.bind(null, sidebar)));
      }

      webInspector.panels.sources.createSidebarPane("Watch Test: Expression", onSidebarCreated);
    },

    function extension_testElementsSidebarPageReplacedWithObject(nextTest) {
      extension_sidebarPageReplacedWithObject("elements", nextTest);
    },

    function extension_testSourcesSidebarPageReplacedWithObject(nextTest) {
      extension_sidebarPageReplacedWithObject("sources", nextTest);
    },

    function extension_sidebarPageReplacedWithObject(panelName, nextTest) {
      var basePath = location.pathname.replace(/\/[^/]*$/, "/");
      var sidebar;

      function onSidebarCreated(aSidebar) {
        sidebar = aSidebar;
        sidebar.onShown.addListener(onShown);
        sidebar.setPage(basePath + "extension-sidebar.html");
        extension_showPanel(panelName, extension_expandSidebar.bind(this, panelName));
      }
      var didSetObject = false;
      function onShown(frame) {
        output("Got onShown(), frame " + (frame ? "defined" : "not defined"));
        if (!didSetObject) {
          didSetObject = true;
          sidebar.setObject({ foo: 'bar' });
        } else {
          sidebar.onShown.removeListener(onShown);
          nextTest();
        }
      }
      webInspector.panels[panelName].createSidebarPane("Sidebar Test: replace page with object", onSidebarCreated);
    },
  ]);
})();
