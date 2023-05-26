// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class SlowOperation {
  async run(data) {
    privateAggregation.contributeToHistogram({ bucket: 1n, value: 1 });
    for (let i = 0; i < 100; i++) {
      await sharedStorage.get('example-key')
    }
    privateAggregation.contributeToHistogram({ bucket: 2n, value: 1 });
  }
}

class FastOperation {
  async run(data) {
    privateAggregation.contributeToHistogram({ bucket: 3n, value: 1 });
  }
}

register("slow-operation", SlowOperation);
register("fast-operation", FastOperation);
