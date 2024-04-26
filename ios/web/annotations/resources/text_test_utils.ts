// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript test harness.
 */

import {TaskTimer} from '//ios/web/annotations/resources/text_tasks.js';

// TODO(crbug.com/40936184): move to general ts utilities.

// Fake time TaskTimer.
class FakeTaskTimer implements TaskTimer {
  // Fake clock.
  nowMs = 0;

  timers = new Map<number, {then: Function, at: number}>();
  uniqueId = 0;

  clear(id: number): void {
    this.timers.delete(id);
  }
  reset(then: Function, ms: number): number {
    const id = ++this.uniqueId;
    this.timers.set(id, {then, at: this.now() + ms});
    return id;
  }
  now(): number {
    return this.nowMs;
  }

  // Fake controls.

  // Finds and returns next event id or null.
  nextEventId(): number|null {
    let next: number|null = null;
    this.timers.forEach((value, key) => {
      if (next === null || value.at < this.timers.get(next)!.at) {
        next = key;
      }
    });
    return next;
  }

  // Move clock and trigger all events that need triggering.
  moveAhead(ms: number, times = 1): void {
    while (times > 0) {
      this.nowMs += ms;
      let next = this.nextEventId();
      while (next) {
        const event = this.timers.get(next)!;
        if (event.at > this.nowMs) {
          break;
        }
        this.timers.delete(next);
        event.then();
        next = this.nextEventId();
      }
      times--;
    }
  }

  // Clear tasks and put time back to 0.
  restart() {
    this.nowMs = 0;
    this.timers.clear();
  }
}

// Result of a single test.
interface TestResult {
  name: string;
  result: string;
  error?: string;
}

// Base class for any test suite.
// Example:
// class TestFoo extends TestSuite {
//   override setUpSuite(): void {
//     createFooSingleton();
//   }
//   override tearDownSuite(): void {
//     destroyFooSingleton();
//   }
//   override setUp(): void {
//     load('<foo id="bar">Bar</foo>');
//     fooSingleton.startMonitor();
//   }
//   override tearDown(): void {
//     fooSingleton.stopMonitor();
//   }
//
//   testBar() {
//     expectEq(fooSingleton.monitoredTextFor('bar'), 'Bar');
//   }
// }
//
// new TestFoo().run();
class TestSuite {
  private results: TestResult[] = [];

  // Called once when starting suite in `run`.
  setUpSuite(): void {
    document.body.innerHTML = '';
    document.head.innerHTML = '';
  }
  // Called before each test in `run`.
  setUp(): void {}

  // Iterates and executes methods starting with 'test'.
  run(): TestResult[] {
    this.results = [];
    const tryPhase = (phase: string, callback: Function) => {
      try {
        callback();
      } catch (error) {
        this.results.push({
          name: phase,
          result: 'FAILED',
          error: '' + error + '\n' + (error as Error).stack
        });
      }
    };
    tryPhase('setUpSuite', () => {
      this.setUpSuite();
    });
    for (const method of Object.getOwnPropertyNames(
             Object.getPrototypeOf(this))) {
      if (method.startsWith('test')) {
        tryPhase('setUp(' + method + ')', () => {
          this.setUp();
        });
        tryPhase(method, () => {
          (this as any)[method]();
          this.results.push({name: method, result: 'OK'});
        });
        tryPhase('tearDown(' + method + ')', () => {
          this.tearDown();
        });
      }
    }
    tryPhase('tearDownSuite', () => {
      this.tearDownSuite();
    });
    return this.results;
  }

  // Called after each test in `run`.
  tearDown(): void {}
  // Called once when ending suite in `run`.
  tearDownSuite(): void {}

  // Add debug information to log.
  log(data: any): void {
    this.results.push({name: 'log', result: 'LOG', error: '' + data});
  }
}

// Throws exception if `a` !== `b`.
function expectEq(a: any, b: any, info = ''): void {
  if (a !== b) {
    throw new Error(info + `"${a}" !== "${b}"`);
  }
}

// Throws exception if `a` === `b`.
function expectNeq(a: any, b: any, info = ''): void {
  if (a === b) {
    throw new Error(info + `"${a}" === "${b}"`);
  }
}

// Throws exception with `info`.
function fail(info = ''): void {
  throw new Error(info);
}

// Loads given `html` into page body.
function load(html: string): void {
  document.body.innerHTML = html;
}

// Loads given `html` into page head.
function loadHead(html: string): void {
  document.head.innerHTML = html;
}

export {
  FakeTaskTimer,
  TestSuite,
  expectEq,
  expectNeq,
  fail,
  load,
  loadHead,
}
