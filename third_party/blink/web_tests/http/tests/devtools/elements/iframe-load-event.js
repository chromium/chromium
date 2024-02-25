// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that iframe content is available after iframe's load event fired. See http://webkit.org/b/76552\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.addIframe('resources/iframe-load-event-iframe-1.html', {id: 'myframe'});

  ElementsTestRunner.expandElementsTree(step1);

  async function step1()
  {
      await TestRunner.evaluateInPageAsync(`
        (function(){
          document.getElementById("myframe").src = "resources/iframe-load-event-iframe-2.html";
          return new Promise((resolve) => document.getElementById("myframe").onload = resolve);
        })();
      `);
      ElementsTestRunner.expandElementsTree(step2);
  }

  function step2()
  {
      TestRunner.addResult("\n\nAfter frame navigate");
      ElementsTestRunner.dumpElementsTree();
      TestRunner.completeTest();
  }
})();
