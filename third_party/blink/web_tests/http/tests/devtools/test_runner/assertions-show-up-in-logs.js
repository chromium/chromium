// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
