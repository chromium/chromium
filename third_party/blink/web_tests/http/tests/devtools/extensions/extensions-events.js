// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API\n`);
  await TestRunner.loadTestModule('extensions_test_runner');
  await TestRunner.loadModule('sources'); await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.showPanel('sources');
  await TestRunner.navigatePromise(TestRunner.url('./resources/extensions-events.html'));

  TestRunner.expandSidebar = function(callback) {
    var sidebar = Extensions.extensionServer.sidebarPanes()[0];
    sidebar.revealView().then(callback);
  }

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testElementsOnSelectionChanged(nextTest) {
      function onSelectionChanged() {
        webInspector.panels.elements.onSelectionChanged.removeListener(onSelectionChanged);
        output("onSelectionChanged fired");
        nextTest();
      }
      webInspector.panels.elements.onSelectionChanged.addListener(onSelectionChanged);
      webInspector.inspectedWindow.eval("inspect(document.body.children[0]), 0");
    },

    function extension_testSourcesOnSelectionChangedShowFile(nextTest) {
      function onSelectionChanged(selectionInfo) {
        webInspector.panels.sources.onSelectionChanged.removeListener(onSelectionChanged);
        output("sources onSelectionChanged fired, selectionInfo:");
        dumpObject(selectionInfo, {url: "url"});
        nextTest();
      }
      webInspector.panels.sources.onSelectionChanged.addListener(onSelectionChanged);
      evaluateOnFrontend("SourcesTestRunner.showScriptSourcePromise(\"test-script.js\")");
    },

    function extension_testSourcesOnSelectionChangedShowFileAndLine(nextTest) {
      function onSelectionChanged(selectionInfo) {
        webInspector.panels.sources.onSelectionChanged.removeListener(onSelectionChanged);
        output("sources onSelectionChanged fired, selectionInfo:");
        dumpObject(selectionInfo, {url: "url"});
        nextTest();
      }
      webInspector.panels.sources.onSelectionChanged.addListener(onSelectionChanged);
      webInspector.panels.openResource('http://127.0.0.1:8000/devtools/extensions/resources/test-script.js', 2);
    },

    function extension_testOnRequestFinished(nextTest) {
      function onRequestFinished() {
        webInspector.network.onRequestFinished.removeListener(onRequestFinished);
        output("onRequestFinished fired");
        nextTest();
      }
      webInspector.network.onRequestFinished.addListener(onRequestFinished);
      webInspector.inspectedWindow.eval("var xhr = new XMLHttpRequest(); xhr.open('GET', location.href, false); xhr.send(null);");
    },

    function extension_testOnNavigated(nextTest) {
      var urls = [];
      var loadCount = 0;

      function onLoad() {
        ++loadCount;
        processEvent();
      }
      function processEvent() {
        if (loadCount !== urls.length)
          return;
        if (loadCount === 1)
          evaluateOnFrontend("TestRunner.navigate(TestRunner.mainTarget.inspectedURL().substring(0, TestRunner.mainTarget.inspectedURL().indexOf('?')), reply)", onLoad);
        else {
          webInspector.network.onNavigated.removeListener(onNavigated);
          for (var i = 0; i < urls.length; ++i)
            output("Navigated to: " + urls[i]);
          nextTest();
        }
      }
      function onNavigated(url) {
        urls.push(url.replace(/^(.*\/)*/, ""));
        processEvent();
      }
      webInspector.network.onNavigated.addListener(onNavigated);
      evaluateOnFrontend("TestRunner.navigate(TestRunner.mainTarget.inspectedURL() + '?navigated', reply)", onLoad);
    },

    function extension_testViewShowHide(nextTest) {
      var listenersToCleanup = [];
      var sidebar;
      var beenToExtensionPanel = false;

      function onViewEvent(type, viewName, viewWindow) {
        output("Got " + type + " event for " + viewName);
        if (type !== "onShown")
          return;
        if (viewName === "panel") {
          output("Panel shown, location: " + trimURL(viewWindow.location.href));
          extension_showPanel("elements");
        } else if (viewName === "sidebar") {
          output("Sidebar shown, location: " + trimURL(viewWindow.location.href));
          if (!beenToExtensionPanel) {
            extension_showPanel("extension");
            beenToExtensionPanel = true;
          } else {
            cleanupListeners();
            nextTest();
          }
        }
      }
      function addListener(view, viewName, type) {
        var listener = bind(onViewEvent, null, type, viewName);
        var event = view[type];
        listenersToCleanup.push({ event: event, listener: listener });
        event.addListener(listener);
      }
      function cleanupListeners() {
        for (var i = 0; i < listenersToCleanup.length; ++i)
          listenersToCleanup[i].event.removeListener(listenersToCleanup[i].listener);
      }
      function onPanelCreated(panel) {
        addListener(panel, "panel", "onShown");
        addListener(panel, "panel", "onHidden");
        addListener(sidebar, "sidebar", "onHidden");
        addListener(sidebar, "sidebar", "onShown");
        sidebar.setPage(basePath + "extension-sidebar.html");
      }
      extension_showPanel("elements", callback => evaluateOnFrontend("TestRunner.expandSidebar(reply);", callback));

      var basePath = location.pathname.replace(/\/[^/]*$/, "/");
      function onSidebarCreated(sidebarPane) {
        sidebar = sidebarPane;
        webInspector.panels.create("Test Panel", basePath + "extension-panel.png", basePath + "extension-panel.html", onPanelCreated);
      }
      webInspector.panels.elements.createSidebarPane("Test Sidebar", onSidebarCreated);
    },
  ]);
})();
