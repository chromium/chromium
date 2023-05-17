// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ExtensionsTestRunner} from 'extensions_test_runner';

(async function() {
  TestRunner.addResult(`Tests WebInspector extension API returns valid data for redirected resources\n`);
  await TestRunner.navigatePromise('resources/extensions-network-redirect.html');
  ExtensionsTestRunner.runExtensionTests([
      function extension_doRequest(force, callback)
      {
          function callbackWrapper(request)
          {
              var lastCallback = !request || /\?redirected=true$/.test(request.request.url);
              if (lastCallback)
                  webInspector.network.onRequestFinished.removeListener(callbackWrapper);
              callback(request, lastCallback);
          }
          webInspector.network.onRequestFinished.addListener(callbackWrapper);
          webInspector.inspectedWindow.eval("doRequest(" + force + ")", function(result) {
              if (result)
                  callbackWrapper(null);
          });
      },

      function extension_testGetRedirectRequestContent(nextTest)
      {
          function onRequestFinished(request, lastCallback)
          {
              if (!lastCallback)
                  return;
              extension_getRequestByUrl([ /redirect-methods-result.php\?status=302$/ ], function(request) {
                  request.getContent(onContent);
              });
          }
          function onContent(content, encoding)
          {
              output("content: " + content + ", encoding: " + encoding);
              nextTest();
          }
          extension_doRequest(false, onRequestFinished);
      },

      function extension_testRedirectRequestInHAR(nextTest)
      {
          function onRequestFinished(resource, lastCallback)
          {
              if (lastCallback)
                  webInspector.network.getHAR(onHAR);
          }
          function onHAR(har)
          {
              var entries = har.entries;
              var urls = [];
              for (var i = 0; i < entries.length; ++i) {
                var url = entries[i].request.url;
                // Workaround for GTK DRT that requests favicon.ico along with
                // the page.
                if (!/\/favicon\.ico$/.test(url))
                  urls.push(url);
              }
              urls.sort();
              output("Requests in HAR:\n" + urls.join("\n"));
              nextTest();
          }
          extension_doRequest(false, onRequestFinished);
      },

      function extension_testRedirectRequestFinished(nextTest)
      {
          function onRequestFinished(request, lastCallback)
          {
              output("Finished request: " + request.request.url);
              if (lastCallback)
                  nextTest();
          }
          extension_doRequest(true, onRequestFinished);
      },
  ]);
})();
