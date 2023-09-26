// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';
import {ElementsTestRunner} from 'elements_test_runner';

(async function() {
  TestRunner.addResult(`Tests representation of selector lists in the protocol. Bug 103118.\n`);
  await TestRunner.showPanel('elements');
  await TestRunner.loadHTML(`
      <style>
      #inspected{
      }

      #InSpEcTeD {
      }

      /* */#inspected/* */ {
      }

      /*
       */ FOO/*Single-line 1*/ bAr,/* Single-line 2*/#inspected/*
          Multiline comment
      */ ,MOO>BAR, /*1*/htML /*2
      */div/*3*/,/**/Foo~/**C*/Moo,/**/MOO /* Comment
       */
      {
        color: green;
      }

      </style>

      <div id="inspected">Text</div>
    `);

  ElementsTestRunner.selectNodeAndWaitForStyles('inspected', step1);

  async function step1() {
    await ElementsTestRunner.dumpSelectedElementStyles(true);
    TestRunner.completeTest();
  }
})();
