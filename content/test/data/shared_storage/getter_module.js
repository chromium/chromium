// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


class GetOperation {
  async run(data) {
    console.log(`sharedStorage.length(): ` + await sharedStorage.length());
    if (data && data.hasOwnProperty('key')) {
      console.log(
          `sharedStorage.get('${data['key']}'): ` +
          await sharedStorage.get(data['key']));
    } else {
      console.log('No `data`, or `data` does not have `key`.');
    }
  }
}

register('get-operation', GetOperation);
