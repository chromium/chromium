// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Platform from 'devtools/core/platform/platform.js';

(async function() {
  TestRunner.addResult(`Tests that formatting processes '%' properly in case of missing formatters.\n`);
  await TestRunner.showPanel('console');

  var params = [
    ['%T', 1],
    ['10% x 20%', 'of the original'],
    ['%%', ''],
    ['%%%', ''],
    ['%%', 1, 2, 3],
    ['%%d', 1],
    ['%%d%', 1],
    ['%%%d%', 1],
    ['%%%d%%', 1],
    ['%', ''],
    ['% %d', 1],
    ['%d % %s', 1, 'foo'],
    ['%.2f', 0.12345],
    ['foo%555 bar', ''],
  ];
  for (var i = 0; i < params.length; ++i)
    TestRunner.addResult(
        'String.sprintf(' + params[i].join(', ') + ') = "' + Platform.StringUtilities.sprintf.apply(String, params[i]) + '"');
  TestRunner.completeTest();
})();
