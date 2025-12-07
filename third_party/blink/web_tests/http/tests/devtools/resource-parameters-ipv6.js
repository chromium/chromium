// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that resources panel shows form data parameters.\n`);
  await TestRunner.navigatePromise('http://[::1]:8000/devtools/resources/inspected-page.html');
  await TestRunner.evaluateInPagePromise(`
      document.write(\`<form target="target-iframe" method="POST" action="http://[::1]:8000/devtools/resources/post-target.cgi?queryParam1=queryValue1&amp;queryParam2=#fragmentParam1=fragmentValue1&amp;fragmentParam2=">
      <input name="formParam1" value="formValue1">
      <input name="formParam2">
      <input id="submit" type="submit">
      </form>
      <iframe name="target-iframe"></iframe>
      <script>
      function submit()
      {
          document.getElementById("submit").click();
      }
      </script>
    \`)`);

  TestRunner.evaluateInPage('submit()');
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestFinished, onRequestFinished);

  async function onRequestFinished(event) {
    var request = event.data;
    if (!/post-target\.cgi/.test(request.url()))
      return;
    TestRunner.addResult(request.url());
    TestRunner.addObject(
        await NetworkTestRunner.buildHARLogEntry(request, {sanitize: false}),
        NetworkTestRunner.HARPropertyFormattersWithSize);
    TestRunner.completeTest();
  }
})();
