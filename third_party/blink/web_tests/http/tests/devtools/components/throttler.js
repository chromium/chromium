// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function() {
  TestRunner.addResult(`This test verifies throttler behavior.\n`);

  class TimeoutMock {
    constructor() {
      this._timeoutId = 0;
      this._timeoutIdToProcess = new Map();
      this._timeoutIdToMillis = new Map();
      this._time = 1;
      this.setTimeout = this.setTimeout.bind(this);
      this.clearTimeout = this.clearTimeout.bind(this);
      this.getTime = this.getTime.bind(this);
    }

    /**
     * @param {!Function} operation
     * @param {number} timeout
     */
    setTimeout(operation, timeout) {
      this._timeoutIdToProcess.set(++this._timeoutId, operation);
      this._timeoutIdToMillis.set(this._timeoutId, timeout);
      return this._timeoutId;
    }

    /**
     *
     * @param {number} timeoutId
     */
    clearTimeout(timeoutId) {
      this._timeoutIdToProcess.delete(timeoutId);
      this._timeoutIdToMillis.delete(timeoutId);
    }

    /**
     * @return {!Array<number>}
     */
    activeTimersTimeouts() {
      return Array.from(this._timeoutIdToMillis.values());
    }

    getTime() {
      return this._time;
    }

    fireAllTimers() {
      this._time = Math.max(...this.activeTimersTimeouts()) + 1;
      for (const timeoutId of this._timeoutIdToProcess.keys())
        this._timeoutIdToProcess.get(timeoutId).call(window);
      this._timeoutIdToProcess.clear();
      this._timeoutIdToMillis.clear();
    }
  }

  class ProcessMock {
    constructor(name, runnable) {
      this._runnable = runnable;
      this.processName = name;
      this.run = this.run.bind(this);
      this.run.processName = name;

      this.startPromise = new Promise(fulfill => this._startCallback = fulfill);
      this.finishPromise = new Promise(fulfill => this._finishCallback = fulfill);
    }

    run() {
      TestRunner.addResult('Process \'' + this.processName + '\' STARTED.');
      this._startCallback();
      if (this._runnable)
        this._runnable.call(null);
      return this.finishPromise;
    }

    finish() {
      this.startPromise.then(onFinish.bind(this));

      function onFinish() {
        TestRunner.addResult('Process \'' + this.processName + '\' FINISHED.');
        this._finishCallback();
      }
    }

    static create(name, runnable) {
      return new ProcessMock(name, runnable);
    }
  }

  var throttler = new Common.Throttler(1989);
  var timeoutMock = new TimeoutMock();
  throttler._setTimeout = timeoutMock.setTimeout;
  throttler._clearTimeout = timeoutMock.clearTimeout;
  throttler._getTime = timeoutMock.getTime;
  TestRunner.addSniffer(throttler, 'schedule', logSchedule, true);

  function testSimpleSchedule(next, runningProcess) {
    assertThrottlerIdle();
    throttler.schedule(ProcessMock.create('operation #1').run, false);
    var process = ProcessMock.create('operation #2');
    throttler.schedule(process.run);

    var promise = Promise.resolve();
    if (runningProcess) {
      runningProcess.finish();
      promise = waitForProcessFinish();
    }

    promise.then(() => {
      assertThrottlerTimeout();
      timeoutMock.fireAllTimers();
      process.finish();
      return waitForProcessFinish();
    }).then(next);
  }

  function testAsSoonAsPossibleOverrideTimeout(next, runningProcess) {
    assertThrottlerIdle();
    throttler.schedule(ProcessMock.create('operation #1').run);
    var process = ProcessMock.create('operation #2');
    throttler.schedule(process.run, true);

    var promise = Promise.resolve();
    if (runningProcess) {
      runningProcess.finish();
      promise = waitForProcessFinish();
    }

    promise
        .then(function() {
          assertThrottlerTimeout();
          timeoutMock.fireAllTimers();
          process.finish();
          return waitForProcessFinish();
        })
        .then(next);
  }

  function testAlwaysExecuteLastScheduled(next, runningProcess) {
    assertThrottlerIdle();
    var process = null;
    for (var i = 0; i < 4; ++i) {
      process = ProcessMock.create('operation #' + i);
      throttler.schedule(process.run, i % 2 === 0);
    }
    var promise = Promise.resolve();
    if (runningProcess) {
      runningProcess.finish();
      promise = waitForProcessFinish();
    }
    promise
        .then(function() {
          assertThrottlerTimeout();
          timeoutMock.fireAllTimers();
          process.finish();
          return waitForProcessFinish();
        })
        .then(next);
  }

  TestRunner.runTestSuite([
    testSimpleSchedule,

    testAsSoonAsPossibleOverrideTimeout,

    testAlwaysExecuteLastScheduled,

    function testSimpleScheduleDuringProcess(next) {
      var runningProcess = throttlerToRunningState();
      runningProcess.startPromise.then(function() {
        testSimpleSchedule(next, runningProcess);
      });
    },

    function testAsSoonAsPossibleOverrideDuringProcess(next) {
      var runningProcess = throttlerToRunningState();
      runningProcess.startPromise.then(function() {
        testAsSoonAsPossibleOverrideTimeout(next, runningProcess);
      });
    },

    function testAlwaysExecuteLastScheduledDuringProcess(next) {
      var runningProcess = throttlerToRunningState();
      runningProcess.startPromise.then(function() {
        testAlwaysExecuteLastScheduled(next, runningProcess);
      });
    },

    function testScheduleFromProcess(next) {
      var nextProcess;
      assertThrottlerIdle();
      var process = ProcessMock.create('operation #1', processBody);
      throttler.schedule(process.run);
      assertThrottlerTimeout();
      timeoutMock.fireAllTimers();
      process.finish();
      waitForProcessFinish()
          .then(function() {
            assertThrottlerTimeout();
            timeoutMock.fireAllTimers();
            nextProcess.finish();
            return waitForProcessFinish();
          })
          .then(next);

      function processBody() {
        nextProcess = ProcessMock.create('operation #2');
        throttler.schedule(nextProcess.run, false);
      }
    },

    function testExceptionFromProcess(next) {
      var process = ProcessMock.create('operation #1', processBody);
      throttler.schedule(process.run);
      timeoutMock.fireAllTimers();
      waitForProcessFinish().then(function() {
        assertThrottlerIdle();
        next();
      });

      function processBody() {
        throw new Error('Exception during process execution.');
      }
    },

    async function testPromise(next) {
      var process = ProcessMock.create('operation #1', () => 1);
      var schedulePromse = throttler.schedule(process.run).then(() => TestRunner.addResult('The promise resolved.'));
      await Promise.resolve();
      timeoutMock.fireAllTimers();
      await Promise.resolve();
      process.finish();

      await schedulePromse;
      next();

    }
  ]);

  function waitForProcessFinish() {
    var promiseResolve;
    var hasFinished;
    TestRunner.addSniffer(Common.Throttler.prototype, '_processCompletedForTests', onFinished);
    function onFinished() {
      hasFinished = true;
      if (promiseResolve)
        promiseResolve();
    }
    return new Promise(function(success) {
      promiseResolve = success;
      if (hasFinished)
        success();
    });
  }

  function throttlerToRunningState() {
    assertThrottlerIdle();
    var process = ProcessMock.create('long operation');
    throttler.schedule(process.run);
    assertThrottlerTimeout();
    timeoutMock.fireAllTimers();
    return process;
  }

  function assertThrottlerIdle() {
    var timeouts = timeoutMock.activeTimersTimeouts();
    if (timeouts.length !== 0) {
      TestRunner.addResult(
          'ERROR: throttler is not in idle state. Scheduled timers timeouts: [' + timeouts.sort().join(', ') + ']');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult('Throttler is in IDLE state (doesn\'t have any timers set up)');
  }

  function assertThrottlerTimeout() {
    var timeouts = timeoutMock.activeTimersTimeouts();
    if (timeouts.length === 0) {
      TestRunner.addResult('ERROR: throttler is not in timeout state. Scheduled timers timeouts are empty!');
      TestRunner.completeTest();
      return;
    }
    TestRunner.addResult(
        'Throttler is in TIMEOUT state. Scheduled timers timeouts: [' + timeouts.sort().join(', ') + ']');
  }

  function logSchedule(operation, asSoonAsPossible, returnValue) {
    if (returnValue === undefined) {
      returnValue = asSoonAsPossible;
      asSoonAsPossible = undefined;
    }
    TestRunner.addResult('SCHEDULED: \'' + operation.processName + '\' asSoonAsPossible: ' + asSoonAsPossible);
  }
})();
