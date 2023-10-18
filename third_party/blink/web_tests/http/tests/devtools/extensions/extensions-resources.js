// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';
import {SourcesTestRunner} from 'sources_test_runner';

import * as UI from 'devtools/ui/legacy/legacy.js';
import * as Console from 'devtools/panels/console/console.js';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Components from 'devtools/ui/legacy/components/utils/utils.js';

(async function() {
  TestRunner.addResult(`Tests resource-related methods of WebInspector extension API\n`);

  TestRunner.clickOnURL = async function() {
    await UI.ViewManager.ViewManager.instance().showView("console").then(() => {
      Console.ConsoleView.ConsoleView.instance().updateMessageList();

      // Trigger link creation so we can properly await pending live location updates. Needed so we can
      // click the link in the first place.
      for (const messageView of Console.ConsoleView.ConsoleView.instance().visibleViewMessages) messageView.element();
      TestRunner.waitForPendingLiveLocationUpdates().then(() => {
        var xpathResult = document.evaluate("//span[@class='devtools-link' and starts-with(., 'test-script.js')]",
                                            Console.ConsoleView.ConsoleView.instance().element, null, XPathResult.ANY_UNORDERED_NODE_TYPE, null);

        var click = document.createEvent("MouseEvent");
        click.initMouseEvent("click", true, true);
        xpathResult.singleNodeValue.dispatchEvent(click);
      });
    });
  }

  TestRunner.waitForStyleSheetChangedEvent = function(reply) {
    TestRunner.addSniffer(SDK.CSSModel.CSSModel.prototype, "fireStyleSheetChanged", reply);
  }

  await TestRunner.evaluateInPageAnonymously(`
    function loadFrame() {
      var callback;
      var promise = new Promise((fulfill) => callback = fulfill);
      var iframe = document.createElement("iframe");
      iframe.src = "resources/subframe.html";
      iframe.addEventListener("load", callback);
      document.body.appendChild(iframe);
      return promise;
    }

    function logMessage() {
      frames[0].logMessage();
    }

    function addResource() {
      var script = document.createElement("script");
      script.src = "data:application/javascript," + escape("function test_func(){};");
      document.head.appendChild(script);
    }
  `);

  ExtensionsTestRunner.evaluateInExtension(function extension_runWithResource(regexp, callback) {
    function onResources(resources) {
      for (var i = 0; i < resources.length; ++i) {
        if (regexp.test(resources[i].url)) {
          callback(resources[i])
          return;
        }
      }
      throw "Failed to find a resource: " + regexp.toString();
    }
    webInspector.inspectedWindow.getResources(onResources);
  });


  await ExtensionsTestRunner.runExtensionTests([
    function extension_testGetAllResources(nextTest) {
      function callback(resources) {
        resources.sort((a, b) => trimURL(a.url).localeCompare(trimURL(b.url)));
        output("page resources:");
        dumpObject(Array.prototype.slice.call(arguments), { url: "url" });
      }
      invokePageFunctionAsync("loadFrame", function() {
        webInspector.inspectedWindow.getResources(callbackAndNextTest(callback, nextTest));
      });
    },

    function extension_testGetResourceContent(nextTest) {
      function onContent() {
        dumpObject(Array.prototype.slice.call(arguments));
      }
      extension_runWithResource(/test-script\.js$/, function(resource) {
        resource.getContent(callbackAndNextTest(onContent, nextTest));
      });
    },

    function extension_testSetResourceContent(nextTest) {
      evaluateOnFrontend("TestRunner.waitForStyleSheetChangedEvent(reply);", step2);

      extension_runWithResource(/audits-style1\.css$/, function(resource) {
        resource.setContent("div.test { width: 126px; height: 42px; }", false, function() {});
      });

      function step2() {
        webInspector.inspectedWindow.eval("frames[0].document.getElementById('test-div').clientWidth", function(result) {
          output("div.test width after stylesheet edited (should be 126): " + result);
          nextTest();
        });
      }
    },

    function extension_testOnContentCommitted(nextTest) {
      var expected_content = "div.test { width: 220px; height: 42px; }";

      webInspector.inspectedWindow.onResourceContentCommitted.addListener(onContentCommitted);
      extension_runWithResource(/audits-style1\.css$/, function(resource) {
        resource.setContent("div.test { width: 140px; height: 42px; }", false);
      });
      extension_runWithResource(/abe\.png$/, function(resource) {
        resource.setContent("", true);
      });
      extension_runWithResource(/audits-style1\.css$/, function(resource) {
        resource.setContent(expected_content, true);
      });

      function onContentCommitted(resource, content) {
        output("content committed for resource " + trimURL(resource.url) + " (type: " + resource.type + "), new content: " + content);
        if (!/audits-style1\.css$/.test(resource.url) || content !== expected_content)
          output("FAIL: stray onContentEdited event");
        webInspector.inspectedWindow.onResourceContentCommitted.removeListener(onContentCommitted);
        resource.getContent(function(content) {
          output("Revision content: " + content);
          nextTest();
        });
      }
    },

    function extension_testOnResourceAdded(nextTest) {
      evaluateOnFrontend("SourcesTestRunner.startDebuggerTest(reply);", step2);

      function step2() {
        webInspector.inspectedWindow.onResourceAdded.addListener(onResourceAdded);
        webInspector.inspectedWindow.eval("addResource()");
      }

      function onResourceAdded(resource) {
        if (resource.url.startsWith('debugger://'))
          return;
        if (resource.url.indexOf("test_func") === -1)
          return;
        output("resource added:");
        dumpObject(Array.prototype.slice.call(arguments), { url: "url" });
        webInspector.inspectedWindow.onResourceAdded.removeListener(onResourceAdded);

        evaluateOnFrontend("SourcesTestRunner.resumeExecution(reply);", nextTest);
      }
    },

    function extension_testOpenResourceHandler(nextTest) {
      function handleOpenResource(resource, lineNumber) {
        output("handleOpenResource() invoked [this should only appear once!]: ");
        dumpObject(Array.prototype.slice.call(arguments), { url: "url" });
        webInspector.panels.setOpenResourceHandler(null);
        evaluateOnFrontend("TestRunner.clickOnURL(); reply()", nextTest);
      }
      webInspector.panels.setOpenResourceHandler(handleOpenResource);
      webInspector.inspectedWindow.eval("logMessage()", function() {
        evaluateOnFrontend("TestRunner.clickOnURL();");
        evaluateOnFrontend("Components.Linkifier.Linkifier.linkHandlerSetting().set('test extension'); TestRunner.clickOnURL();");
      });
    },
  ]);
})();
