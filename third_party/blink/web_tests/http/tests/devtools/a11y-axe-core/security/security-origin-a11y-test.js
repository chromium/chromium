// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';
import {AxeCoreTestRunner} from 'axe_core_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';
import * as Security from 'devtools/panels/security/security.js';

(async function() {
  await TestRunner.showPanel('security');

  const request1 = new SDK.NetworkRequest.NetworkRequest(0, 'https://foo.test/', 'https://foo.test', 0, 0, null);
  request1.setSecurityState(Protocol.Security.SecurityState.Secure);
  const securityDetails = {
    protocol : 'TLS 1.2',
    keyExchange : 'Key_Exchange',
    keyExchangeGroup : '',
    cipher : 'Cypher',
    mac : 'Mac',
    subjectName : 'foo.test',
    sanList : ['foo.test', '*.test'],
    issuer : 'Super CA',
    validFrom : 1490000000,
    validTo : 2000000000,
    CertificateId : 0,
    signedCertificateTimestampList : [],
    certificateTransparencyCompliance : Protocol.Network.CertificateTransparencyCompliance.Compliant
  };

  request1.setSecurityDetails(securityDetails);
  SecurityTestRunner.dispatchRequestFinished(request1);
  const securityPanel = Security.SecurityPanel.SecurityPanel.instance();

  securityPanel.showOrigin('https://foo.test');
  await AxeCoreTestRunner.runValidation(securityPanel.contentElement);

  TestRunner.completeTest();
})();
