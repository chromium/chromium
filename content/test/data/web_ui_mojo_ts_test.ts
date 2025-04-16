// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MappedOptionalContainer, StringDictType, TestNode} from './web_ui_mojo_ts_test_mapped_types.js';
import {MojoResultTestCallbackRouter, MojoResultTestReceiver, MojoResultTestRemote, OptionalNumericsStruct, Result, TestEnum, WebUITsMojoTestCache} from './web_ui_ts_test.test-mojom-webui.js';
import {StringWrapper} from './web_ui_ts_test_types.test-mojom-webui.js';

const TEST_DATA: Array<{url: string, contents: string}> = [
  { url: 'https://google.com/', contents: 'i am in fact feeling lucky' },
  { url: 'https://youtube.com/', contents: 'probably cat videos?' },
  { url: 'https://example.com/', contents: 'internets wow' },
];

function assert(condition: any, msg: string) {
  if (!condition) {
    throw new Error('assertion failed: ' + msg);
  }
}

function assertArrayEquals(a: Array<any>, b: Array<any>, msg: string) {
  assert(a.length === b.length, msg);
  for (let i = 0; i < a.length; ++i) {
    assert(a[i] === b[i], msg);
  }
}

function assertObjectEquals(a: any, b: any, msg: string) {
  assert(Object.keys(a).length === Object.keys(b).length, msg);
  for (let key of Object.keys(a)) {
    assert(a[key] === b[key], msg);
  }
}

