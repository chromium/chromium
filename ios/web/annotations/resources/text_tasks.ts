// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tasks and Idle state related utilities.
 */

// Interface for all real time operations. Can be mocked easily.
interface TaskTimer {
  clear(id: number): void;
  reset(then: Function, ms: number): number;
  now(): number;
}

// Real time TaskTimer.
class LiveTaskTimer implements TaskTimer {
  clear(id: number): void {
    clearTimeout(id);
  }
  reset(then: Function, ms: number): number {
    return setTimeout(then, ms);
  }
  now(): number {
    return Date.now();
  }
}

// Minimum delay between two tasks.
const TASK_SEPARATOR_MS = 50;

// Minimum delay after user activity.
const TASK_ACTIVITY_DELAY_MS = 300;

// Task manager wrapper `TaskTimer`. It also monitors user's activity and pushes
// back the timer if needed.
// TODO(crbug.com/40936184): replace by requestIdleCallback when available or
// move to general ts utilities.
class IdleTaskTracker {
  // The events to monitor for user activity.
  private eventNames = [
    'gesturechange', 'gesturestart', 'mousemove', 'mousedown', 'touchmove',
    'touchstart', 'click', 'keydown', 'scroll'
  ];

  // Id for the current timer for the next task.
  private timerId = 0;

  // Keeps track of calls to `startActivityListeners` and
  // `stopActivityListeners` to properly install and remove event listeners.
  private activityListenersCount = 0;

  // The last time a user activity was recorded.
  public lastActivityMs = 0;

  // Map of all tasks, keyed by the task callback `Function`. `value` is the
  // current wake up time requested.
  private tasks = new Map<Function, number>();

  // Requires a `taskTimer` so it can be easily testable. `activityDelayMs` is
  // the delay between a user activity and when the next task should be
  // triggered (if any). `minimumTaskDelayMs` is an extra delay between two
  // tasks to avoid blocking the main thread for too long.
  constructor(
      private taskTimer: TaskTimer = new LiveTaskTimer(),
      private activityDelayMs = TASK_ACTIVITY_DELAY_MS,
      private minimumTaskDelayMs = TASK_SEPARATOR_MS) {
    this.lastActivityMs = taskTimer.now() - this.activityDelayMs;
  }

  // Queues the given `task` for execution when the page is idled, starting
  // `wakeUpDelayMs` from now. If the task exists, its new execution time is
  // updated.
  schedule(task: Function, wakeUpDelayMs = this.minimumTaskDelayMs): void {
    const taskStartMs = this.taskTimer.now() + wakeUpDelayMs;
    if (!this.tasks.has(task)) {
      // For each task, start activity listeners only once.
      this.startActivityListeners();
    }
    // Add to tasks and reschedule next wakeup time from new set of tasks.
    this.tasks.set(task, taskStartMs);
    this.setNextWakeup(this.nextWakeUpMs());
  }

  // Stops and clears this scheduler.
  shutdown(): void {
    this.setNextWakeup(0);
    this.activityListenersCount = 1;
    this.stopActivityListeners();
    this.tasks.clear();
  }

  // Starts all activity listeners. Will do it only once when
  // `activityListenersCount` goes from 0 to 1.
  startActivityListeners(): void {
    if (++this.activityListenersCount !== 1) {
      return;
    }
    this.eventNames.forEach((eventName) => {
      window.addEventListener(eventName, this.recordLastActivity);
    });
  }

  // Stops all activity listeners. Will do it only once when
  // `activityListenersCount` goes from 1 to 0.
  stopActivityListeners(): void {
    if (--this.activityListenersCount !== 0) {
      return;
    }
    this.eventNames.forEach((eventName) => {
      window.removeEventListener(eventName, this.recordLastActivity);
    });
  }

  // Returns the next task and its wakeup time.
  private nextTask(): [Function|null, number] {
    let earlierTaskMs = 0;
    let earlierTask: Function|null = null;
    this.tasks.forEach((taskMs: number, task: Function) => {
      if (!earlierTask || taskMs < earlierTaskMs) {
        earlierTaskMs = taskMs;
        earlierTask = task;
      }
    });
    return [earlierTask, earlierTaskMs];
  }

  // Returns next wakeup time based on current set of tasks.
  private nextWakeUpMs(): number {
    let [earlierTask, earlierTaskMs] = this.nextTask();
    if (!earlierTask)
      return 0;
    return earlierTaskMs;
  }

  // Resets timer for the new `wakeUpMs` wakeup time. A `wakeUp` time of 0 stops
  // the timer. Minimum delay will be `minimumTaskDelayMs` from now, to let user
  // some time between tasks.
  private setNextWakeup(wakeUpMs: number): void {
    if (this.timerId) {
      this.taskTimer.clear(this.timerId);
      this.timerId = 0;
    }
    if (wakeUpMs > 0) {
      const now = this.taskTimer.now();
      wakeUpMs = Math.max(wakeUpMs, now + this.minimumTaskDelayMs);
      this.timerId = this.taskTimer.reset(this.onWakeUp, wakeUpMs - now);
    }
  }

  // Called when the timer triggers. It will check that enough idle time
  // has passed, if not schedule more time, then execute one task. Finally
  // will schedule next wakeup time based on tasks left.
  private onWakeUp = () => {
    const now = this.taskTimer.now();
    if (this.lastActivityMs + this.activityDelayMs > now) {
      this.setNextWakeup(this.lastActivityMs + this.activityDelayMs);
    } else {
      let [earlierTask] = this.nextTask();
      if (earlierTask) {
        this.tasks.delete(earlierTask);
        earlierTask();
        this.stopActivityListeners();
      }
      this.setNextWakeup(this.nextWakeUpMs());
    }
  };

  // Records last activity time event listener.
  private recordLastActivity = () => {
    this.lastActivityMs = this.taskTimer.now();
  };
}

export {
  TASK_SEPARATOR_MS,
  TASK_ACTIVITY_DELAY_MS,
  TaskTimer,
  LiveTaskTimer,
  IdleTaskTracker,
}
