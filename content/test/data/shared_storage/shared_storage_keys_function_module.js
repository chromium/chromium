// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing shared_storage_keys_function_module.js')

class TestOperation {
  async run(data) {
    console.log('Start executing \'test-operation\'');

    for await (const key of sharedStorage.keys()) {
      console.log(key);
    }

    console.log('Finish executing \'test-operation\'');
  }
}

registerOperation("test-operation", TestOperation);

console.log('Finish executing shared_storage_keys_function_module.js')
