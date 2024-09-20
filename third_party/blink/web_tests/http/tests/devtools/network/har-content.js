// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests conversion of Inspector's resource representation into HAR format.\n`);
  await TestRunner.showPanel('network');

  await TestRunner.NetworkAgent.setCacheDisabled(true);

  NetworkTestRunner.makeSimpleXHR('GET', 'resources/initiator.css', false, sendBinaryRequest);

  function sendBinaryRequest() {
    NetworkTestRunner.makeSimpleXHR('GET', 'resources/binary.data', false, makeHAR);
  }

  async function makeHAR() {
    var stream = new TestRunner.StringOutputStream(onSaved);
    var progress = new Common.Progress.Progress();
    await NetworkTestRunner.writeHARLog(
        stream,
        NetworkTestRunner.networkRequests(),
        {sanitize: false},
        progress);
    progress.done();
    stream.close();
  }

  function dumpContent(content) {
    if (!content) {
      TestRunner.addResult('    NOT FOUND');
      return;
    }
    var propertyNames = Object.keys(content);
    propertyNames.sort();
    for (var prop of propertyNames)
      TestRunner.addResult(`    ${prop}: ` + JSON.stringify(content[prop]));
  }

  function onSaved(data) {
    var har = JSON.parse(data);

    TestRunner.addResult('initiator.css:');
    dumpContent(findEntry(har, /\/initiator\.css$/).response.content);

    TestRunner.addResult('');

    TestRunner.addResult('binary.data:');
    dumpContent(findEntry(har, /\/binary\.data$/).response.content);

    TestRunner.completeTest();
  }

  function findEntry(har, regexp) {
    var entry = har.log.entries.find(entry => regexp.test(entry.request.url));
    if (entry)
      return entry;
    TestRunner.addResult('FAIL: can\'t find resource for ' + regexp);
    return null;
  }
})();
