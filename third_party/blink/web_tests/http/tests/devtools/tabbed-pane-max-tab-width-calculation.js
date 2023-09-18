// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as UIModule from 'devtools/ui/legacy/legacy.js';

(async function() {
  TestRunner.addResult(`Tests tabbed pane max tab element width calculation.\n`);

  function calculateAndDumpMaxWidth(measuredWidths, totalWidth) {
    var maxWidth = UIModule.TabbedPane.TabbedPane.prototype.calculateMaxWidth(measuredWidths, totalWidth);
    TestRunner.addResult(
        'measuredWidths = [' + String(measuredWidths) + '], totalWidth = ' + totalWidth + ', maxWidth = ' + maxWidth +
        '.');
  }

  calculateAndDumpMaxWidth([50, 70, 20], 150);
  calculateAndDumpMaxWidth([50, 80, 20], 150);
  calculateAndDumpMaxWidth([50, 90, 20], 150);
  calculateAndDumpMaxWidth([90, 90, 20], 150);
  calculateAndDumpMaxWidth([90, 80, 70], 150);

  TestRunner.completeTest();
})();
