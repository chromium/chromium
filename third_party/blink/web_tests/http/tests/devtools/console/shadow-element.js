// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as Elements from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(`Tests that $0 works with shadow dom.\n`);
  await TestRunner.showPanel('console');
  await TestRunner.loadHTML(`
      <div><div><div id="host"></div></div></div>
  `);
  await TestRunner.evaluateInPagePromise(`
        var host = document.querySelector('#host');
        var sr = host.attachShadow({mode: 'open'});
        sr.innerHTML = "<div><div><div id='shadow'><input id='user-agent-host' type='range'></div></div></div>";
    `);

  Common.Settings.settingForTest('show-ua-shadow-dom').set(true);
  ElementsTestRunner.selectNodeWithId('shadow', step1);

  function step1() {
    ConsoleTestRunner.evaluateInConsoleAndDump('\'Author shadow element: \' + $0.id', step3);
  }

  function step3() {
    ElementsTestRunner.selectNodeWithId('user-agent-host', step4);
  }

  function step4(node) {
    Elements.ElementsPanel.ElementsPanel.instance().revealAndSelectNode(node.shadowRoots()[0]);
    ConsoleTestRunner.evaluateInConsoleAndDump('\'User agent shadow host: \' + $0.id', step5);
  }

  function step5() {
    TestRunner.completeTest();
  }
})();
