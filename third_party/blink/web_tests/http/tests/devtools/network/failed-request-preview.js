// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ApplicationTestRunner} from 'application_test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Verifies that network request previews don't have src set when the request fails`);
  await TestRunner.showPanel('network');

  SDK.NetworkManager.MultitargetNetworkManager.instance().requestConditions.conditionsEnabled = true;
  TestRunner.networkManager.addEventListener(
    SDK.NetworkManager.Events.RequestFinished, (event) => {
      const request = event.data;
      TestRunner.addResult('request.url(): ' + request.url());
      TestRunner.addResult('request.failed: ' + request.failed);
      TestRunner.addResult('request.blockedReason: ' + request.blockedReason());

      const previewImage = document.createElement('img');
      previewImage.classList.add('image-network-icon-preview');
      request.populateImageSource(previewImage).then(() => {
        TestRunner.addResult('previewImage.src: ' + previewImage.src);
        TestRunner.completeTest();
      });
    });

  SDK.NetworkManager.MultitargetNetworkManager.instance().requestConditions.add(
    SDK.NetworkManager.RequestCondition.createFromSetting({url: '*://*:*', enabled: true})
  );

  NetworkTestRunner.makeXHR('GET', 'http://localhost:8000');
})();
