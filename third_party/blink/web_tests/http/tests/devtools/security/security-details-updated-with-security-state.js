// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the security details for an origin are updated if its security state changes.\n`);
  await TestRunner.loadTestModule('security_test_runner');
  await TestRunner.showPanel('security');

  // Add a request without security details.
  const request1 = SDK.NetworkRequest.create(
      0, 'https://foo.test/foo.jpg', 'https://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Unknown);
  SecurityTestRunner.dispatchRequestFinished(request1);

  // Add an unrelated request.
  const request2 = SDK.NetworkRequest.create(
      0, 'https://bar.test/bar.jpg', 'https://bar.test', 0, 0, null);
  request2.setSecurityState(Protocol.Security.SecurityState.Unknown);
  SecurityTestRunner.dispatchRequestFinished(request2);

  // Add a request to the first origin, this time including security details.
  const request3 = SDK.NetworkRequest.create(
      0, 'https://foo.test/foo2.jpg', 'https://foo.test', 0, 0, null);
  request3.setSecurityState(Protocol.Security.SecurityState.Secure);
  let securityDetails = {};
  securityDetails.protocol = 'TLS 1.2';
  securityDetails.keyExchange = 'Key_Exchange';
  securityDetails.keyExchangeGroup = '';
  securityDetails.cipher = 'Cypher';
  securityDetails.mac = 'Mac';
  securityDetails.subjectName = 'foo.test';
  securityDetails.sanList = ['foo.test', '*.test'];
  securityDetails.issuer = 'Super CA';
  securityDetails.validFrom = 1490000000;
  securityDetails.validTo = 2000000000;
  securityDetails.CertificateId = 0;
  securityDetails.signedCertificateTimestampList = [];
  securityDetails.certificateTransparencyCompliance = Protocol.Network.CertificateTransparencyCompliance.Unknown;
  request3.setSecurityDetails(securityDetails);
  SecurityTestRunner.dispatchRequestFinished(request3);

  TestRunner.addResult('Sidebar Origins --------------------------------');
  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();

  Security.SecurityPanel.instance().sidebarTree.elementsByOrigin.get('https://foo.test').select();

  TestRunner.addResult('Origin view ------------------------------------');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.instance().visibleView.contentElement);

  TestRunner.completeTest();
})();
