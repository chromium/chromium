// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {SecurityTestRunner} from 'security_test_runner';

(async function() {
  TestRunner.addResult(`Tests that origin group names in the Security panel are distinct.\n`);
  await TestRunner.showPanel('security');

  var originGroupNameSize = Object.keys(Security.SecurityPanelSidebarTree.OriginGroup).length;

  var deduplicatedNames = new Set();
  for (var key in Security.SecurityPanelSidebarTree.OriginGroup) {
    var name = Security.SecurityPanelSidebarTree.OriginGroup[key];
    deduplicatedNames.add(name);
  }

  TestRunner.addResult(
      'Number of names (' + originGroupNameSize + ') == number of unique names (' + deduplicatedNames.size +
      '): ' + (originGroupNameSize == deduplicatedNames.size) + ' (expected: true)');

  TestRunner.completeTest();
})();
