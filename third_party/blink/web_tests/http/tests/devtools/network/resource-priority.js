// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests resource priorities.\n`);
  await TestRunner.showPanel('network');
  await TestRunner.evaluateInPagePromise(`
      function sendSyncScriptRequest()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.contentDocument.write('<html><body><script src="http://localhost:8000/devtools/network/resources/empty-script.js?sync"></s' + 'cript>;</body></html>');
      }

      function sendAsyncScriptRequest()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.contentDocument.write('<html><body><script src="http://localhost:8000/devtools/network/resources/empty-script.js?async" async></s' + 'cript>;</body></html>');
      }

      function sendModuleScriptRequest()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.contentDocument.write('<html><body><script type="module" src="../network/resources/module1.js"></s' + 'cript>;</body></html>');
      }

      function sendXHRSync()
      {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "../network/resources/empty.html?xhr-sync", false);
          xhr.send();
      }

      function sendXHRAsync()
      {
          var xhr = new XMLHttpRequest();
          xhr.open("GET", "resources/empty.html?xhr-async");
          xhr.send();
      }

      function sendImageRequest()
      {
          var img = document.createElement("img");
          img.src = "resources/abe.png?image";
          document.body.appendChild(img);
      }

      function sendStyleRequest()
      {
          var link = document.createElement("link");
          link.rel = "stylesheet";
          link.href = "resources/style.css?style";
          document.head.appendChild(link);
      }

      function sendScriptRequestPrecededByImage()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.srcdoc = '<html><body><img src="resources/abe.png?precedingScript">'
              + '<script src="http://localhost:8000/devtools/network/resources/empty-script.js?precededByImage"></s'
              + 'cript>;</body></html>';
      }

      function sendScriptRequestPrecededByPreloadedImage()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.srcdoc = '<html><body><link href="resources/abe.png?preloaded" rel=preload as=image>'
              + '<link rel="stylesheet" type="text/css" href="http://localhost:8000/devtools/network/resources/style.css?precededByPreloadedImage">'
              + '<img src="resources/abe.png?preloaded"></body></html>';
      }

      function sendStyleRequestPrecededByImage()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.srcdoc = '<html><body><img src="resources/abe.png?precedingStyle">'
              + '<link rel="stylesheet" type="text/css" href="http://localhost:8000/devtools/network/resources/style.css?precededByImage">'
              + '</body></html>';
      }

      function sendStyleRequestPrecededByPreloadedImage()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.srcdoc = '<html><body><link href="resources/abe.png?preloaded" rel=preload as=image>'
              + '<script src="http://localhost:8000/devtools/network/resources/empty-script.js?precededByPreloadedImage"></s'
              + 'cript><img src="resources/abe.png?preloaded"></body></html>';
      }

      function sendScriptsFromDocumentWriteAfterImage()
      {
          var iframe = document.createElement("iframe");
          document.body.appendChild(iframe);
          iframe.srcdoc = '<html><body><img src="resources/abe.png?precedingDocWrite">'
              + '<script src="resources/docwrite.js"></s'
              + 'cript></body></html>';
      }

      function createIFrame()
      {
          var iframe = document.createElement("iframe");
          iframe.src = "resources/empty.html?iframe";
          document.head.appendChild(iframe);
      }
  `);

  var actions = [
    {'fn': 'sendSyncScriptRequest', 'requests': 1},
    {'fn': 'sendAsyncScriptRequest', 'requests': 1},
    {'fn': 'sendModuleScriptRequest', 'requests': 2},
    {'fn': 'sendScriptRequestPrecededByImage', 'requests': 2},
    {'fn': 'sendScriptRequestPrecededByPreloadedImage', 'requests': 2},
    {'fn': 'sendStyleRequestPrecededByImage', 'requests': 2},
    {'fn': 'sendStyleRequestPrecededByPreloadedImage', 'requests': 2},
    {'fn': 'sendXHRSync', 'requests': 1},
    {'fn': 'sendXHRAsync', 'requests': 1},
    {'fn': 'sendImageRequest', 'requests': 1},
    {'fn': 'sendStyleRequest', 'requests': 1},
    {'fn': 'createIFrame', 'requests': 1},
    {'fn': 'sendScriptsFromDocumentWriteAfterImage', 'requests': 5},
  ];
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestStarted, onRequestStarted);

  var nextAction = 0;
  var expectedRequestCount = 0;
  performNextAction();

  function performNextAction() {
    if (nextAction >= actions.length) {
      TestRunner.networkManager.removeEventListener(SDK.NetworkManager.Events.RequestStarted, onRequestStarted);
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult(actions[nextAction].fn);
    expectedRequestCount = actions[nextAction].requests;
    TestRunner.evaluateInPage(actions[nextAction++].fn + '()');
  }
  function onRequestStarted(event) {
    var request = event.data;
    TestRunner.addResult('Request: ' + request.name() + ' priority: ' + request.initialPriority());
    expectedRequestCount--;
    if (expectedRequestCount < 1)
      performNextAction();
  }
})();
