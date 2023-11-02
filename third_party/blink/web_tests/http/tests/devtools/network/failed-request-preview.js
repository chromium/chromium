// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that network request previews don't have src set when the request fails`);
  await TestRunner.loadLegacyModule('console'); await TestRunner.loadTestModule('application_test_runner');
  await TestRunner.loadTestModule('network_test_runner');
  await TestRunner.showPanel('network');

  SDK.multitargetNetworkManager.setBlockingEnabled(true);
  TestRunner.networkManager.addEventListener(
    SDK.NetworkManager.Events.RequestFinished, (event) => {
      const request = event.data;
      TestRunner.addResult('request.url(): ' + request.url());
      TestRunner.addResult('request.failed: ' + request.failed);

      const previewImage = document.createElement('img');
      previewImage.classList.add('image-network-icon-preview');
      request.populateImageSource(previewImage).then(() => {
        TestRunner.addResult('previewImage.src: ' + previewImage.src);
        TestRunner.completeTest();
      });
    });

  SDK.multitargetNetworkManager.setBlockedPatterns([
    {url: '*', enabled: true}
  ]);

  NetworkTestRunner.makeXHR('GET', 'http://localhost:8000');
})();
