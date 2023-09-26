// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests that XSL-transformed documents in the main frame are rendered correctly in the Elements panel.
      https://bugs.webkit.org/show_bug.cgi?id=111313`);

  await TestRunner.showPanel('elements');

  await TestRunner.navigatePromise('../styles/resources/xsl-transformed.xml');

  ElementsTestRunner.expandElementsTree(step2);

  function step2() {
    ElementsTestRunner.dumpElementsTree();
    TestRunner.completeTest();
  }
})();