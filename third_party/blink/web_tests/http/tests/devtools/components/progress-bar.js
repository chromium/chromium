// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {TestRunner} from 'test_runner';

import * as Common from 'devtools/core/common/common.js';

(async function() {
  TestRunner.addResult(`Tests inspector's composite progress bar.\n`);


  var MockProgressIndicator = function() {};

  MockProgressIndicator.prototype = {
    // Implementation of Common.Progress interface.
    isCanceled: function() {
      return this._isCanceled;
    },

    done: function() {
      TestRunner.addResult('progress indicator: done');
    },

    setTotalWork: function(totalWork) {
      this._totalWork = totalWork;
    },

    setWorked: function(worked, title) {
      this._worked = worked;
      if (typeof title !== 'undefined')
        this._title = title;
    },

    setTitle: function(title) {
      this._title = title;
    },

    // Test methods.
    cancel: function() {
      this._isCanceled = true;
    },

    dump: function() {
      const roundFactor = 10000;

      var worked = this._worked;
      var totalWork = this._totalWork;

      if (typeof worked === 'number')
        worked = Math.round(worked * roundFactor) / roundFactor;
      if (typeof totalWork === 'number')
        totalWork = Math.round(totalWork * roundFactor) / roundFactor;

      TestRunner.addResult('progress: `' + this._title + '\' ' + worked + ' out of ' + totalWork + ' done.');
    }
  };

  TestRunner.runTestSuite([
    function testOneSubProgress(next) {
      var indicator = new MockProgressIndicator();
      var composite = new Common.Progress.CompositeProgress(indicator);
      var subProgress = composite.createSubProgress();

      TestRunner.addResult('Testing CompositeProgress with a single subprogress:');
      indicator.dump();
      subProgress.setTitle('cuckooing');
      subProgress.setWorked(10);
      indicator.dump();
      subProgress.setTotalWork(100);
      indicator.dump();
      subProgress.setWorked(20, 'meowing');
      indicator.dump();
      subProgress.done();
      indicator.dump();
      next();
    },

    function testMultipleSubProgresses(next) {
      var indicator = new MockProgressIndicator();
      var composite = new Common.Progress.CompositeProgress(indicator);
      var subProgress1 = composite.createSubProgress();
      var subProgress2 = composite.createSubProgress(3);

      TestRunner.addResult('Testing CompositeProgress with multiple subprogresses:');
      indicator.dump();

      subProgress1.setTitle('cuckooing');
      subProgress1.setTotalWork(100);
      subProgress1.setWorked(20);
      indicator.dump();

      subProgress2.setWorked(10);
      indicator.dump();

      subProgress2.setTotalWork(10);
      subProgress2.setWorked(3, 'barking');
      indicator.dump();

      subProgress1.setWorked(50, 'meowing');
      subProgress2.setWorked(5);
      indicator.dump();

      subProgress2.done();
      indicator.dump();

      subProgress1.done();
      indicator.dump();
      next();
    },

    function testCancel(next) {
      var indicator = new MockProgressIndicator();
      var composite = new Common.Progress.CompositeProgress(indicator);
      var subProgress = composite.createSubProgress();

      TestRunner.addResult('Testing isCanceled:');
      TestRunner.assertTrue(!subProgress.isCanceled(), 'progress should not be canceled');
      indicator.cancel();
      TestRunner.assertTrue(subProgress.isCanceled(), 'progress should be canceled');
      next();
    },

    function testNested(next) {
      var indicator = new MockProgressIndicator();
      var composite0 = new Common.Progress.CompositeProgress(indicator);
      var subProgress01 = composite0.createSubProgress();
      var composite1 = new Common.Progress.CompositeProgress(subProgress01);
      var subProgress11 = composite1.createSubProgress(10);  // Weight should have no effect.

      TestRunner.addResult('Testing nested subprogresses:');
      indicator.dump();

      subProgress11.setWorked(10);
      indicator.dump();

      subProgress11.setTotalWork(100);
      indicator.dump();

      subProgress11.setWorked(50, 'meowing');
      indicator.dump();

      TestRunner.assertTrue(!subProgress11.isCanceled());
      indicator.cancel();
      TestRunner.assertTrue(subProgress11.isCanceled());

      subProgress11.done();
      indicator.dump();
      next();
    }
  ]);
})();
