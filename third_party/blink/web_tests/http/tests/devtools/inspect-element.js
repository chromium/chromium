// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ConsoleTestRunner} from 'console_test_runner';
import * as SDK from 'devtools/core/sdk/sdk.js';
import * as ElementsModule from 'devtools/panels/elements/elements.js';
import * as UIModule from 'devtools/ui/legacy/legacy.js';
import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(
      `Tests that inspect element action works for iframe children (https://bugs.webkit.org/show_bug.cgi?id=76808).\n`);
  await TestRunner.showPanel('elements');

  await TestRunner.addIframe('resources/inspect-element-iframe.html');

  // The Elements panel publishes the selected node as the DOMNode flavor.
  UIModule.Context.Context.instance().addFlavorChangeListener(
      SDK.DOMModel.DOMNode, selectedNodeChanged, this);
  function selectedNodeChanged(event) {
    var node = event.data;
    if (!node)
      return;
    if (node.getAttribute('id') == 'div') {
      TestRunner.addResult(ElementsModule.DOMPath.fullQualifiedSelector(node));
      TestRunner.completeTest();
    }
  }
  ConsoleTestRunner.evaluateInConsole('inspect(iframeDivElement)');
})();
