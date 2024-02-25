// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ConsoleTestRunner} from 'console_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(
      `Tests that inspect element action works for iframe children (https://bugs.webkit.org/show_bug.cgi?id=76808).\n`);
  await TestRunner.showPanel('elements');

  await TestRunner.addIframe('resources/inspect-element-iframe.html');

  ElementsTestRunner.firstElementsTreeOutline().addEventListener(
      ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, selectedNodeChanged, this);
  function selectedNodeChanged(event) {
    var node = event.data.node;
    if (!node)
      return;
    if (node.getAttribute('id') == 'div') {
      TestRunner.addResult(ElementsModule.DOMPath.fullQualifiedSelector(node));
      TestRunner.completeTest();
    }
  }
  ConsoleTestRunner.evaluateInConsole('inspect(iframeDivElement)');
})();
