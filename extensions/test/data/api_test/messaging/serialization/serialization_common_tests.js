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

/**
 * Asserts that two containers are equal by value (except `Map`s with objects
   as keys: `new Map([['key', {'a': 1}]])`).
  TODO(crbug.com/40321352): Swap with the enhanced `chrome.test.assertEq()` when
  crbug.com/466303357 is resolved.
 */
function containerAssertEq(expected, actual) {
  chrome.test.assertTrue(
      containerCheckDeepEq(actual, expected),
      `Deep equality check failed. Expected: ${
          JSON.stringify(expected)}, Actual: ${JSON.stringify(actual)}`);
}

// Returns a common set of tests for message serialization to/from V8.
export function getMessageSerializationTestCases(
    apiToTest, structuredCloneFeatureEnabled, tabId = -1) {
  // The API used for each test method (`chrome.runtime` or `chrome.tabs`)
  // depends on the background context.
  const testTabsAPI = apiToTest === 'tabs';
  const messagingAPI = testTabsAPI ? chrome.tabs : chrome.runtime;
  // Test the correct API depending on the API specified.
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
  const unserializableErrorTestCases = structuredCloneFeatureEnabled ?
      testCases.structureCloneUnserializableError :
      testCases.jsonUnserializableError;


  return [

    /**
     * Tests to/from v8 message serialization for on-time messages by sending
     * various message types and receiving a response with that same message.
     * It's expected that the message sent should match what we get back for
     * serialization to succeed.
     */
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

    /**
     * Tests to/from v8 message serialization for a few test cases that require
     * special equality logic to do assertions. It's expected that the message
     * sent should match (by value) what we get back for serialization to
     * succeed.
     */
    async function structuredCloneEdgeCaseSerialization() {
      if (!structuredCloneFeatureEnabled) {
        chrome.test.succeed();
        return;
      }

      // Test `NaN`.
      let nanMessagePromise = sendMessage(NaN);
      let nanResponse = await nanMessagePromise;
      if (!Number.isNaN(nanResponse)) {
        chrome.test.fail(`Equality check failed. Expected: NaN, Actual: ${
            JSON.stringify(nanResponse)}`);
      }

      // Test `Blob`.
      let blobMessage = new Blob(['hello!'], {type: 'text/plain'});
      let blobMessagePromise = sendMessage(blobMessage);
      let blobResponse = await blobMessagePromise;
      // TODO(crbug.com/40321352): Once the `extensions::Message` class supports
      // `blink::mojom::CloneableMessage` for message transfer then we can
      // assert the below. Until then the response will default to `null` since
      // the wire data doesn't have the internal `Blob` metadata.
      chrome.test.assertEq(null, blobResponse, 'Test: Blob');
      // chrome.test.assertEq(
      //     blobMessage.arrayBuffer(), blobResponse.arrayBuffer(), 'Test:
      //     Blob');

      chrome.test.succeed();
    },

    /**
     * Tests to/from v8 message serialization for `object`-types  It's expected
     * that the message sent should match (by value) what we get back for
     * serialization to succeed.
     */
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

    /**
     * Tests to/from v8 message serialization for long-lived connections by
     * sending various message types and receiving a response with that same
     * message. It's expected that the message sent should match what we get
     * back for serialization to succeed.
     */
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

    /**
     * Tests to/from v8 message serialization for one-time messages where
     * serialization is expected to fail synchronously.
     */
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

    /**
     * Tests to/from v8 message serialization for long-lived connections where
     * serialization is expected to fail synchronously.
     */
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
