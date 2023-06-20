// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {OptionalNumericsStruct, TestEnum, WebUITsMojoTestCache} from './web_ui_ts_test.test-mojom-webui.js';

const TEST_DATA: Array<{url: string, contents: string}> = [
  { url: 'https://google.com/', contents: 'i am in fact feeling lucky' },
  { url: 'https://youtube.com/', contents: 'probably cat videos?' },
  { url: 'https://example.com/', contents: 'internets wow' },
];

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
      optionalUint8: undefined,
      optionalEnum: TestEnum.kOne,
    };

    const {optionalBool, optionalUint8, optionalEnum, optionalNumerics} =
        await cache.echo(true, null, TestEnum.kOne, testStruct);
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
  }
  {
    const testStruct: OptionalNumericsStruct = {
      optionalBool: undefined,
      optionalUint8: 1,
      optionalEnum: undefined,
    };

    const {optionalBool, optionalUint8, optionalEnum, optionalNumerics} =
        await cache.echo(null, 1, null, testStruct);
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
  }

  return true;
}

async function runTest(): Promise<boolean> {
  return doTest();
}

Object.assign(window, {runTest});
