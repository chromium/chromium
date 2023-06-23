// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult('Tests the test runner shows assertions in the logs\n');

  TestRunner.addResult('Passing assert should not show up');
  console.assert(true, 'This is a passing assertion.');
  TestRunner.addResult('');

  TestRunner.addResult('Failing assert should show up');
  console.assert(false, 'This is a failing assertion.');
  TestRunner.addResult('');

  TestRunner.addResult('Failing assert with multiple arguments');
  console.assert(false, 'This is ', 'a ', 'failing assertion.');

  TestRunner.completeTest();
})();