async function doTest(): Promise<boolean> {
  const cache = WebUITsMojoTestCache.getRemote();
  for (const entry of TEST_DATA) {
    cache.put({ url: entry.url }, entry.contents);
    let stringWrapper = StringWrapper.getRemote();
    stringWrapper.putString(entry.contents);
    cache.addStringWrapper(stringWrapper);
  }

  const {items} = await cache.getAll();
  if (items.length !== TEST_DATA.length) {
    return false;
  }

  const entries: {[key: string]: string } = {};
  for (const item of items) {
    entries[item.url.url] = item.contents;
  }

  for (const entry of TEST_DATA) {
    if (!(entry.url in entries)) {
      return false;
    }
    if (entries[entry.url] != entry.contents) {
      return false;
    }
  }

  const {stringWrapperList} = await cache.getStringWrapperList();
  if (stringWrapperList.length !== TEST_DATA.length) {
    return false;
  }

  let stringsInList = [];
  for (const stringWrapper of stringWrapperList) {
    let {item} = await stringWrapper.getString();
    stringsInList.push(item);
  }

  for (const entry of TEST_DATA) {
    if (!stringsInList.includes(entry.contents)) {
      return false;
    }
  }

  {
    const testStruct: OptionalNumericsStruct = {
      optionalBool: true,
      optionalUint8: null,
      optionalEnum: TestEnum.kOne,
    };

    const {
      optionalBool,
      optionalUint8,
      optionalEnum,
      optionalNumerics,
      optionalBools,
      optionalInts,
      optionalEnums,
      boolMap,
      intMap,
      enumMap
    } =
        await cache.echo(
            true, null, TestEnum.kOne, testStruct, [], [], [], {}, {}, {}, '',
            new TestNode(), null);
    if (optionalBool !== false) {
      return false;
    }
    if (optionalUint8 !== null) {
      return false;
    }
    if (optionalEnum !== TestEnum.kTwo) {
      return false;
    }
    if (optionalNumerics.optionalBool !== false) {
      return false;
    }
    if (optionalNumerics.optionalUint8 !== null) {
      return false;
    }
    if (optionalNumerics.optionalEnum !== TestEnum.kTwo) {
      return false;
    }
    for (const arr of [optionalBools, optionalInts, optionalEnums]) {
      assertArrayEquals(arr, [], 'empty array');
    }
    for (const map of [boolMap, intMap, enumMap]) {
      assertObjectEquals(map, [], 'empty map');
    }
  }
  {
    const testStruct: OptionalNumericsStruct = {
      optionalBool: null,
      optionalUint8: 1,
      optionalEnum: null,
    };

    const inOptionalBools = [false, null, true];
    const inOptionalInts = [null, 0, 1, null];
    const inOptionalEnums = [null, 0, null, 1, null];
    const inBoolMap = {0: true, 1: false, 2: null};
    const inIntMap = {0: 0, 2: null};
    const inEnumMap = {0: 0, 1: null};

    const {
      optionalBool,
      optionalUint8,
      optionalEnum,
      optionalNumerics,
      optionalBools,
      optionalInts,
      optionalEnums,
      boolMap,
      intMap,
      enumMap
    } =
        await cache.echo(
            null, 1, null, testStruct, inOptionalBools, inOptionalInts,
            inOptionalEnums, inBoolMap, inIntMap, inEnumMap, '', new TestNode(),
            null);
    if (optionalBool !== null) {
      return false;
    }
    if (optionalUint8 !== 254) {
      return false;
    }
    if (optionalEnum !== null) {
      return false;
    }
    if (optionalNumerics.optionalBool !== null) {
      return false;
    }
    if (optionalNumerics.optionalUint8 !== 254) {
      return false;
    }
    if (optionalNumerics.optionalEnum !== null) {
      return false;
    }

    assertArrayEquals(inOptionalBools, optionalBools, 'optional bools');
    assertArrayEquals(inOptionalInts, optionalInts, 'optional ints');
    assertArrayEquals(inOptionalEnums, optionalEnums, 'optional enums');

    assertObjectEquals(inBoolMap, boolMap, 'bool map');
    assertObjectEquals(inIntMap, intMap, 'bool int');
    assertObjectEquals(inEnumMap, enumMap, 'enum map');
  }

  const testStruct: OptionalNumericsStruct = {
    optionalBool: null,
    optionalUint8: null,
    optionalEnum: null,
  };
  // Test simple mapped type where a struct is mapped to a string.
  {
    const str = 'foobear';
    const result = await cache.echo(
        null, 1, null, testStruct, [], [], [], {}, {}, {}, str, new TestNode(),
        null);

    if (result.simpleMappedType !== str) {
      return false;
    }
  }

  // Tests an empty nested struct to test basic encoding/decoding.
  {
    const result = await cache.echo(
        null, 1, null, testStruct, [], [], [], {}, {}, {}, '', new TestNode(),
        null);

    assertObjectEquals(
        new TestNode(), result.nestedMappedType,
        'nested mappped type: got: ' + JSON.stringify(result.nestedMappedType) +
            ', expected: {next: null}');
  }

  // Tests a nested type where a struct includes itself.
  {
    const depth = 10;
    const chain: TestNode = new TestNode();
    let cursor = chain;
    for (let i = 0; i < depth; ++i) {
      cursor = cursor!.next = new TestNode();
    }
    const result = await cache.echo(
        null, 1, null, testStruct, [], [], [], {}, {}, {}, '', chain, null);

    if (JSON.stringify(chain) !== JSON.stringify(result.nestedMappedType)) {
      throw new Error(
          'nested mappped type: got: ' +
          JSON.stringify(result.nestedMappedType) +
          ', expected: ' + JSON.stringify(chain));
    }
  }

  {
    let map: StringDictType = new Map<string, string>();
    map.set('foo', 'bear');
    map.set('some', 'where');
    const result = await cache.echo(
        null, 1, null, testStruct, [], [], [], {}, {}, {}, '', new TestNode(),
        map);

    for (const key of map.keys()) {
      assert(
          result.otherMappedType!.get(key) === map.get(key),
          `Expected value: ${map.get(key)} for key: ${key}, got: ${
              result.otherMappedType!.get(key)}`);
    }
  }

  {
    const token = '0123456789ABCDEFBEEFDEADDEADBEEF';
    const result = await cache.echoTypemaps(
        new Date(12321),
        token,
    );
    assert(
        result.time.getTime() === new Date(12321).getTime(),
        `unexpected date received ${result.time.getTime()}`);
    assert(result.token === token, `unexpected token ${token}`);
  }

  const assertTypemapContainerEquals =
      (expected: MappedOptionalContainer, result: MappedOptionalContainer,
       msg: string) => {
        assert(expected.optionalInt === result.optionalInt, msg);
        assertArrayEquals(expected.bools, result.bools, msg);
        assertObjectEquals(expected.optionalMap, result.optionalMap, msg);
      };

  {
    const withNulls = {
      optionalInt: null,
      bools: [null, null, null],
      optionalMap: {'foo': null}
    };
    const result = await cache.echoOptionalTypemaps(withNulls);
    assertTypemapContainerEquals(
        withNulls, result.result,
        `unexpected object ${JSON.stringify(result)}, expected: ${
            JSON.stringify(withNulls)}`);
  }

  {
    const withValues = {
      optionalInt: 6,
      bools: [null, false, null, true, null],
      optionalMap: {'foo': null, 'bear': true}
    };
    const result = await cache.echoOptionalTypemaps(withValues);
    assertTypemapContainerEquals(
        withValues, result.result,
        `unexpected object ${JSON.stringify(result.result)}, expected: ${
            JSON.stringify(withValues)}`);
  }

  // Loopback test for result types.
  {
    // Test general success case.
    const listener = {
      testResult:
          (():
               Promise<Result> => {
                 return Promise.resolve({secretMessage: `it's all for naught`});
               })
    };
    const service = new MojoResultTestReceiver(listener);
    const client: MojoResultTestRemote = service.$.bindNewPipeAndPassRemote();

    await client.testResult().then(result => {
      assert(
          result.secretMessage === `it's all for naught`,
          `got unexpected msg: ${JSON.stringify(result)}`);
    });
  }

  {
    // Tests listener pattern.
    const callbacks = new MojoResultTestCallbackRouter();
    const client = callbacks.$.bindNewPipeAndPassRemote();
    callbacks.testResult.addListener(
        () => Promise.resolve({secretMessage: 'I listen'}));

    await client.testResult().then(result => {
      assert(
          result.secretMessage === 'I listen',
          `got unexpected msg: ${JSON.stringify(result)}`);
    });
  }

  {
    // Tests rejection.
    const callbacks = new MojoResultTestCallbackRouter();
    const client = callbacks.$.bindNewPipeAndPassRemote();
    callbacks.testResult.addListener(
        () => Promise.reject(new Error('cannot go on')));

    await client.testResult()
        .then(() => {
          assert(false, 'should have failed');
        })
        .catch((error: Error) => {
          assert(error.message === 'cannot go on', JSON.stringify(error));
        });
  }

  {
    // Tests loose js error encoding for JsError.
    const callbacks = new MojoResultTestCallbackRouter();
    const client = callbacks.$.bindNewPipeAndPassRemote();
    callbacks.testResult.addListener(
        () => Promise.reject({message: 'cannot go on'}));

    await client.testResult()
        .then(() => {
          assert(false, 'should have failed');
        })
        .catch((error: Error) => {
          assert(error.message === 'cannot go on', JSON.stringify(error));
        });
  }

  {
    // Tests throwing.
    const callbacks = new MojoResultTestCallbackRouter();
    const client = callbacks.$.bindNewPipeAndPassRemote();
    callbacks.testResult.addListener(() => {
      throw new Error('oh noes');
    });

    await client.testResult()
        .then(() => {
          assert(false, 'should have failed');
        })
        .catch((error: Error) => {
          assert(error.message === 'oh noes', JSON.stringify(error));
        });
  }

  {
    // Tests unknown object to JsError mapping.
    class Potato {}
    const callbacks = new MojoResultTestCallbackRouter();
    const client = callbacks.$.bindNewPipeAndPassRemote();
    callbacks.testResult.addListener(() => {
      throw new Potato();
    });

    await client.testResult()
        .then(() => {
          assert(false, 'should have failed');
        })
        .catch((error: Error) => {
          assert(
              error.message === 'unknown error has occured',
              JSON.stringify(error));
        });
  }
  return true;
}

async function runTest(): Promise<boolean> {
  return doTest();
}

Object.assign(window, {runTest});
