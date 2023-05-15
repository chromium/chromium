// Copyright 2018 The Chromium Authors
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

async function testFunctionNumerics() {
  let remote = test.mojom.TestPageHandler.getRemote();

  // Tests that compiling passing values as optional numeric params works.
  let result = await remote.methodWithOptionalNumerics(
    /*optionalBool=*/ true,
    /*optionalUnit8=*/ 1,
    /*optionalInt8=*/ -1,
    /*optionalUint16=*/ 1,
    /*optionalInt16=*/ -1,
    /*optionalUint32=*/ 1,
    /*optionalInt32=*/ -1,
    /*optionalUint64=*/ BigInt('1'),
    /*optionalInt64=*/ BigInt('-1'),
    /*optionalFloat=*/ 1.0,
    /*optionalDouble=*/ 1.0,
    /*optionalEnum=*/ test.mojom.TestEnum.FIRST);

  // The following assignments test that the returned values
  // have the right type. For example the returned optionalBool should be
  // a nullable boolean i.e. `?boolean`. If it wasn't we wouldn't be able
  // to assign both a boolean and null to it: compilation would fail.

  /** @type {?boolean} */
  let optionalBool = result.optionalBool;
  result.optionalBool = true;
  result.optionalBool = null;

  /** @type {?number} */
  let optionalUint8 = result.optionalUint8;
  result.optionalUint8 = 1;
  result.optionalUint8 = null;

  /** @type {?number} */
  let optionalInt8 = result.optionalInt8;
  result.optionalInt8 = -1;
  result.optionalInt8 = null;

  /** @type {?number} */
  let optionalUint16 = result.optionalUint16;
  result.optionalUint16 = 1;
  result.optionalUint16 = null;

  /** @type {?number} */
  let optionalInt16 = result.optionalInt16;
  result.optionalInt16 = -1;
  result.optionalInt16 = null;

  /** @type {?number} */
  let optionalUint32 = result.optionalUint32;
  result.optionalUint32 = 1;
  result.optionalUint32 = null;

  /** @type {?number} */
  let optionalInt32 = result.optionalInt32;
  result.optionalInt32 = -1;
  result.optionalInt32 = null;

  /** @type {?bigint} */
  let optionalUint64 = result.optionalUint64;
  result.optionalUint64 = BigInt('64');
  result.optionalUint64 = null;

  /** @type {?bigint} */
  let optionalInt64 = result.optionalInt64;
  result.optionalInt64 = BigInt('-64');
  result.optionalInt64 = null;

  /** @type {?number} */
  let optionalFloat = result.optionalFloat;
  result.optionalFloat = 1.0;
  result.optionalFloat = null;

  /** @type {?number} */
  let optionalDouble = result.optionalDouble;
  result.optionalDouble = 1.0;
  result.optionalDouble = null;

  /** @type {?test.mojom.TestEnum} */
  let optionalEnum = result.optionalEnum;
  result.optionalEnum = test.mojom.TestEnum.SECOND;
  result.optionalEnum = null;

  // Tests compiling passing null as optional numeric params works.
  await remote.methodWithOptionalNumerics(
    /*optionalBool=*/ null,
    /*optionalUnit8=*/ null,
    /*optionalInt8=*/ null,
    /*optionalUint16=*/ null,
    /*optionalInt16=*/ null,
    /*optionalUint32=*/ null,
    /*optionalInt32=*/ null,
    /*optionalUint64=*/ null,
    /*optionalInt64=*/ null,
    /*optionalFloat=*/ null,
    /*optionalDouble=*/ null,
    /*optionalEnum=*/ null);
}

