// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests that elements panel search is not crashing on documentElement-less cases.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <iframe src="resources/dom-search-crash-iframe.html" onload="runTest()"></iframe>
    `);

  TestRunner.runTestSuite([
    function testSetUp(next) {
      TestRunner.domModel.requestDocument().then(next);
    },

    function testNoCrash(next) {
      TestRunner.domModel.performSearch('FooBar', false).then(next);
    }
  ]);
})();
