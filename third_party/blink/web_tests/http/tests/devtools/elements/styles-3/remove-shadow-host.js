// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as SDK from 'devtools/core/sdk/sdk.js';

(async function() {
  TestRunner.addResult(`Test remove shadow host.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
        <!DOCTYPE html>
        <div id="host"></div>
      `);
  await TestRunner.evaluateInPagePromise(`
        function addShadow() {
          var root = host.attachShadow({mode:"open"});
          root.innerHTML = '<link rel="stylesheet" href="data:text/css,#x{color:pink}">';
        }
    `);

  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetAdded, sheetAdded);
  TestRunner.cssModel.addEventListener(SDK.CSSModel.Events.StyleSheetRemoved, sheetRemoved);
  TestRunner.evaluateInPage('addShadow()');

  function sheetAdded(event) {
    TestRunner.addResult('Sheet added: ' + event.data.sourceURL);
    TestRunner.evaluateInPage('host.remove()');
  }

  function sheetRemoved(event) {
    TestRunner.addResult('Sheet removed: ' + event.data.sourceURL);
    TestRunner.completeTest();
  }
})();
