// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult('Verifies that HAR exports have correct POST data');
  await TestRunner.loadModule('network_test_runner');
  await TestRunner.showPanel('network');

  await TestRunner.NetworkAgent.setCacheDisabled(true);

  async function doPost(data) {
    await new Promise(
        resolve => NetworkTestRunner.makeSimpleXHRWithPayload(
            'POST', '/devtools/network/resources/echo-payload.php', false, data,
            resolve));
  }

  async function doTypedArrayPost(data) {
    await new Promise(
        resolve => NetworkTestRunner.makeXHRWithTypedArrayPayload(
          'POST', '/devtools/network/resources/echo-payload.php', false, data,
          resolve));
  }

  await doPost('1 byte utf-8 char: a');
  await doPost('2 byte utf-8 char: ž');
  await doPost('3 byte utf-8 char: ツ');
  await doPost('4 byte utf-8 char: 🔥');
  await doTypedArrayPost(new Uint8Array([67, 114]));

  const harString = await new Promise(async resolve => {
    const stream = new TestRunner.StringOutputStream(resolve);
    const progress = new Common.Progress();
    await Network.HARWriter.write(
        stream, NetworkTestRunner.networkRequests(), progress);
    progress.done();
    stream.close();
  });

  const har = JSON.parse(harString);
  const entries = har.log.entries
                      .filter(
                          entry => entry.request.url.endsWith(
                              'devtools/network/resources/echo-payload.php'))
                      .map(entry => {
                        return {
                          url: entry.request.url,
                              bodySize: entry.request.bodySize,
                              postData: entry.request.postData
                        }
                      });

  for (const entry of entries) {
    TestRunner.addResult('\nrequest: ' + JSON.stringify(entry, null, 2));
  }

  TestRunner.completeTest();
})();
