// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

var TEST = {
  finished: false,
  success: false,
  message: "ok",
  logs: [],

  reportSuccess: function () {
    this.finished = true;
    this.success = true;
  },

  reportFailure: function (error) {
    this.finished = true;
    this.success = false;
    this.message = error.toString();
    this.log(this.message);
  },

  assert: function (condition, msg) {
    if (!condition)
      this.reportFailure("Assertion failed: " + msg);
  },

  summary: function () {
    return this.message + "\n\n" + this.logs.join("\n");
  },

  log: function (msg) {
    this.logs.push(msg);
    console.log(msg);
  },

  run: function (arg) {
    let p = main(arg);

    if (p) {
      p.then(
        _ => {
          if (!this.finished)
            this.reportSuccess();
        },
        error => {
          if (!this.finished)
            this.reportFailure(error);
        }
      );
    }
  }
};

async function createFrame(width, height, ts) {
  let canvas = new OffscreenCanvas(width, height);
  let ctx = canvas.getContext('2d', { alpha: false });

  let gradient = ctx.createLinearGradient(0, 0, width, height);
  gradient.addColorStop(0, 'magenta');
  gradient.addColorStop(0.15, 'blue');
  gradient.addColorStop(0.30, 'green');
  gradient.addColorStop(0.70, 'yellow');
  gradient.addColorStop(0.85, 'orange');
  gradient.addColorStop(1.0, 'red');
  ctx.fillStyle = gradient;
  ctx.fillRect(0, 0, width, height);

  ctx.fillStyle = 'black';
  ctx.font = (height / 4) + 'px fantasy';
  ctx.fillText(ts.toString(), width / 2, height / 2);

  return new VideoFrame(canvas, { timestamp: ts });
}

function delay(time_ms) {
  return new Promise((resolve, reject) => {
    setTimeout(resolve, time_ms);
  });
};