// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {StringDictType, TestNode} from './web_ui_mojo_ts_test_mapped_types.js';
import {OptionalNumericsStruct, TestEnum, WebUITsMojoTestCache} from './web_ui_ts_test.test-mojom-webui.js';

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
  }

  const {items} = await cache.getAll();
  if (items.length != TEST_DATA.length) {
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

  return true;
}

async function runTest(): Promise<boolean> {
  return doTest();
}

Object.assign(window, {runTest});
