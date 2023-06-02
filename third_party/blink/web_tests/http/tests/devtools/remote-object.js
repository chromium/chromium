// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`Tests formatting of different types of remote objects.\n`);

  var result = await TestRunner.evaluateInPageRemoteObject('new Date(2011, 11, 7, 12, 01)');
  TestRunner.addResult('date = ' + result.description.substring(0, 25));
  TestRunner.completeTest();
})();
