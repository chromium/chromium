// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`This test passes if it doesn't crash. crbug.com/789263\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <!DOCTYPE html>
      <style>
      ** { }
      @supports (display: flex) { }
      </style>
    `);

  TestRunner.completeTest();
})();
