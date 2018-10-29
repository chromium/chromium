// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Verifies that RequestHTMLView's iframe src is URI encoded`);
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  const dataUrl = `data:text/html,<body><p>hello world!<p>%3Cp%3EURI%20encoded%20tag!%3C%2Fp%3E</body>`;
  const requestHtmlView = new Network.RequestHTMLView(dataUrl);
  requestHtmlView.wasShown();
  const iframe = requestHtmlView.contentElement.firstChild;

  TestRunner.addResult('iframe.src: ' + iframe.src);
  TestRunner.addResult('decoded iframe.src: ' + decodeURIComponent(iframe.src));

  TestRunner.completeTest();
})();
