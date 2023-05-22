// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Test that ProfilerAgent start/stop doesn't crash.\n`);

  TestRunner.ProfilerAgent.start().then(onStart);

  function onStart() {
    TestRunner.addResult('ProfilerAgent started.');
    TestRunner.ProfilerAgent.stop().then(onStop);
  }

  function onStop() {
    TestRunner.addResult('ProfilerAgent stopped.');
    TestRunner.completeTest();
  }
})();
