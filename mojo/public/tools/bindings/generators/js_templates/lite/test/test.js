// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function testFunction() {
  /** @type {test.mojom.TestPageHandlerRemote} */
  let remote = test.mojom.TestPageHandler.getRemote()

  // Type infers {?{values: !Array<!string>}} from Promise return type.
  let result = await remote.method1(' ', 5);

  /** @type {Array<string>} */
  let values = result.values;

  /** @type {test.mojom.TestStruct} */
  let testStruct = result.ts
}

/** @implements {test.mojom.TestPageInterface} */
class TestPageImpl {
  /** @override */
  onEvent1(s) {
    /** @type {test.mojom.TestStruct} */ let t = s;
    /** @type {string} */ let id = t.id;
    /** @type {string|undefined} */ let title = t.title;
    /** @type {test.mojom.TestEnum} */ let enumValue = t.enums[0];

    /** @type {string} */ let numberToStringMapValue = t.numberToStringMap[5];

    /** @type {test.mojom.Message} */ let messageToMessageArrayValue =
        t.messageToArrayMap.get({message: 'asdf'})[0];

    /** @type {test.mojom.TestEnum} */ let enumToMapMapValue =
        t.enumToMapMap[test.mojom.TestEnum.FIRST][test.mojom.TestEnum.SECOND];
    /** @type {test.mojom.TestPageInterface} */ let handler =
        t.numberToInterfaceProxyMap[3];
    handler.onEvent1(t);
  }
}
