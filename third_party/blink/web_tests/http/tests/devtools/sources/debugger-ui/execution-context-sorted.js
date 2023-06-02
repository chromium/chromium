// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests how execution context and target are selected.\n`);
  await TestRunner.showPanel('sources');
  await TestRunner.addIframe('../resources/execution-context-iframe1.html');

  var contexts = TestRunner.runtimeModel.executionContexts();
  for (var c of contexts)
    TestRunner.addResult(TestRunner.resourceTreeModel.frameForId(c.frameId).displayName());
  TestRunner.completeTest();
})();