async function testFunctionNumericsStruct() {
  /** @type {!test.mojom.TestStructOptionalNumerics} */
  let input = {};

  // The following assignments test that the struct annotations have the
  // the right type. For example `optionalBool` should be boolean or
  // undefined i.e. `boolean|undefined`. If it wasn't we wouldn't be able to
  // assign both a boolean and `undefined` to it: compilation would fail.
  /** @type {boolean|undefined} */
  input.optionalBool = true;
  input.optionalBool = undefined;

  input.optionalUint8 = 1;
  input.optionalUint8 = undefined;

  input.optionalInt8 = -1;
  input.optionalInt8 = undefined;

  input.optionalUint16 = 1;
  input.optionalUint16 = undefined;

  input.optionalInt16 = -1;
  input.optionalInt16 = undefined;

  input.optionalUint32 = 1;
  input.optionalUint32 = undefined;

  input.optionalInt32 = -1;
  input.optionalInt32 = undefined;

  input.optionalUint64 = BigInt('1');
  input.optionalUint64 = undefined;

  input.optionalInt64 = BigInt('-1');
  input.optionalInt64 = undefined;

  input.optionalFloat = 1.0;
  input.optionalFloat = undefined;

  input.optionalDouble = 1.0;
  input.optionalDouble = undefined;

  input.optionalEnum = test.mojom.TestEnum.FIRST;
  input.optionalEnum = undefined;

  let remote = test.mojom.TestPageHandler.getRemote();

  // Test that the response struct is a TestStructOptionalNumerics. If
  // it wasn't, trying to assign `out` to `response` would fail.
  const {out} = await remote.methodWithStructWithOptionalNumerics(input);
  /** @type {!test.mojom.TestStructOptionalNumerics} */
  let response = out;

  // The following assignments test that the struct annotations have the
  // the right type. For example `optionalBool` should be boolean or
  // undefined i.e. `boolean|undefined`. If it wasn't we wouldn't be able to
  // assign both a boolean and `undefined` to it: compilation would fail.
  out.optionalBool = true;
  out.optionalBool = undefined;

  out.optionalUint8 = 1;
  out.optionalUint8 = undefined;

  out.optionalInt8 = -1;
  out.optionalInt8 = undefined;

  out.optionalUint16 = 1;
  out.optionalUint16 = undefined;

  out.optionalInt16 = -1;
  out.optionalInt16 = undefined;

  out.optionalUint32 = 1;
  out.optionalUint32 = undefined;

  out.optionalInt32 = -1;
  out.optionalInt32 = undefined;

  out.optionalUint64 = BigInt('1');
  out.optionalUint64 = undefined;

  out.optionalInt64 = BigInt('-1');
  out.optionalInt64 = undefined;

  out.optionalFloat = 1.0;
  out.optionalFloat = undefined;

  out.optionalDouble = 1.0;
  out.optionalDouble = undefined;

  out.optionalEnum = test.mojom.TestEnum.FIRST;
  out.optionalEnum = undefined;
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

  /** @override */
  onEventWithStructWithOptionalNumerics(s) {
    // Test that the argument is a TestStructOptionalNumerics. If
    // it wasn't, trying to assign `s` to `arg` would fail.
    /** @type {!test.mojom.TestStructOptionalNumerics} */
    let arg = s;

    // The following assignments test that the struct annotations have the
    // the right type. For example `optionalBool` should be boolean or
    // undefined i.e. `boolean|undefined`. If it wasn't we wouldn't be able to
    // assign both a boolean and `undefined` to it: compilation would fail.
    s.optionalBool = true;
    s.optionalBool = undefined;

    s.optionalUint8 = 1;
    s.optionalUint8 = undefined;

    s.optionalInt8 = -1;
    s.optionalInt8 = undefined;

    s.optionalUint16 = 1;
    s.optionalUint16 = undefined;

    s.optionalInt16 = -1;
    s.optionalInt16 = undefined;

    s.optionalUint32 = 1;
    s.optionalUint32 = undefined;

    s.optionalInt32 = -1;
    s.optionalInt32 = undefined;

    s.optionalUint64 = BigInt('1');
    s.optionalUint64 = undefined;

    s.optionalInt64 = BigInt('-1');
    s.optionalInt64 = undefined;

    s.optionalFloat = 1.0;
    s.optionalFloat = undefined;

    s.optionalDouble = 1.0;
    s.optionalDouble = undefined;

    s.optionalEnum = test.mojom.TestEnum.FIRST;
    s.optionalEnum = undefined;
  }
}
