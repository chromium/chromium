// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(testRunner) {
  const html = `<!doctype html>
    <html><body>Hello, world!</body></html>
    `;

  const {page, dp} =
      await testRunner.startHTML(html, 'Tests window size upon start.');

  const {result: {result: {value}}} = await dp.Runtime.evaluate({
    expression: `'Window size: ' + window.outerWidth + 'x' + window.outerHeight`
  });

  testRunner.log(value);

  testRunner.completeTest();
})
