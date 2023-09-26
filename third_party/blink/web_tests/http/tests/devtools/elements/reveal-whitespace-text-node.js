// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Common from 'devtools/core/common/common.js';
import * as ElementsModule from 'devtools/panels/elements/elements.js';

(async function() {
  TestRunner.addResult(
      `This test verifies that revealing a whitespace text node RemoteObject reveals its parentElement DIV.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <p id="description"></p>

      <div id="test">
      </div>
    `);
  await TestRunner.evaluateInPagePromise(`
      function whitespaceChild()
      {
          return document.getElementById("test").firstChild;
      }
  `);

  var childObject = await TestRunner.evaluateInPageRemoteObject('whitespaceChild()');

  ElementsTestRunner.firstElementsTreeOutline().addEventListener(
      ElementsModule.ElementsTreeOutline.ElementsTreeOutline.Events.SelectedNodeChanged, selectedNodeChanged);
  Common.Revealer.reveal(childObject);

  function selectedNodeChanged(event) {
    var node = event.data.node;
    TestRunner.addResult('SelectedNodeChanged: ' + node.localName());
    TestRunner.completeTest();
  }
})();
