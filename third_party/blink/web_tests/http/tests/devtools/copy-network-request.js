// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Network from 'devtools/panels/network/network.js';
import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  'use strict';
  TestRunner.addResult(`Tests curl command generation\n`);
  await TestRunner.showPanel('network');

  var logView = Network.NetworkPanel.NetworkPanel.instance().networkLogView;
  const BROWSER = 0;
  const NODE_JS = 1;

  function newRequest(isBlob, headers, data, opt_url, method = null) {
    var request = SDK.NetworkRequest.NetworkRequest.create(
        0,
        (isBlob === true ? 'blob:' : '') +
            (opt_url || 'http://example.org/path'),
        0, 0, 0);
    request.requestMethod = method || (data ? 'POST' : 'GET');
    var headerList = [];
    if (headers) {
      for (var i in headers)
        headerList.push({name: i, value: headers[i]});
    }
    request.setRequestFormData(!!data, data);
    if (data)
      headerList.push({name: 'Content-Length', value: data.length});
    request.setRequestHeaders(headerList);
    return request;
  }

  async function dumpRequest(headers, data, opt_url, method) {
    const request = newRequest(false, headers, data, opt_url, method);
    var curlWin = await Network.NetworkLogView.NetworkLogView.generateCurlCommand(request, 'win');
    var curlUnix = await Network.NetworkLogView.NetworkLogView.generateCurlCommand(request, 'unix');
    var powershell = await logView.generatePowerShellCommand(request);
    var fetchForBrowser = await logView.generateFetchCall(request, BROWSER);
    var fetchForNodejs = await logView.generateFetchCall(request, NODE_JS);
    TestRunner.addResult(`cURL Windows:\n${curlWin}\n\n`);
    TestRunner.addResult(`cURL Unix:\n${curlUnix}\n\n`);
    TestRunner.addResult(`Powershell:\n${powershell}\n\n`);
    TestRunner.addResult(`fetch (for browser):\n${fetchForBrowser}\n\n`);
    TestRunner.addResult(`fetch (for nodejs):\n${fetchForNodejs}\n\n`);
  }

  async function dumpMultipleRequests(blobPattern) {
    const header = {'Content-Type': 'foo/bar'};
    const data = 'baz';
    const allRequests = blobPattern.map(isBlob => newRequest(isBlob, header, data));

    var allCurlWin = await logView.generateAllCurlCommand(allRequests, 'win');
    var allCurlUnix = await logView.generateAllCurlCommand(allRequests, 'unix');
    var allPowershell = await logView.generateAllPowerShellCommand(allRequests);
    var allFetchForBrowser = await logView.generateAllFetchCall(allRequests, BROWSER);
    var allFetchForNodejs = await logView.generateAllFetchCall(allRequests, NODE_JS);
    TestRunner.addResult(`cURL Windows:\n${allCurlWin}\n\n`);
    TestRunner.addResult(`cURL Unix:\n${allCurlUnix}\n\n`);
    TestRunner.addResult(`Powershell:\n${allPowershell}\n\n`);
    TestRunner.addResult(`fetch (for browser):\n${allFetchForBrowser}\n\n`);
    TestRunner.addResult(`fetch (for nodejs):\n${allFetchForNodejs}\n\n`);
  }

  await dumpRequest({});
  await dumpRequest({}, '123');
  await dumpRequest({'Content-Type': 'application/x-www-form-urlencoded'}, '1&b');
  await dumpRequest({'Content-Type': 'application/json'}, '{"a":1}');
  await dumpRequest(
      {'Content-Type': 'application/binary'}, '1234\r\n00\x02\x03\x04\x05\'"!');
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '1234\r\n\x0100\x02\x03\x04\x05\'"!');
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '%OS%\r\n%%OS%%\r\n"\\"\'$&!');  // Ensure %OS% for windows is not env evaled
  await dumpRequest(
      {'Content-Type': 'application/binary'},
      '!@#$%^&*()_+~`1234567890-=[]{};\':",./\r<>?\r\nqwer\nt\n\nyuiopasdfghjklmnbvcxzQWERTYUIOPLKJHGFDSAZXCVBNM');
  await dumpRequest({'Content-Type': 'application/binary'}, '\x7F\x80\x90\xFF\u0009\u0700');
  await dumpRequest(
      {}, null,
      'http://labs.ft.com/?querystring=[]{}');  // Character range symbols must be escaped (http://crbug.com/265449).
  await dumpRequest({'Content-Type': 'application/binary'}, '%PATH%$PATH');
  await dumpRequest({':host': 'h', 'version': 'v'});
  await dumpRequest({'Cookie': '_x=fdsfs; aA=fdsfdsf; FOO=ID=BAR:BAZ=FOO:F=d:AO=21.212.2.212-:A=dsadas8d9as8d9a8sd9sa8d9a; AAA=117'});
  await dumpRequest({}, null, null, '|evilcommand|');
  await dumpRequest({'Content-Type':'application/x-www-form-urlencoded'}, '@/etc/passwd');
  await dumpRequest({'Referer' : 'https://example.com'});
  await dumpRequest({'No-Value-Header': ''});

  await dumpMultipleRequests([]);
  await dumpMultipleRequests([true]);
  await dumpMultipleRequests([true, true]);
  await dumpMultipleRequests([false]);
  await dumpMultipleRequests([true, false]);

  TestRunner.completeTest();
})();
