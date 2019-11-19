// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(
      `Tests resource content is correctly loaded if Resource.requestContent was called before network request was finished. https://bugs.webkit.org/show_bug.cgi?id=90153\n`);
  await TestRunner.loadModule('application_test_runner');
  await TestRunner.showPanel('resources');
  await TestRunner.addStylesheetTag('resources/styles-initial.css');
  TestRunner.addResult('Adding dynamic script: ');
  await TestRunner.evaluateInPageAsync(`
    (function createScriptTag() {
      var scriptElement = document.createElement("script");
      scriptElement.src = "${TestRunner.url('resources/dynamic-script.js')}";
      document.head.appendChild(scriptElement);
    })();
  `);

  TestRunner.addResult('Waiting for script request to finish: ');
  await TestRunner.waitForUISourceCode('dynamic-script.js');

  TestRunner.addResult('Clearing memory cache: ');
  await TestRunner.NetworkAgent.setCacheDisabled(true);
  await TestRunner.NetworkAgent.setCacheDisabled(false);
  TestRunner.addResult('Requesting content: ');
  var resource = ApplicationTestRunner.resourceMatchingURL('dynamic-script.js');
  var { content } = await resource.requestContent();
  TestRunner.assertTrue(!!content, 'No content available.');
  TestRunner.addResult('Resource url: ' + resource.url);
  TestRunner.addResult('Resource content: ' + content);
  TestRunner.completeTest();
})();
