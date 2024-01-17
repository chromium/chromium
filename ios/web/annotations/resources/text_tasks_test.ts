// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tests for text_tasks.ts.
 */

import {IdleTaskTracker} from '//ios/web/annotations/resources/text_tasks.js';
import {expectEq, fail, FakeTaskTimer, TestSuite} from '//ios/web/annotations/resources/text_test_utils.js';

class TestTextTasks extends TestSuite {
  // Checks for proper task timing when there's no user activity recorded.
  testIdleTaskTrackerNoActivity() {
    const timer = new FakeTaskTimer();
    const tracker = new IdleTaskTracker(timer, 100, 50);
    let runCount = 0;
    tracker.schedule(() => {
      expectEq(timer.now(), 100, 'time at task 1:');
      runCount++;
    }, 100);
    tracker.schedule(() => {
      // Asking for 110, but won't run until 50ms after first task.
      expectEq(timer.now(), 150, 'time at task 2:');
      runCount++;
    }, 110);
    tracker.schedule(() => {
      expectEq(timer.now(), 300, 'time at task 3:');
      runCount++;
    }, 300);
    // Regardless of the number of tasks, only one timer should run.
    expectEq(timer.timers.size, 1, 'number of timers :');
    timer.moveAhead(/* ms= */ 10, /* times= */ 20);  // -> 200ms total
    expectEq(runCount, 2, 'timer run count :');
    timer.moveAhead(/* ms= */ 10, /* times= */ 15);  // -> 350ms total
    expectEq(runCount, 3, 'timer run count :');
    // No task, no timer.
    expectEq(timer.timers.size, 0, 'number of timers :');
  }

  // Checks for proper task timing when there is some user activity recorded.
  testIdleTaskTrackerWithActivity() {
    const timer = new FakeTaskTimer();
    const tracker = new IdleTaskTracker(timer, 100, 50);
    let runCount = 0;
    tracker.schedule(() => {
      expectEq(timer.now(), 100, 'time at task 1:');
      runCount++;
    }, 100);
    tracker.schedule(() => {
      // Asking for 110, but won't run until 50ms after first task (at 150ms).
      // But at 120ms, there is activity, so the next task is pushed back by
      // 100ms.
      expectEq(timer.now(), 220, 'time at task 2:');
      runCount++;
    }, 110);
    // Regardless of the number of tasks, only one timer should run.
    expectEq(timer.timers.size, 1, 'number of timers :');
    timer.moveAhead(/* ms= */ 10, /* times= */ 12);  // -> 120ms total
    expectEq(runCount, 1, 'timer run count :');
    const event = new Event('scroll');
    window.dispatchEvent(event);
    timer.moveAhead(/* ms= */ 10, /* times= */ 13);  // -> 250ms total
    expectEq(runCount, 2, 'timer run count :');
    // No task, no timer.
    expectEq(timer.timers.size, 0, 'number of timers :');
  }

  // Checks for proper task timing when there is some user activity recorded
  // before any task is scheduled.
  testIdleTaskTrackerWithPreActivity() {
    const timer = new FakeTaskTimer();
    const tracker = new IdleTaskTracker(timer, 100, 10);
    const event = new Event('scroll');
    let runCount = 0;

    // Before scheduling this should affect anything.
    window.dispatchEvent(event);
    tracker.schedule(() => {
      expectEq(timer.now(), 50, 'time at task 1:');
      runCount++;
    }, 50);
    timer.moveAhead(/* ms= */ 10, /* times= */ 10);  // -> 100ms total
    expectEq(runCount, 1, 'timer run count :');
    expectEq(timer.timers.size, 0, 'number of timers :');

    // Now start prechecks for activity with nothing scheduled.
    tracker.startActivityListeners();
    timer.moveAhead(/* ms= */ 10, /* times= */ 2);  // -> 120ms total
    window.dispatchEvent(event);
    timer.moveAhead(/* ms= */ 10, /* times= */ 3);  // -> 150ms total
    // Schedule task for 200ms (150 + 50).
    tracker.schedule(() => {
      // But considering the activity at 120ms, this should run at 220ms.
      expectEq(timer.now(), 220, 'time at task 2:');
      runCount++;
    }, 50);
    timer.moveAhead(/* ms= */ 10, /* times= */ 10);  // -> 250ms total
    expectEq(runCount, 2, 'timer run count :');
    // No task, no timer.
    expectEq(timer.timers.size, 0, 'number of timers :');

    // Stop checks.
    tracker.stopActivityListeners();
    window.dispatchEvent(event);
    tracker.schedule(() => {
      expectEq(timer.now(), 300, 'time at task 3:');
      runCount++;
    }, 50);
    timer.moveAhead(/* ms= */ 10, /* times= */ 7);  // -> 320ms total
    expectEq(runCount, 3, 'timer run count :');
    // No task, no timer.
    expectEq(timer.timers.size, 0, 'number of timers :');
  }

  // Tests for proper cleanup when shutdown if called.
  testIdleTaskTrackerShutdown() {
    const timer = new FakeTaskTimer();
    const tracker = new IdleTaskTracker(timer, 100, 10);
    tracker.schedule(() => {
      fail('task should not run');
    }, 50);
    timer.moveAhead(/* ms= */ 10, /* times= */ 4);  // -> 40ms total
    tracker.shutdown();
    timer.moveAhead(/* ms= */ 10, /* times= */ 6);  // -> 100ms total
    expectEq(timer.timers.size, 0, 'number of timers :');
  }
}

export {TestTextTasks}
