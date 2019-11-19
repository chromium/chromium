// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that info explanations are placed after regular explanations.\n`);
  await TestRunner.loadModule('security_test_runner');
  await TestRunner.showPanel('security');

  // Explanations from https://cbc.badssl.com/ as of 2016-06-13.
  // We explicitly place the explanation with the security state "info"
  // first to make sure it gets reordered.
  var explanations = [
    {
      description: 'Public-key pinning was bypassed by a local root certificate.',
      securityState: 'info',
      summary: 'Public-Key Pinning Bypassed',
      certificate: []
    },
    {
      description: 'The connection to this site is using a valid, trusted server certificate.',
      securityState: 'secure',
      summary: 'Valid Certificate',
      certificate: ['BASE64CERTIFICATE']
    },
    {
      description:
          'The connection to this site uses a strong protocol (TLS 1.2), a strong key exchange (ECDHE_RSA), and an obsolete cipher (AES_256_CBC with HMAC-SHA1).',
      securityState: 'secure',
      summary: 'Obsolete Connection Settings',
      certificate: []
    },
    {
      description: 'All resources on this page are served securely.',
      securityState: 'secure',
      summary: 'Secure resources',
      certificate: []
    }
  ];

  TestRunner.mainTarget.model(Security.SecurityModel)
      .dispatchEventToListeners(
          Security.SecurityModel.Events.SecurityStateChanged,
          new Security.PageSecurityState(
              Protocol.Security.SecurityState.Secure, explanations, null));

  var request = new SDK.NetworkRequest(0, 'http://foo.test', 'https://foo.test', 0, 0, null);
  SecurityTestRunner.dispatchRequestFinished(request);

  var explanations =
      Security.SecurityPanel._instance()._mainView.contentElement.getElementsByClassName('security-explanation');
  for (var i = 0; i < explanations.length; i++)
    TestRunner.dumpDeepInnerHTML(explanations[i]);
  TestRunner.completeTest();
})();
