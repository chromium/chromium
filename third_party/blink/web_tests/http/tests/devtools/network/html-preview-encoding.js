// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(
    'Verifies that html resources are previewed with the encoding set in their content-type header');
  await TestRunner.showPanel('network');

  async function runEncodingTest(url, nextTestCallback) {
    const fetchScript = `fetch('${url}')`;
    TestRunner.addResult('fetchScript: ' + fetchScript);
    TestRunner.evaluateInPage(fetchScript);

    const request = await TestRunner.networkManager.once(SDK.NetworkManager.Events.RequestFinished);

    const previewView = new Network.RequestPreviewView(request);
    previewView.wasShown();
    const htmlPreviewView = await previewView.contentViewPromise;
    htmlPreviewView.wasShown();
    const iframe = htmlPreviewView.contentElement.firstChild;
    TestRunner.addResult('iframe.src: ' + iframe.src);

    nextTestCallback();
  }

  TestRunner.runTestSuite([
      function testUTF8(next) {
        runEncodingTest('/devtools/network/resources/content-type-utf8.php', next);
      },
      function testISO(next) {
        runEncodingTest('/devtools/network/resources/content-type-iso.php', next);
      },
      function testNoCharset(next) {
        runEncodingTest('/devtools/network/resources/content-type-none.php', next);
      },
    ]);
})();
