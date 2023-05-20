// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests XSL stylsheet content. http://crbug.com/603806\n`);
  await TestRunner.showPanel('network');
  NetworkTestRunner.recordNetwork();
  await TestRunner.evaluateInPageAsync(`
    var iframe = document.createElement("iframe");
    document.body.appendChild(iframe);
    iframe.src = "resources/xml-with-stylesheet.xml";
    new Promise(f => iframe.addEventListener('load', f))
  `);

  var resultsOutput = [];
  const requests = NetworkTestRunner.networkRequests();
  for (const request of requests) {
    const content = await TestRunner.NetworkAgent.getResponseBody(request.requestId());
    var output = [];
    output.push(request.url());
    output.push('resource.type: ' + request.resourceType());
    output.push('resource.content: ' + content);
    resultsOutput.push(output.join('\n'));
  }
  TestRunner.addResult(resultsOutput.sort().join('\n'));
  TestRunner.completeTest();
})();
