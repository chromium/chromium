// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Tests that resources panel shows form data parameters.\n`);
  await TestRunner.loadHTML(`
      <form target="target-iframe" method="POST" action="http://127.0.0.1:8000/devtools/resources/post-target.cgi?queryParam1=queryValue1&amp;queryParam2=#fragmentParam1=fragmentValue1&amp;fragmentParam2=">
      <input name="formParam1" value="formValue1">
      <input name="formParam2">
      <input id="submit" type="submit">
      </form>
      <iframe name="target-iframe"></iframe>
    `);
  await TestRunner.evaluateInPagePromise(`
      function submit()
      {
          document.getElementById("submit").click();
      }
  `);

  TestRunner.evaluateInPage('submit()');
  TestRunner.networkManager.addEventListener(SDK.NetworkManager.Events.RequestFinished, onRequestFinished);

  async function onRequestFinished(event) {
    var request = event.data;
    TestRunner.addResult(request.url());
    TestRunner.addObject(
        await NetworkTestRunner.buildHARLogEntry(request, {sanitize: false}),
        NetworkTestRunner.HARPropertyFormattersWithSize);
    TestRunner.completeTest();
  }
})();
