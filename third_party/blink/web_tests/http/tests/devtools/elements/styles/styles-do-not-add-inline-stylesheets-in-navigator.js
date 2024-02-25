// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SourcesTestRunner} from 'sources_test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

import * as Sources from 'devtools/panels/sources/sources.js';
import * as UI from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Verify that inline stylesheets do not appear in navigator.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      </style>
    `);
  await TestRunner.evaluateInPageAnonymously(`
      function injectStyleSheet()
      {
          var style = document.createElement('style');
          style.textContent = '* {color: blue; }';
          document.head.appendChild(style);
      }
  `);

  Promise.all([UI.InspectorView.InspectorView.instance().showPanel('sources'), TestRunner.evaluateInPageAnonymously('injectStyleSheet()')])
      .then(onInjected);

  function onInjected() {
    var sourcesNavigator = new Sources.SourcesNavigator.NetworkNavigatorView();
    SourcesTestRunner.dumpNavigatorView(sourcesNavigator);
    TestRunner.completeTest();
  }
})();
