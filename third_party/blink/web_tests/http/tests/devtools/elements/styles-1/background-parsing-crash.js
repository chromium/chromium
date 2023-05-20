// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

(async function() {
  TestRunner.addResult(`This test passes if it doesn't ASSERT.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      .absent {
          background: #fff url(foo.png) no-repeat left 4px;
      }

      body {
          background: #fff;
      }
      </style>
    `);

  TestRunner.completeTest();
})();
