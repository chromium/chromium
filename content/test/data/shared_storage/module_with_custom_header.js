// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestOperation {
  async run(data) {
    if (data && data.hasOwnProperty('set-key') &&
        data.hasOwnProperty('set-value')) {
      await sharedStorage.set(data['set-key'], data['set-value']);
    }
  }
}

register("test-operation", TestOperation);
