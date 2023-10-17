// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(`Tests that the security details for an origin are updated if its security state changes.\n`);
  await TestRunner.showPanel('security');

  // Add a request without security details.
  const request1 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'https://foo.test/foo.jpg', 'https://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Unknown);
  SecurityTestRunner.dispatchRequestFinished(request1);

  // Add an unrelated request.
  const request2 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'https://bar.test/bar.jpg', 'https://bar.test', 0, 0, null);
  request2.setSecurityState(Protocol.Security.SecurityState.Unknown);
  SecurityTestRunner.dispatchRequestFinished(request2);

  // Add a request to the first origin, this time including security details.
  const request3 = SDK.NetworkRequest.NetworkRequest.create(
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

  // Add a request with both keyExchange and keyExchangeGroup (TLS 1.2 ECDHE), and with empty MAC (an AEAD cipher).
  const request4 = SDK.NetworkRequest.NetworkRequest.create(0, 'https://ecdhe.foo.test/foo2.jpg', 'https://ecdhe.foo.test', 0, 0, null);
  request4.setSecurityState(Protocol.Security.SecurityState.Secure);
  securityDetails = {};
  securityDetails.protocol = 'TLS 1.2';
  securityDetails.keyExchange = 'ECDSA_RSA';
  securityDetails.keyExchangeGroup = 'X25519';
  securityDetails.cipher = 'AES-128-GCM';
  securityDetails.mac = '';
  securityDetails.serverSignatureAlgorithm = 0x0804;  // rsa_pss_rsae_sha256
  securityDetails.subjectName = 'ecdhe.foo.test';
  securityDetails.sanList = ['ecdhe.foo.test'];
  securityDetails.issuer = 'Super CA';
  securityDetails.validFrom = 1490000000;
  securityDetails.validTo = 2000000000;
  securityDetails.CertificateId = 0;
  securityDetails.signedCertificateTimestampList = [];
  securityDetails.certificateTransparencyCompliance = Protocol.Network.CertificateTransparencyCompliance.Unknown;
  request4.setSecurityDetails(securityDetails);
  SecurityTestRunner.dispatchRequestFinished(request4);

  // Add a request with only keyExchangeGroup (TLS 1.3).
  const request5 = SDK.NetworkRequest.NetworkRequest.create(0, 'https://tls13.foo.test/foo2.jpg', 'https://tls13.foo.test', 0, 0, null);
  request5.setSecurityState(Protocol.Security.SecurityState.Secure);
  securityDetails = {};
  securityDetails.protocol = 'TLS 1.3';
  securityDetails.keyExchange = '';
  securityDetails.keyExchangeGroup = 'X25519';
  securityDetails.cipher = 'AES-128-GCM';
  securityDetails.mac = '';
  securityDetails.serverSignatureAlgorithm = 0x0804;  // rsa_pss_rsae_sha256
  securityDetails.subjectName = 'tls13.foo.test';
  securityDetails.sanList = ['tls13.foo.test'];
  securityDetails.issuer = 'Super CA';
  securityDetails.validFrom = 1490000000;
  securityDetails.validTo = 2000000000;
  securityDetails.CertificateId = 0;
  securityDetails.signedCertificateTimestampList = [];
  securityDetails.certificateTransparencyCompliance = Protocol.Network.CertificateTransparencyCompliance.Unknown;
  request5.setSecurityDetails(securityDetails);
  SecurityTestRunner.dispatchRequestFinished(request5);

  // Add a request with ECH.
  const request6 = SDK.NetworkRequest.NetworkRequest.create(0, 'https://ech.foo.test/foo2.jpg', 'https://ech.foo.test', 0, 0, null);
  request6.setSecurityState(Protocol.Security.SecurityState.Secure);
  securityDetails = {};
  securityDetails.protocol = 'TLS 1.3';
  securityDetails.keyExchange = '';
  securityDetails.keyExchangeGroup = 'X25519';
  securityDetails.cipher = 'AES-128-GCM';
  securityDetails.mac = '';
  securityDetails.serverSignatureAlgorithm = 0x0804;  // rsa_pss_rsae_sha256
  securityDetails.encryptedClientHello = true;
  securityDetails.subjectName = 'ech.foo.test';
  securityDetails.sanList = ['ech.foo.test'];
  securityDetails.issuer = 'Super CA';
  securityDetails.validFrom = 1490000000;
  securityDetails.validTo = 2000000000;
  securityDetails.CertificateId = 0;
  securityDetails.signedCertificateTimestampList = [];
  securityDetails.certificateTransparencyCompliance = Protocol.Network.CertificateTransparencyCompliance.Unknown;
  request6.setSecurityDetails(securityDetails);
  SecurityTestRunner.dispatchRequestFinished(request6);

  TestRunner.addResult('Sidebar Origins --------------------------------');
  SecurityTestRunner.dumpSecurityPanelSidebarOrigins();

  const origins = [
      "https://foo.test",
      "https://ecdhe.foo.test",
      "https://tls13.foo.test",
      "https://ech.foo.test",
  ];
  for (const origin of origins) {
    Security.SecurityPanel.SecurityPanel.instance().sidebarTree.elementsByOrigin.get(origin).select();
    TestRunner.addResult('Origin view (' + origin + ') ' + '-'.repeat(33 - origin.length));
    TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().visibleView.contentElement);
  }

  TestRunner.completeTest();
})();
