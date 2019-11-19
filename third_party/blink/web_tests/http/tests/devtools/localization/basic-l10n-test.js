// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`Tests that the the localization functions work as expected.\n`);

  // Verify localize returns an empty string.
  TestRunner.addResult(Common.localize(''));
  TestRunner.addResult(Common.localize('Test string is returned as is'));
  TestRunner.addResult(Common.localize('Test string with a placeholder %s is returned as is'));
  TestRunner.addResult(Common.localize('Test %s string with multiple placeholders %s is returned as is'));
  TestRunner.addResult(Common.localize('Test unicode character \xa0%'));

  // Verify UIString returns an empty string.
  TestRunner.addResult(Common.UIString(''));
  TestRunner.addResult(Common.UIString('Test string is returned as is'));
  TestRunner.addResult(Common.UIString('Test string with a %s is returned with a substitution', 'placeholder'));
  TestRunner.addResult(Common.UIString('%s string with multiple %s is returned with substitutions', 'Test', 'placeholders'));
  TestRunner.addResult(Common.UIString('Test numeric placeholder: %d', -99));
  TestRunner.addResult(Common.UIString('Test numeric formatted placeholder and unicode character: %.2f\xa0%%', 88.149));

  // Verify ls returns an empty string.
  TestRunner.addResult(ls``);
  TestRunner.addResult(ls`Test string is returned as is`);
  TestRunner.addResult(ls`Test string with a ${'placeholder'} is returned with a substitution.`);
  TestRunner.addResult(ls`${'Test'} string with ${'placeholders'} is returned with substitutions.`);
  TestRunner.addResult(ls`Test numeric placeholder: ${-99}`);
  TestRunner.addResult(ls`Test numeric placeholder and unicode character ${88.149}\xa0%`);
  TestRunner.addResult(ls('Test calling ls as a function'));

  TestRunner.completeTest();
})();