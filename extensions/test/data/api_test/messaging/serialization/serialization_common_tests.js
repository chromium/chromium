// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as testCases from './test_cases.js';

const unserializableRuntimeError =
    'Error in invocation of runtime.sendMessage(optional string extensionId, ' +
    'any message, optional object options, optional function callback): ' +
    'Could not serialize message.';
const unserializableTabsError =
    'Error in invocation of tabs.sendMessage(integer tabId, any message, ' +
    'optional object options, optional function callback): Could not ' +
    'serialize message.';

// Checks if two containers (`Array`, `Map`, `Set`) are equal by value. However,
// it can only handle Maps with primitive objects as keys (e.g. not `new
// Map([['key', {'a': 1}]])`).
function containerCheckDeepEq(expected, actual) {
  if (Array.isArray(actual)) {
    return Array.isArray(expected) && actual.length === expected.length &&
        actual.every((val, i) => val === expected[i]);
  }

  if (actual instanceof Map) {
    return expected instanceof Map && actual.size === expected.size &&
        Array.from(actual.entries())
            .every(
                ([key, val]) => expected.has(key) && val === expected.get(key));
  }

  if (actual instanceof Set) {
    return expected instanceof Set && actual.size === expected.size &&
        expected.difference(actual).size === 0;
  }

  return false;
}

// Asserts that two containers are equal by value (except `Map`s with objects
// as keys: `new Map([['key', {'a': 1}]])`).
// TODO(crbug.com/40321352): Swap with the enhanced `chrome.test.assertEq()`
// when crbug.com/466303357 is resolved.
function containerAssertEq(expected, actual) {
  chrome.test.assertTrue(
      containerCheckDeepEq(actual, expected),
      `Deep equality check failed. Expected: ${
          JSON.stringify(expected)}, Actual: ${JSON.stringify(actual)}`);
}

function assertArrayBufferEq(expected, actual) {
  chrome.test.assertTrue(actual instanceof ArrayBuffer, 'Expected ArrayBuffer');
  chrome.test.assertEq(
      expected.byteLength, actual.byteLength, 'Byte length mismatch');
  const view1 = new Uint8Array(expected);
  const view2 = new Uint8Array(actual);
  for (let i = 0; i < view1.length; i++) {
    if (view1[i] !== view2[i]) {
      chrome.test.fail(`ArrayBuffer content mismatch at index ${i}`);
    }
  }
}

function assertTypedArrayEq(expected, actual) {
  chrome.test.assertEq(expected.constructor.name, actual.constructor.name);
  chrome.test.assertEq(expected.length, actual.length);
  for (let i = 0; i < expected.length; i++) {
    if (expected[i] !== actual[i]) {
      chrome.test.fail(
          `${expected.constructor.name} content mismatch at index ${i}`);
    }
  }
}

