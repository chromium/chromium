// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';

(async function() {
  TestRunner.addResult(`Verifies that RequestHTMLView's iframe src is URI encoded`);
  await TestRunner.showPanel('network');

  const dataUrl = `data:text/html,<body><p>octothorp: #</p><p>hello world!<p>%3Cp%3EURI%20encoded%20tag!%3C%2Fp%3E</body>`;
  const requestHtmlView = new Network.RequestHTMLView.RequestHTMLView(dataUrl);
  requestHtmlView.wasShown();
  const iframe = requestHtmlView.contentElement.firstChild;

  TestRunner.addResult('iframe.src: ' + iframe.src);
  TestRunner.addResult('decoded iframe.src: ' + decodeURIComponent(iframe.src));

  TestRunner.completeTest();
})();
