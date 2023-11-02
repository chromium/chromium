// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing erroneous_function_module.js')

class TestOperation {
  async run(data) {
    console.log('Start executing \'test-operation\'');
    undefinedVariable
    console.log('Finish executing \'test-operation\'');
  }
}

register("test-operation", TestOperation);

console.log('Finish executing erroneous_function_module.js')
