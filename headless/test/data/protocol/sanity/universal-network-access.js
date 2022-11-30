// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const resourcePath = 'protocol/sanity/resources/body.html';
  const port = window.location.port;
  const privilegedPageURL = `http://site1.test:${port}/${resourcePath}`;
  const nonprivilegedPageURL = `http://site2.test:${port}/${resourcePath}`;
  const fetchURL = `http://site3.test:${port}/${resourcePath}`;
  const options = {
    createContextOptions: {
      originsWithUniversalNetworkAccess: [new URL(privilegedPageURL).origin]
    }
  }
  const {page, session, dp} = await testRunner.startURL(
      privilegedPageURL,
      'Tests handling of originsWithUniversalNetworkAccess',
      options);

  const fetchInPrivilegedPage = await session.evaluateAsync(`
    fetch('${fetchURL}')
        .then(response => response.text())
        .then(text => 'PASS: ' + text.replace(/\\s+/mg, ' '))
        .catch(exception => 'FAIL: ' + exception.toString())
  `);
  testRunner.log(fetchInPrivilegedPage);
  await session.navigate(nonprivilegedPageURL);
  const fetchInNonPrivilegedPage = await session.evaluateAsync(`
    fetch('${fetchURL}')
        .then('FAIL: request succeeded')
        .catch(exception => 'PASS: request failed')
  `);
  testRunner.log(fetchInNonPrivilegedPage);
  testRunner.completeTest();
})
