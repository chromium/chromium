// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      `Verifies that Page.getResourceTree reports an iframe's main resource only once.`);

  await session.evaluateAsync(url => new Promise(resolve => {
                                const iframe = document.createElement('iframe');
                                iframe.src = url;
                                iframe.onload = resolve;
                                document.body.appendChild(iframe);
                              }),
                              testRunner.url('../resources/empty.html'));

  const {frameTree} = (await dp.Page.getResourceTree()).result;
  const urls = [];
  (function collect(tree) {
    urls.push(tree.frame.url);
    for (const resource of tree.resources) {
      urls.push(resource.url);
    }
    for (const child of tree.childFrames || []) {
      collect(child);
    }
  })(frameTree);

  testRunner.log('Reported resources:');
  testRunner.log(urls.sort().map(url => '  ' + url).join('\n'));
  testRunner.completeTest();
});
