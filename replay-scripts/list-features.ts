// Copyright 2021 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// List all the features that are passed to IsRecordingOrReplaying or FeatureEnabled.
// Run this script from chromium as:
//
// rg --no-ignore "(IsRecordingOrReplaying)|(FeatureEnabled)" | ts-node replay-scripts/list-features
//
// This is done to workaround problems piping output from ripgrep.
// See https://linear.app/replay/issue/RUN-2255 for details.

import fs from "fs";

main();

async function main() {
  const buffers: Buffer[] = [];
  for await (const buf of process.stdin) {
    buffers.push(buf);
  }
  const stdin = Buffer.concat(buffers).toString('utf8');
  const lines = stdin.split("\n");

  const features = new Set<string>();

  for (const line of lines) {
    let match;
    match = /IsRecordingOrReplaying\("(.*?)"/.exec(line);
    if (match) {
      features.add(match[1]);
    }
    match = /FeatureEnabled\("(.*?)"/.exec(line);
    if (match) {
      features.add(match[1]);
    }
  }

  const featuresArray = [...features];
  featuresArray.sort();

  for (const feature of featuresArray) {
    console.log(feature);
  }
}
