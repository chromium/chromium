// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

class TestHarness {
  finished = false;
  success = false;
  skipped = false;
  message = 'ok';
  logs = [];
  logWindow = null;
  cleanups = [];

  constructor() {}

  skip(message) {
    this.skipped = true;
    this.finished = true;
    this.message = message;
    this.log('Test skipped: ' + message);
  }

  reportSuccess() {
    this.finished = true;
    this.success = true;
    this.log('Test completed');
  }

  reportFailure(error) {
    this.finished = true;
    this.success = false;
    this.message = error.toString();
    this.log(this.message);
  }

  assert(condition, msg) {
    if (!condition)
      this.reportFailure("Assertion failed: " + msg);
  }

  assert_equals(val1, val2, msg) {
    if (val1 != val2) {
      this.reportFailure(`Assertion failed: ${msg}. ${JSON.stringify(val1)} ` +
                         `!= ${JSON.stringify(val2)}.`);
    }
  }

  assert_not_equals(val1, val2, msg) {
    if (val1 == val2) {
      this.reportFailure(`Assertion failed: ${msg}. ${JSON.stringify(val1)} ` +
                         `== ${JSON.stringify(val2)}.`);
    }
  }

  summary() {
    return this.message + "\n\n" + this.logs.join("\n");
  }

  log(msg) {
    this.logs.push(msg);
    console.log(msg);
    if (this.logWindow === null)
      this.logWindow = document.getElementById('consoleId');
    if (this.logWindow)
      this.logWindow.innerText += msg + '\n';
  }

  addCleanup(cb) {
    this.cleanups.push(cb);
  }

  run(arg) {
    main(arg).then(
        _ => {
          if (!this.finished)
            this.reportSuccess();
        },
        error => {
          if (!this.finished)
            this.reportFailure(error);
        }).finally(() => {
          let cleanup = this.cleanups.pop()
          while (cleanup) {
            cleanup();
            cleanup = this.cleanups.pop();
          }
        });
  }
};

var TEST = new TestHarness();
