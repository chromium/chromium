// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests overriding user agent via WebInspector extension API\n`);
  await TestRunner.navigatePromise('resources/extensions-useragent.html');
  await ExtensionsTestRunner.runExtensionTests([
    function extension_testUserAgent(nextTest)
    {
        const requestsToCheck = [
            "extensions-useragent.html",
            "xhr-exists.html"
        ];
        var requestCount = 0;
        var queuedOutput = [];

        function onRequestFinished(request)
        {
            var url = request.request.url.replace(/^.*[/]/, "");
            if (requestsToCheck.indexOf(url) < 0)
                return;

            queuedOutput.push("user-agent header for " + url + ": " + getHeader(request.request.headers, "user-agent"));
            if (++requestCount < requestsToCheck.length)
                return;
            webInspector.network.onRequestFinished.removeListener(onRequestFinished);
            webInspector.inspectedWindow.eval("navigator.userAgent", onEval);
        }
        function getHeader(headers, name)
        {
            for (var i = 0; i < headers.length; ++i) {
                if (headers[i].name.toLowerCase() === name)
                    return headers[i].value;
            }
        }
        function onEval(result)
        {
            queuedOutput.push("navigator.userAgent: " + result);
            webInspector.inspectedWindow.eval("undefined", cleanUp);
        }
        function cleanUp()
        {
            evaluateOnFrontend("TestRunner.waitForPageLoad(reply)", onPageLoaded);
            webInspector.inspectedWindow.reload({userAgent: ""});
        }
        function onPageLoaded()
        {
            queuedOutput.sort();
            for (var i = 0; i < queuedOutput.length; ++i)
                output(queuedOutput[i]);
            nextTest();
        }

        webInspector.network.onRequestFinished.addListener(onRequestFinished);
        webInspector.inspectedWindow.reload({ignoreCache: true, userAgent: "Mozilla/4.0 (compatible; WebInspector Extension User-Agent override; RSX-11M)"});
    }
  ]);
})();
