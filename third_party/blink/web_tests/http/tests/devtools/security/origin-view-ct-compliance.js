// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(
      `Tests that the panel includes Certificate Transparency compliance status\n`);
  await TestRunner.showPanel('security');

  var request1 = SDK.NetworkRequest.NetworkRequest.create(
      0, 'https://foo.test/', 'https://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
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
  securityDetails.certificateTransparencyCompliance = Protocol.Network.CertificateTransparencyCompliance.Compliant;
  request1.setSecurityDetails(securityDetails);
  SecurityTestRunner.dispatchRequestFinished(request1);

  Security.SecurityPanel.SecurityPanel.instance().sidebarTree.elementsByOrigin.get('https://foo.test').select();

  TestRunner.addResult('Panel on origin view:');
  TestRunner.dumpDeepInnerHTML(Security.SecurityPanel.SecurityPanel.instance().visibleView.contentElement);

  TestRunner.completeTest();
})();