// Returns a common set of tests for message serialization to/from V8.
export function getMessageSerializationTestCases(
    apiToTest, structuredCloneFeatureEnabled, tabId = -1) {
  // The API used for each test method (`chrome.runtime` or `chrome.tabs`)
  // depends on the background context.
  const testTabsAPI = apiToTest === 'tabs';
  const messagingAPI = testTabsAPI ? chrome.tabs : chrome.runtime;
  // Test the correct API depending on the API specified. The receiving end
  // (background or content script) will echo the message back.
  const sendMessage = (message) => {
    return testTabsAPI ? messagingAPI.sendMessage(tabId, message) :
                         messagingAPI.sendMessage(message);
  };
  const connect = () => {
    return testTabsAPI ? messagingAPI.connect(tabId, {name: 'test_port'}) :
                         messagingAPI.connect({name: 'test_port'});
  };
  const serializationTestCases = structuredCloneFeatureEnabled ?
      testCases.structuredClone :
      testCases.json;
  const objectTypeTestCases = structuredCloneFeatureEnabled ?
      testCases.structureCloneObjectType :
      testCases.jsonObjectType;
  const errorTypeTestCases = structuredCloneFeatureEnabled ?
      testCases.structureCloneErrorType :
      testCases.jsonErrorType;
  const unserializableErrorTestCases = structuredCloneFeatureEnabled ?
      testCases.structureCloneUnserializableError :
      testCases.jsonUnserializableError;


  return [

    // Tests to/from v8 message serialization for on-time messages by sending
    // various message types and receiving a response with that same message.
    // It's expected that the message sent should match what we get back for
    // serialization to succeed.
    async function sendMessageMessageSerialization() {
      let testsSucceeded = 0;
      for (const test of serializationTestCases) {
        let messagePromise = sendMessage(test.message);
        let response = await messagePromise;
        chrome.test.assertEq(test.expected, response, `Test: ${test.name}`);
        testsSucceeded++;
      }

      chrome.test.assertEq(
          serializationTestCases.length, testsSucceeded,
          'Didn\'t run the expected number of tests.');
      chrome.test.succeed();
    },

    // Tests to/from v8 message serialization for a sampling of various
    // objects that are structured clone, but not JSON serializable. These
    // objects also need manual verification since chrome.test.assertEq cannot
    // compare them accurately.
    //
    // Note: Some types are not available in all contexts (e.g. Service
    // Workers).
    // - `FileList`: Requires `DataTransfer` to construct, which is not
    // available in Service Workers. We skip this test if `DataTransfer` is
    // undefined.
    async function structuredCloneExpandedSerialization() {
      if (!structuredCloneFeatureEnabled) {
        // JSON serialization cannot handle these types well so we don't run
        // these tests for it.
        chrome.test.succeed();
        return;
      }

      // Test `NaN`.
      let nanMessagePromise = sendMessage(NaN);
      let nanResponse = await nanMessagePromise;
      chrome.test.assertTrue(
          Number.isNaN(nanResponse),
          `Equality check failed. Expected: NaN, Actual: ${
              JSON.stringify(nanResponse)}`);

      // RegExp.
      const regex = new RegExp('abc', 'i');
      const regexResponse = await sendMessage(regex);
      chrome.test.assertTrue(
          regexResponse instanceof RegExp, 'Expected RegExp');
      chrome.test.assertEq(regex.source, regexResponse.source);
      chrome.test.assertEq(regex.flags, regexResponse.flags);

      // ArrayBuffer (using Uint8Array is just a convenience to get an
      // ArrayBuffer).
      const buffer = new Uint8Array([1, 2, 3]).buffer;
      const bufferResponse = await sendMessage(buffer);
      assertArrayBufferEq(buffer, bufferResponse);

      // TypedArrays.
      const typedArray = new Int32Array([10, 20, -30]);
      const typedArrayResponse = await sendMessage(typedArray);
      assertTypedArrayEq(typedArray, typedArrayResponse);
      const bigInt64Array = new BigInt64Array([1n, 2n, -3n]);
      const bigInt64ArrayResponse = await sendMessage(bigInt64Array);
      assertTypedArrayEq(bigInt64Array, bigInt64ArrayResponse);

      // DataView.
      const dataViewBuffer = new ArrayBuffer(4);
      const dataView = new DataView(dataViewBuffer);
      // Write a 32-bit int using explicitly little-endian to prevent any
      // potential test flakiness.
      dataView.setInt32(0, 123456789, /*littleEndian=*/ true);
      const dataViewResponse = await sendMessage(dataView);
      chrome.test.assertTrue(
          dataViewResponse instanceof DataView, 'Expected DataView');
      chrome.test.assertEq(
          dataView.byteLength, dataViewResponse.byteLength,
          'DataView byteLength mismatch');
      chrome.test.assertEq(
          123456789, dataViewResponse.getInt32(0, /*littleEndian=*/ true),
          'DataView content mismatch');

      // Error-types
      const error = new Error('test error');
      const errorResponse = await sendMessage(error);
      chrome.test.assertTrue(errorResponse instanceof Error, 'Expected Error');
      chrome.test.assertEq(error.message, errorResponse.message);
      chrome.test.assertEq(error.name, errorResponse.name);
      const typeError = new TypeError('test type error');
      const typeErrorResponse = await sendMessage(typeError);
      chrome.test.assertTrue(
          typeErrorResponse instanceof TypeError, 'Expected TypeError');
      chrome.test.assertEq(typeError.message, typeErrorResponse.message);
      chrome.test.assertEq(typeError.name, typeErrorResponse.name);

      // DOMException.
      const domException =
          new DOMException('test dom exception', 'NotSupportedError');
      const domExceptionResponse = await sendMessage(domException);
      chrome.test.assertTrue(
          domExceptionResponse instanceof DOMException,
          'Expected DOMException');
      chrome.test.assertEq(domException.message, domExceptionResponse.message);
      chrome.test.assertEq(domException.name, domExceptionResponse.name);

      // File.
      const file = new File(
          ['content'], 'test.txt',
          {type: 'text/plain', lastModified: 1234567890});
      const fileResponse = await sendMessage(file);
      chrome.test.assertTrue(fileResponse instanceof File, 'Expected File');
      chrome.test.assertEq(file.name, fileResponse.name);
      chrome.test.assertEq(file.type, fileResponse.type);
      chrome.test.assertEq(file.size, fileResponse.size);
      chrome.test.assertEq(file.lastModified, fileResponse.lastModified);
      chrome.test.assertEq(await file.text(), await fileResponse.text());

      // FileList.
      if (typeof DataTransfer !== 'undefined') {
        const dt = new DataTransfer();
        const f1 = new File(['a'], 'a.txt', {type: 'text/plain'});
        const f2 = new File(['b'], 'b.txt', {type: 'text/plain'});
        dt.items.add(f1);
        dt.items.add(f2);
        const fileList = dt.files;
        const fileListResponse = await sendMessage(fileList);
        chrome.test.assertTrue(
            fileListResponse instanceof FileList, 'Expected FileList');
        chrome.test.assertEq(dt.files.length, fileListResponse.length);
        chrome.test.assertEq('a.txt', fileListResponse[0].name);
        chrome.test.assertEq('b.txt', fileListResponse[1].name);
      } else {
        console.log(
            '[INFO] Skipping FileList test: DataTransfer not defined in ' +
            'workers');
      }

      // ImageData.
      const imageData =
          new ImageData(new Uint8ClampedArray([255, 0, 0, 255]), 1, 1);
      const imageDataResponse = await sendMessage(imageData);
      chrome.test.assertTrue(
          imageDataResponse instanceof ImageData, 'Expected ImageData');
      chrome.test.assertEq(imageData.width, imageDataResponse.width);
      chrome.test.assertEq(imageData.height, imageDataResponse.height);
      assertTypedArrayEq(imageData.data, imageDataResponse.data);

      // DOMPoint.
      const point = new DOMPoint(1, 2, 3, 4);
      const pointResponse = await sendMessage(point);
      chrome.test.assertTrue(
          pointResponse instanceof DOMPoint, 'Expected DOMPoint');
      chrome.test.assertEq(point.x, pointResponse.x);
      chrome.test.assertEq(point.y, pointResponse.y);
      chrome.test.assertEq(point.z, pointResponse.z);
      chrome.test.assertEq(point.w, pointResponse.w);

      // DOMRect.
      const rect = new DOMRect(10, 20, 30, 40);
      const rectResponse = await sendMessage(rect);
      chrome.test.assertTrue(
          rectResponse instanceof DOMRect, 'Expected DOMRect');
      chrome.test.assertEq(rect.x, rectResponse.x);
      chrome.test.assertEq(rect.y, rectResponse.y);
      chrome.test.assertEq(rect.width, rectResponse.width);
      chrome.test.assertEq(rect.height, rectResponse.height);

      // DOMMatrix.
      const matrix = new DOMMatrix([1, 2, 3, 4, 5, 6]);
      const matrixResponse = await sendMessage(matrix);
      chrome.test.assertTrue(
          matrixResponse instanceof DOMMatrix, 'Expected DOMMatrix');
      chrome.test.assertEq(matrix.a, matrixResponse.a);
      chrome.test.assertEq(matrix.b, matrixResponse.b);
      chrome.test.assertEq(matrix.c, matrixResponse.c);
      chrome.test.assertEq(matrix.d, matrixResponse.d);
      chrome.test.assertEq(matrix.e, matrixResponse.e);
      chrome.test.assertEq(matrix.f, matrixResponse.f);

      // ImageBitmap.
      const ibImageData = new ImageData(1, 1);
      const imageBitmap = await createImageBitmap(ibImageData);
      const imageBitmapResponse = await sendMessage(imageBitmap);
      chrome.test.assertTrue(
          imageBitmapResponse instanceof ImageBitmap, 'Expected ImageBitmap');
      chrome.test.assertEq(imageBitmap.width, imageBitmapResponse.width);
      chrome.test.assertEq(imageBitmap.height, imageBitmapResponse.height);

      chrome.test.succeed();
    },

    // Tests to/from v8 message serialization for Blob scenarios.
    async function structuredCloneBlobs() {
      if (!structuredCloneFeatureEnabled) {
        // JSON serialization cannot handle Blobs well so we don't run
        // these tests for it.
        chrome.test.succeed();
        return;
      }

      // Plain text `Blob`
      let plainBlobMesssage = new Blob(['hello!'], {type: 'text/plain'});
      let plainBlobResponse = await sendMessage(plainBlobMesssage);
      chrome.test.assertTrue(plainBlobResponse instanceof Blob);
      const plainBlobResponseBuffer = await plainBlobResponse.arrayBuffer();
      assertArrayBufferEq(
          await plainBlobMesssage.arrayBuffer(), plainBlobResponseBuffer);

      // Blob from TypedArray
      const typedBlobData = new Uint8Array([1, 2, 3]);
      const typedBlobMessage = new Blob([typedBlobData]);
      const typedBlobResponse = await sendMessage(typedBlobMessage);
      chrome.test.assertTrue(typedBlobResponse instanceof Blob);
      const typedBlobResponseBuffer = await typedBlobResponse.arrayBuffer();
      assertArrayBufferEq(typedBlobData.buffer, typedBlobResponseBuffer);

      // Nested Blob
      const nestedBlob = {foo: new Blob(['bar'])};
      const nestedBlobResponse = await sendMessage(nestedBlob);
      chrome.test.assertTrue(nestedBlobResponse.foo instanceof Blob);
      chrome.test.assertEq('bar', await nestedBlobResponse.foo.text());

      // Array of Blobs
      const arrayBlobsMessage = [new Blob(['a']), new Blob(['b'])];
      const arrayBlobResponse = await sendMessage(arrayBlobsMessage);
      chrome.test.assertTrue(Array.isArray(arrayBlobResponse));
      chrome.test.assertEq('a', await arrayBlobResponse[0].text());
      chrome.test.assertEq('b', await arrayBlobResponse[1].text());

      chrome.test.succeed();
    },

    // Tests to/from v8 message serialization for `SharedArrayBuffer`.
    // SAB serialization is currently unsupported in extension messaging
    // regardless of isolation status, so we do not expect the message to
    // serialize.
    async function structuredCloneSharedArrayBufferToNonIsolatedContext() {
      if (!structuredCloneFeatureEnabled) {
        chrome.test.succeed();
        return;
      }

      chrome.test.assertFalse(self.crossOriginIsolated);

      if (typeof SharedArrayBuffer === 'undefined') {
        // In a tab (Content Script), `SharedArrayBuffer` is not defined
        // so we do not test it.
        chrome.test.succeed();
        return;
      }

      const sab = new SharedArrayBuffer(16);
      const int32 = new Int32Array(sab);
      int32[0] = 42;

      const response = await sendMessage(sab);

      // In the service worker, `SharedArrayBuffer` is defined but fails to
      // successfully serialize.
      chrome.test.assertEq(
          null, response,
          'SharedArrayBuffer should fail serialization and return null');

      chrome.test.succeed();
    },

    // Tests to/from v8 message serialization for `object`-types  It's expected
    // that the message sent should match (by value) what we get back for
    // serialization to succeed.
    async function objectTypeMessageSerialization() {
      let testsSucceeded = 0;
      for (const test of objectTypeTestCases) {
        let messagePromise = sendMessage(test.message);
        let response = await messagePromise;
        // chrome.test.assertEq doesn't handle value comparisons of containers
        // so we examine the containers by value manually to determine equality.
        containerAssertEq(test.expected, response);
        testsSucceeded++;
      }

      chrome.test.assertEq(
          objectTypeTestCases.length, testsSucceeded,
          'Didn\'t run the expected number of tests.');
      chrome.test.succeed();
    },


    // Tests to/from v8 message serialization for `Error`-types.
    async function ErrorMessageSerialization() {
      let testsSucceeded = 0;
      for (const test of errorTypeTestCases) {
        let messagePromise = sendMessage(test.message);
        let response = await messagePromise;

        // `chrome.test.assertEq()` doesn't deeply compare `Error` objects.
        if (structuredCloneFeatureEnabled) {
          chrome.test.assertTrue(
              response instanceof Error,
              `Test ${test.name}: Expected Error, got ${typeof response}`);
          chrome.test.assertEq(
              test.expected.name, response.name,
              `Test ${test.name}: Error name mismatch`);
          chrome.test.assertEq(
              test.expected.message, response.message,
              `Test ${test.name}: Error message mismatch`);
        } else {
          chrome.test.assertEq(response, test.expected);
        }

        testsSucceeded++;
      }

      chrome.test.assertEq(
          errorTypeTestCases.length, testsSucceeded,
          'Didn\'t run the expected number of tests.');
      chrome.test.succeed();
    },

    // Tests to/from v8 message serialization for long-lived connections by
    // sending various message types and receiving a response with that same
    // message. It's expected that the message sent should match what we get
    // back for serialization to succeed.
    async function connectMessageSerialization() {
      let testsSucceeded = 0;
      let port = connect();
      port.onMessage.addListener((response) => {
        const test = serializationTestCases[testsSucceeded];
        chrome.test.assertEq(test.expected, response, `Test: ${test.name}`);
        testsSucceeded++;
        if (testsSucceeded === serializationTestCases.length) {
          port.disconnect();
          chrome.test.succeed();
        }
      });

      for (const test of serializationTestCases) {
        port.postMessage(test.message);
      }
    },

    // Tests to/from v8 message serialization for one-time messages where
    // serialization is expected to fail synchronously.
    async function sendMessageUnserializableError() {
      const expectedSerializationError =
          testTabsAPI ? unserializableTabsError : unserializableRuntimeError;
      let testsRun = 0;
      for (const test of unserializableErrorTestCases) {
        try {
          await sendMessage(test.message);
          chrome.test.fail(
              `Test: ${test.name} should have thrown an error but succeeded.`);
        } catch (e) {
          chrome.test.assertEq(expectedSerializationError, e.message);
        }
        testsRun++;
      }

      chrome.test.assertEq(
          unserializableErrorTestCases.length, testsRun,
          'Didn\'t run the expected number of tests.');
      chrome.test.succeed();
    },

    // Tests to/from v8 message serialization for long-lived connections where
    // serialization is expected to fail synchronously.
    async function connectUnserializableError() {
      let testsRun = 0;
      let port = connect();

      for (const test of unserializableErrorTestCases) {
        try {
          port.postMessage(test.message);
          chrome.test.fail(
              `Test: ${test.name} should have thrown an error but succeeded.`);
        } catch (e) {
          chrome.test.assertEq('Could not serialize message.', e.message);
          testsRun++;
        }
      }

      port.disconnect();
      chrome.test.assertEq(
          unserializableErrorTestCases.length, testsRun,
          'Didn\'t run the expected number of tests.');
      chrome.test.succeed();
    },

  ];
}
