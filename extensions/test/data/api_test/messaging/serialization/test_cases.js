// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Automatically populates 'expected' with the 'message' value if the 'expected'
// property is omitted from the test case object. This deduplicates test cases
// where the message is expected to serialize identically and helps identify
// when a different value is expected than the original.
const fillDuplicateExpected = t =>
    ({...t, expected: 'expected' in t ? t.expected : t.message});

// Test basic serializable message types.
export const commonTestCases = [
  // Test basic serializable message types.
  {
    name: 'null',
    message: null,
  },
  {
    name: 'boolean',
    message: true,
  },
  {
    name: 'number',
    message: 123,
  },
  {
    name: 'string',
    message: 'hello',
  },
  {
    name: 'object',
    message: {a: 1, b: 'c'},
  },
].map(fillDuplicateExpected);

export const json = [
  ...commonTestCases,
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
  {
    name: 'object with undefined property',
    message: {a: 1, b: undefined},
    expected: {a: 1},
  },
  {
    name: 'object with function property',
    message: {a: 1, b: () => {}},
    expected: {a: 1},
  },
  {
    name: 'object with Symbol',
    message: {a: Symbol('foo')},
    expected: {},
  },
  // TODO(crbug.com/40321352): Move this to `jsonUnserializableError` once
  // crbug.com/466303357 is resolved.
  {
    name: 'map',
    message: new Map([['a', 1], ['b', 2]]),
    expected: {},
  },
  {
    name: 'date',
    message: new Date('2025-01-01T12:00:00Z'),
    expected: '2025-01-01T12:00:00.000Z',
  },
  {
    name: 'Blob',
    message: new Blob(['hello!'], {type: 'text/plain'}),
    expected: {},
  },
  {
    name: 'Set',
    message: new Set([1, 'a', null]),
    expected: {},
  },
].map(fillDuplicateExpected);

export const structuredClone = [
  ...commonTestCases,
  {
    name: 'BigInt',
    message: 123n,
  },
  {
    name: 'undefined',
    message: undefined,
  },
  {
    name: 'Infinity',
    message: Infinity,
  },
  {
    name: '-Infinity',
    message: -Infinity,
  },
  {
    name: 'object with undefined property',
    message: {a: 1, b: undefined},
  },
  {
    name: 'object with BigInt',
    message: {a: 123n},
  },
  {
    name: 'Date',
    message: new Date('2025-01-01T12:00:00Z'),
  },
].map(fillDuplicateExpected);

const commonContainerTestCases = [
  {
    name: 'array',
    message: [1, 'a', null],
  },
].map(fillDuplicateExpected);

export const jsonObjectType = [
  ...commonContainerTestCases,
  {
    name: 'array with undefined',
    message: [1, undefined, 2],
    expected: [1, null, 2],
  },
  {
    name: 'array with function',
    message: [1, () => {}, 2],
    expected: [1, null, 2],
  },
].map(fillDuplicateExpected);

export const jsonErrorType = [
  {
    name: 'Error',
    message: new Error('test error'),
    expected: {},
  },
  {
    name: 'TypeError',
    message: new TypeError('test type error'),
    expected: {},
  },
  {
    name: 'RangeError',
    message: new RangeError('test range error'),
    expected: {},
  },
  {
    name: 'SyntaxError',
    message: new SyntaxError('test syntax error'),
    expected: {},
  },
  {
    name: 'ReferenceError',
    message: new ReferenceError('test reference error'),
    expected: {},
  },
].map(fillDuplicateExpected);

export const structureCloneObjectType = [
  ...commonContainerTestCases,
  {
    name: 'array with undefined',
    message: [1, undefined, 2],
  },
  {
    name: 'Map',
    message: new Map([['a', 1], ['b', 2]]),
  },
  {
    name: 'Set',
    message: new Set([1, 'a', null]),
  },
].map(fillDuplicateExpected);

export const structureCloneErrorType = [
  {
    name: 'Error',
    message: new Error('test error'),
  },
  {
    name: 'TypeError',
    message: new TypeError('test type error'),
  },
  {
    name: 'RangeError',
    message: new RangeError('test range error'),
  },
  {
    name: 'SyntaxError',
    message: new SyntaxError('test syntax error'),
  },
  {
    name: 'ReferenceError',
    message: new ReferenceError('test reference error'),
  },
].map(fillDuplicateExpected);

const commonUnserializableErrorTestCases = [
  {
    name: 'Symbol',
    message: Symbol('foo'),
  },
  {
    name: 'function',
    message: () => {},
  },
];

export const jsonUnserializableError = [
  ...commonUnserializableErrorTestCases,
  {
    name: 'BigInt',
    message: 123n,
  },
  {
    name: 'object with BigInt',
    message: {a: 123n},
  },
];

export const structureCloneUnserializableError = [
  ...commonUnserializableErrorTestCases,
  {
    name: 'object with Symbol',
    message: {a: Symbol('foo')},
  },
  {
    name: 'array with function',
    message: [1, () => {}, 2],
  },
  {
    name: 'object with function property',
    message: {a: 1, b: () => {}},
  },
];
