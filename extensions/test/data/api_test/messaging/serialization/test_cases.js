// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export const testCases = [
  // Test basic serializable message types.
  {
    name: 'null',
    message: null,
    expected: null,
  },
  {
    name: 'boolean',
    message: true,
    expected: true,
  },
  {
    name: 'number',
    message: 123,
    expected: 123,
  },
  {
    name: 'string',
    message: 'hello',
    expected: 'hello',
  },
  {
    name: 'array',
    message: [1, 'a', null],
    expected: [1, 'a', null],
  },
  {
    name: 'object',

    message: {a: 1, b: 'c'},
    expected: {a: 1, b: 'c'},
  },

  // Test edge cases for message serialization.
  {
    name: 'undefined',
    message: undefined,
    // Note: `undefined` is a special case. When sent as the top-level
    // message, it arrives as `null` on the other side since JSON.stringify
    // won't serialize undefined, and previous choices decided `null` is the
    // best equivalent.
    expected: null,
  },
  {
    name: 'array with undefined',
    message: [1, undefined, 2],
    expected: [1, null, 2],
  },
  {
    name: 'object with undefined property',
    message: {a: 1, b: undefined},
    expected: {a: 1},
  },
  {
    name: 'array with function',
    message: [1, () => {}, 2],
    expected: [1, null, 2],
  },
  {
    name: 'object with function property',
    message: {a: 1, b: () => {}},
    expected: {a: 1},
  },
  {
    name: 'NaN',
    message: NaN,
    expected: null,
  },
  {
    name: 'Infinity',
    message: Infinity,
    expected: null,
  },
  {
    name: '-Infinity',
    message: -Infinity,
    expected: null,
  },
  {
    name: 'object with toJSON',
    message: {
      a: 1,
      toJSON: () => {
        return {b: 2};
      }
    },
    expected: {b: 2},
  },
];

export const unserializableTestCases = [
  {
    name: 'BigInt',
    message: 123n,
  },
  {
    name: 'object with BigInt',
    message: {a: 123n},
  },
  {
    name: 'Symbol',
    message: Symbol('foo'),
  },
  {
    name: 'function',
    message: () => {},
  },
  // TODO(crbug.com/40321352): This results in `{}` being sent and `{}` being
  // returned, but they don't pass chrome.test.assertEq...why is that?
  {
    name: 'object with Symbol',
    message: {a: Symbol('foo')},
    expected: {a: Symbol('foo')},
  },
];
