// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {NetworkTestRunner} from 'network_test_runner';

(async function() {
  TestRunner.addResult(`Tests how request headers are decoded in network item view.\n`);
  await TestRunner.showPanel('network');


  var value = 'Test+%21%40%23%24%25%5E%26*%28%29_%2B+parameters.';
  var parameterElement = Network.RequestPayloadView.prototype.formatParameter(value, '', true);
  TestRunner.addResult('Original value: ' + value);
  TestRunner.addResult('Decoded value: ' + parameterElement.textContent);
  TestRunner.completeTest();
})();
