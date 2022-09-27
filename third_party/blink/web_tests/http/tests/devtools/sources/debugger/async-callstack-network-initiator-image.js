// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests asynchronous network initiator for image loaded from JS.\n`);
  await TestRunner.loadLegacyModule('sources');
  await TestRunner.loadTestModule('sources_test_runner');
  await TestRunner.loadLegacyModule('console');
  await TestRunner.loadTestModule('console_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.loadLegacyModule('components');
  await TestRunner.showPanel('sources');
  await TestRunner.evaluateInPagePromise(`
      function testFunction()
      {
          var image = document.createElement("img");
          image.src = "resources/image.png";
          document.body.appendChild(image);
      }
  `);

  TestRunner.evaluateInPage('testFunction()');
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestFinished, requestFinished);

  async function requestFinished(event) {
    if (!event.data.url().endsWith('resources/image.png'))
      return;

    var initiatorInfo =
        NetworkTestRunner.networkLog().initiatorInfoForRequest(event.data);
    var element = new Components.Linkifier().linkifyScriptLocation(
        TestRunner.mainTarget, initiatorInfo.scriptId, initiatorInfo.url, initiatorInfo.lineNumber,
        initiatorInfo.columnNumber - 1);
    // Linkified script locations may contain an unresolved live locations.
    await TestRunner.waitForPendingLiveLocationUpdates();
    TestRunner.addResult(element.textContent);
    TestRunner.completeTest();
  }
})();
