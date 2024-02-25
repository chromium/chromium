// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

import * as Security from 'devtools/panels/security/security.js';

(async function() {
  TestRunner.addResult(`Tests that origin group names in the Security panel are distinct.\n`);
  await TestRunner.showPanel('security');

  var originGroupNameSize = Object.keys(Security.SecurityPanel.OriginGroup).length;

  var deduplicatedNames = new Set();
  for (var key in Security.SecurityPanel.OriginGroup) {
    var name = Security.SecurityPanel.OriginGroup[key];
    deduplicatedNames.add(name);
  }

  TestRunner.addResult(
      'Number of names (' + originGroupNameSize + ') == number of unique names (' + deduplicatedNames.size +
      '): ' + (originGroupNameSize == deduplicatedNames.size) + ' (expected: true)');

  TestRunner.completeTest();
})();
