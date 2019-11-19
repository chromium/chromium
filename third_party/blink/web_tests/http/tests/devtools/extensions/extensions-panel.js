// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API\n`);
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('sources_test_runner');
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.loadModule('extensions_test_runner');
  await TestRunner.showPanel('network');
  await TestRunner.showPanel('sources');
  await TestRunner.showPanel('resources');
  await TestRunner.navigatePromise('resources/extensions-panel.html');

  TestRunner.getPanelSize = function() {
    var boundingRect = UI.inspectorView._tabbedPane._contentElement.getBoundingClientRect();
    return {
      width: boundingRect.width,
      height: boundingRect.height
    };
  }

  TestRunner.dumpStatusBarButtons = function() {
    var panel = UI.inspectorView.currentPanelDeprecated();
    var items = panel._panelToolbar._contentElement.children;
    TestRunner.addResult("Status bar buttons state:");
    for (var i = 0; i < items.length; ++i) {
      var item = items[i];
      if (item instanceof HTMLSlotElement)
        continue;
      if (!(item instanceof HTMLButtonElement)) {
        TestRunner.addResult("status bar item " + i + " is not a button: " + item);
        continue;
      }
      // Strip url(...) and prefix of the URL within, leave just last 3 components.
      var url = item.style.backgroundImage.replace(/^url\(.*(([/][^/]*){3}[^/)]*)\)$/, "...$1");
      TestRunner.addResult("status bar item " + i + ", icon: \"" + url + ", tooltip: '" + item[UI.Tooltip._symbol].content + "', disabled: " + item.disabled);
    }
  }

  TestRunner.clickButton = function(index) {
    var panel = UI.inspectorView.currentPanelDeprecated();
    var items = panel._panelToolbar._contentElement.children;
    for (var i = 0, buttonIndex = 0; i < items.length; ++i) {
      if (items[i] instanceof HTMLButtonElement) {
        if (buttonIndex === index) {
          items[i].click();
          return;
        }
        buttonIndex++;
      }
    }
    TestRunner.addResult("No button with index " + index);
    items[index].click();
  }

  TestRunner.logMessageAndClickOnURL = function() {
    ConsoleTestRunner.disableConsoleViewport();
    TestRunner.evaluateInPage("logMessage()");
    var wrappedConsoleMessageAdded = TestRunner.safeWrap(consoleMessageAdded);
    SDK.consoleModel.addEventListener(SDK.ConsoleModel.Events.MessageAdded, wrappedConsoleMessageAdded);

    function consoleMessageAdded() {
      SDK.consoleModel.removeEventListener(SDK.ConsoleModel.Events.MessageAdded, wrappedConsoleMessageAdded);
      Console.ConsoleView.instance()._invalidateViewport();
      var xpathResult = document.evaluate("//span[@class='devtools-link' and starts-with(., 'extensions-panel.html')]", Console.ConsoleView.instance()._viewport.element, null, XPathResult.ANY_UNORDERED_NODE_TYPE, null);

      var click = document.createEvent("MouseEvent");
      click.initMouseEvent("click", true, true);
      xpathResult.singleNodeValue.dispatchEvent(click);
    }
  }

  TestRunner.installShowResourceLocationHooks = function() {
    function showURL(panelName, url, lineNumber) {
      var url = url.replace(/^.*(([/][^/]*){3}[^/)]*)$/, "...$1");
      TestRunner.addResult("Showing resource " + url + " in panel " + panelName + "), line: " + lineNumber);
    }
    NetworkTestRunner.recordNetwork();
    TestRunner.addSniffer(UI.panels.sources, "showUILocation", showUILocationHook, true);
    TestRunner.addSniffer(UI.panels.resources._sidebar, "showResource", showResourceHook, true);
    TestRunner.addSniffer(UI.panels.network, "revealAndHighlightRequest", showRequestHook, true);

    function showUILocationHook(uiLocation) {
      showURL("sources", uiLocation.uiSourceCode.url(), uiLocation.lineNumber);
    }

    function showResourceHook(resource, lineNumber) {
      showURL("resources", resource.url, lineNumber);
    }

    /**
     * @param {!SDK.NetworkRequest} request
     */
    function showRequestHook(request) {
      showURL("network", request.url());
    }
  }

  TestRunner.switchToLastPanel = function() {
    var lastPanelName = UI.inspectorView._tabbedPane._tabs.peekLast().id;
    return UI.inspectorView.showPanel(lastPanelName);
  }

  await ExtensionsTestRunner.runExtensionTests([
    function extension_testThemeName(nextTest) {
      output("Theme name: " + webInspector.panels.themeName);
      nextTest();
    },

    function extension_testCreatePanel(nextTest) {
      var expectOnShown = false;

      function onPanelShown(panel, window) {
        if (!expectOnShown) {
          output("FAIL: unexpected onShown event");
          nextTest();
          return;
        }
        output("Panel shown");
        panel.onShown.removeListener(onPanelShown);
        evaluateOnFrontend("reply(TestRunner.getPanelSize())", function(result) {
          if (result.width !== window.innerWidth)
            output("panel width mismatch, outer: " + result.width + ", inner:" + window.innerWidth);
          else if (result.height !== window.innerHeight)
            output("panel height mismatch, outer: " + result.height + ", inner:" + window.innerHeight);
          else
            output("Extension panel size correct");
          nextTest();
        });
      }

      function onPanelCreated(panel) {
        function onPanelShown(window) {
          if (!expectOnShown) {
             output("FAIL: unexpected onShown event");
             nextTest();
             return;
          }
          output("Panel shown");
          panel.onShown.removeListener(onPanelShown);
          panel.onHidden.addListener(onPanelHidden);
          evaluateOnFrontend("reply(TestRunner.getPanelSize())", function(result) {
            if (result.width !== window.innerWidth)
              output("panel width mismatch, outer: " + result.width + ", inner:" + window.innerWidth);
            else if (result.height !== window.innerHeight)
              output("panel height mismatch, outer: " + result.height + ", inner:" + window.innerHeight);
            else
              output("Extension panel size correct");
            extension_showPanel("console");
          });
        }

        function onPanelHidden() {
          panel.onHidden.removeListener(onPanelHidden);
          output("Panel hidden");
          nextTest();
        }

        output("Panel created");
        dumpObject(panel);
        panel.onShown.addListener(onPanelShown);

        // This is not authorized and therefore should not produce any output
        panel.show();
        extension_showPanel("console");

        function handleOpenResource(resource, lineNumber) {
          // This will force extension iframe to be really loaded.
          panel.show();
        }
        webInspector.panels.setOpenResourceHandler(handleOpenResource);
        evaluateOnFrontend("Components.Linkifier._linkHandlerSetting().set('test extension')");
        evaluateOnFrontend("TestRunner.logMessageAndClickOnURL();");
        expectOnShown = true;
      }
      var basePath = location.pathname.replace(/\/[^/]*$/, "/");
      webInspector.panels.create("Test Panel", basePath + "extension-panel.png", basePath + "extension-panel.html", onPanelCreated);
    },

    function extension_testSearch(nextTest) {
      var callbackCount = 0;

      function onPanelCreated(panel) {
        var callback = function(action, queryString) {
          output("Panel searched:");
          dumpObject(Array.prototype.slice.call(arguments));
          callbackCount++;
          if (callbackCount === 2) {
            nextTest();
            panel.onSearch.removeListener(callback);
          }
        };
        panel.onSearch.addListener(callback);

        extension_showPanel("extension");

        function performSearch(query) {
          UI.inspectorView.panel(Extensions.extensionsOrigin + "TestPanelforsearch").then(panel => {
            panel.searchableView().showSearchField();
            panel.searchableView()._searchInputElement.value = query;
            panel.searchableView()._performSearch(true, true);
            panel.searchableView().cancelSearch();
          });
        }

        evaluateOnFrontend(performSearch.toString() + " performSearch(\"hello\");");
      }
      var basePath = location.pathname.replace(/\/[^/]*$/, "/");
      webInspector.panels.create("Test Panel for search", basePath + "extension-panel.png", basePath + "non-existent.html", onPanelCreated);
    },

    function extension_testStatusBarButtons(nextTest) {
      var basePath = location.pathname.replace(/\/[^/]*$/, "/");

      function onPanelCreated(panel) {
        var button1 = panel.createStatusBarButton(basePath + "button1.png", "Button One tooltip");
        var button2 = panel.createStatusBarButton(basePath + "button2.png", "Button Two tooltip", true);
        output("Created a status bar button, dump follows:");
        dumpObject(button1);
        function updateButtons() {
          button1.update(basePath + "button1-updated.png");
          button2.update(null, "Button Two updated tooltip", false);
          output("Updated status bar buttons");
          evaluateOnFrontend("TestRunner.dumpStatusBarButtons(); TestRunner.clickButton(1);");
        }
        button1.onClicked.addListener(function() {
          output("button1 clicked");
          evaluateOnFrontend("TestRunner.dumpStatusBarButtons(); reply();", updateButtons);
        });
        button2.onClicked.addListener(function() {
          output("button2 clicked");
          nextTest();
        });
        // First we click on button2 (that is [1] in array). But it is disabled, so this should be a noop. Then we click on button1.
        // button1 click updates buttons. and clicks button2.
        evaluateOnFrontend("ExtensionsTestRunner.showPanel('extension').then(function() { TestRunner.clickButton(1); TestRunner.clickButton(0); })");
      }

      webInspector.panels.create("Buttons Panel", basePath + "extension-panel.png", basePath + "non-existent.html", onPanelCreated);
    },

    function extension_testOpenResource(nextTest) {
      var urls = [
        'http://127.0.0.1:8000/devtools/extensions/resources/extensions-panel.html',
        'http://127.0.0.1:8000/devtools/extensions/resources/abe.png',
        'http://127.0.0.1:8000/devtools/extensions/resources/missing.txt',
        'http://127.0.0.1:8000/devtools/extensions/not-found.html',
        "javascript:console.error('oh no!')"
      ];

      var urlIndex = 0;

      evaluateOnFrontend("TestRunner.installShowResourceLocationHooks(); reply();", function() {
        webInspector.inspectedWindow.eval("loadResources();");
        webInspector.network.onRequestFinished.addListener(request => {
          if (request.request.url.includes('abe.png'))
            showNextURL();
        });
      });
      function showNextURL() {
        if (urlIndex >= urls.length) {
          nextTest();
          return;
        }
        var url = urls[urlIndex++];
        output("Showing " + trimURL(url));
        webInspector.panels.openResource(url, 1000 + urlIndex, showNextURL);
      }
    },

    function extension_testGlobalShortcuts(nextTest) {
      var platform;
      var testPanel;
      evaluateOnFrontend("reply(Host.platform())", function(result) {
        platform = result;
        var basePath = location.pathname.replace(/\/[^/]*$/, "/");
        webInspector.panels.create("Shortcuts Test Panel", basePath + "extension-panel.png", basePath + "extension-panel.html", onPanelCreated);
      });
      function dispatchKeydownEvent(attributes) {
        var event = new KeyboardEvent("keydown", attributes);
        document.dispatchEvent(event);
      }
      function onPanelCreated(panel) {
        testPanel = panel;
        testPanel.onShown.addListener(onPanelShown);
        evaluateOnFrontend("TestRunner.switchToLastPanel();");
      }
      var panelWindow;
      function onPanelShown(win) {
        panelWindow = win;
        testPanel.onShown.removeListener(onPanelShown);
        output("Panel shown, now toggling console...");
        panelWindow.addEventListener("resize", onPanelResized);
        dispatchKeydownEvent({ key: "Escape", keyCode: 27 });
      }
      function onPanelResized() {
        panelWindow.removeEventListener("resize", onPanelResized);
        output("Panel resized, test passed.");
        evaluateOnFrontend("reply(UI.inspectorView._closeDrawer())", nextTest);
      }
    },
  ]);
})();
