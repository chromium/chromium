// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(`Tests that info explanations are placed after regular explanations.\n`);
  await TestRunner.showPanel('security');

  const pageVisibleSecurityState = new Security.SecurityModel.PageVisibleSecurityState(
    Protocol.Security.SecurityState.Secure,
    {
      protocol: 'TLS 1.0',
      keyExchange: 'RSA',
      keyExchangeGroup: null,
      cipher: 'AES_128_CBC',
      mac: 'HMAC-SHA1',
      certificate: ['BASE64CERTIFICATE'],
      subjectName: 'testexample.com',
      issuer: 'Issuer',
      // set valid time frame to avoid certificate expiring soon message
      validFrom: new Date(Date.now()).setHours(-100),
      validTo: new Date(Date.now()).setHours(100),
      certifcateHasWeakSignature: false,
      certificateHasSha1SignaturePresent: false,
      modernSSL: false,
      obsoleteSslProtocol: false,
      obsoleteSslKeyExchange: false,
      obsoleteSslCipher: false,
      obsoleteSslSignature: false,
    },
    null,
    ['pkp-bypassed']
  );

  TestRunner.mainTarget.model(Security.SecurityModel.SecurityModel)
      .dispatchEventToListeners(
        Security.SecurityModel.Events.VisibleSecurityStateChanged,
        pageVisibleSecurityState);

  var request = SDK.NetworkRequest.NetworkRequest.create(
      0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request);

  var explanations =
      Security.SecurityPanel.SecurityPanel.instance().mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);
  TestRunner.completeTest();
})();
