// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  var {page, session, dp} = await testRunner.startBlank(`Test to ensure devtools gets post data for redirected navigation requests.\n`);

  dp.Network.enable();
  dp.Page.enable();

  dp.Runtime.evaluate({expression: `
    document.body.innerHTML = '<form id="form" method="post" action="/loading/resources/redirect-methods-result.php?status=302"><input type="text" name="foo" value="bar" /></form>';
    var form = document.getElementById('form');
    form.submit();
  `});

  dp.Network.onRequestWillBeSent(event => {
    testRunner.log("Got request: " + event.params.request.url);
    if (event.params.request.postData)
      testRunner.log("Post Data: " + event.params.request.postData);
  });
  dp.Page.onLoadEventFired(() => testRunner.completeTest());
});
