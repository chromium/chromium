// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
    `Tests content is moved from cached resource to resource agent's data storage when cached resource is destroyed.\n`
  );

  await TestRunner.loadModule('network_test_runner');
  await TestRunner.loadModule('console_test_runner');
  await TestRunner.showPanel('network');

  await TestRunner.evaluateInPagePromise(`
    var image;;
    function loadFirstImage() {
        image = new Image();
        image.onload = firstImageLoaded;
        document.body.appendChild(image);
        image.src = "resources/resource.php?type=image&random=1&size=400";
    };
    function firstImageLoaded()
    {
        console.log("Done1.");
    };
    function loadSecondImage() {
        image.onload = secondImageLoaded;
        image.src = "resources/resource.php?type=image&random=1&size=200";
    };
    function secondImageLoaded()
    {
        console.log("Done2.");
    };
  `);

  var imageRequest;
  NetworkTestRunner.recordNetwork();
  ConsoleTestRunner.addConsoleSniffer(step2);
  TestRunner.evaluateInPage('loadFirstImage()');

  function step2() {
    imageRequest = NetworkTestRunner.networkRequests().pop();
    imageRequest.requestContent().then(step3);
  }

  var originalContentLength;

  function step3({ content, error, isEncoded }) {
    TestRunner.addResult(imageRequest.url());
    TestRunner.addResult('request.type: ' + imageRequest.resourceType());
    TestRunner.addResult('request.content.length after requesting content: ' + content.length);
    originalContentLength = content.length;
    TestRunner.assertTrue(content.length > 0, 'No content before destroying CachedResource.');
    TestRunner.addResult('Releasing cached resource.');
    ConsoleTestRunner.addConsoleSniffer(step4);
    TestRunner.evaluateInPage('loadSecondImage()');
  }

  function step4(msg) {
    TestRunner.NetworkAgent.setCacheDisabled(true).then(step5);
  }

  function step5() {
    TestRunner.NetworkAgent.setCacheDisabled(false).then(step6);
  }

  function step6() {
    delete imageRequest._contentData;
    imageRequest.requestContent().then(step7);
  }

  function step7({ content, error, isEncoded }) {
    TestRunner.addResult('request.content.length after requesting content: ' + content.length);
    TestRunner.assertTrue(
      content.length === originalContentLength,
      'Content changed after cached resource was destroyed'
    );
    TestRunner.completeTest();
  }
})();
