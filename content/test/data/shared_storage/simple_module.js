// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

console.log('Start executing simple_module.js')

class TestOperation {
  async run(data) {
    console.log('Start executing \'test-operation\'');
    console.log(JSON.stringify(data, Object.keys(data).sort()));
    console.log('Finish executing \'test-operation\'');
  }
}

class TestURLSelectionOperation {
  async run(urls, data) {
    console.log('Start executing \'test-url-selection-operation\'');
    console.log(JSON.stringify(urls));
    console.log(JSON.stringify(data, Object.keys(data).sort()));
    console.log('Finish executing \'test-url-selection-operation\'');

    if (data && data.hasOwnProperty('mockResult')) {
      return data['mockResult'];
    }

    return -1;
  }
}

register("test-operation", TestOperation);
register("test-url-selection-operation", TestURLSelectionOperation);

console.log('Finish executing simple_module.js')
