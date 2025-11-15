// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const unserializableRuntimeError =
    'Error in invocation of runtime.sendMessage(optional string extensionId, ' +
    'any message, optional object options, optional function callback): ' +
    'Could not serialize message.';
const unserializableTabsError =
    'Error in invocation of tabs.sendMessage(integer tabId, any message, ' +
    'optional object options, optional function callback): Could not ' +
    'serialize message.';

const testCases = [
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

const unserializableTestCases = [
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


// Returns a common set of tests for message serialization to/from V8.
export function getMessageSerializationTestCases(apiToTest, tabId = -1) {
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

  return [

    /**
     * Tests to/from v8 message serialization for on-time messages by sending
     * various message types and receiving a response with that same message.
     * It's expected that the message sent should match what we get back for
     * serialization to succeed.
     */
    async function sendMessageMessageSerialization() {
      let testsSucceeded = 0;
      for (const test of testCases) {
        let messagePromise = sendMessage(test.message);
        let response = await messagePromise;
        chrome.test.assertEq(test.expected, response, `Test: ${test.name}`);
        testsSucceeded++;
      }

      chrome.test.assertEq(
          testCases.length, testsSucceeded,
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
        const test = testCases[testsSucceeded];
        chrome.test.assertEq(test.expected, response, `Test: ${test.name}`);
        testsSucceeded++;
        if (testsSucceeded === testCases.length) {
          port.disconnect();
          chrome.test.succeed();
        }
      });

      for (const test of testCases) {
        port.postMessage(test.message);
      }
    },

    /**
     * Tests to/from v8 message serialization for one-time messages in known
     * failure scenarios. It's expected that either the message send will throw
     * an error due to failure to serialize, or the response received will not
     * match the message sent.
     */
    async function sendMessageMessageUnserializable() {
      const expectedSerializationError =
          testTabsAPI ? unserializableTabsError : unserializableRuntimeError;
      let testsRun = 0;
      for (const test of unserializableTestCases) {
        try {
          let messagePromise = sendMessage(test.message);
          let response = await messagePromise;
          // If the message is able to be sent, it won't return the same.
          chrome.test.assertTrue('expected' in test);
          chrome.test.assertNe(test.expected, response, `Test: ${test.name}`);
        } catch (e) {
          // If the message send fails it should be the same error.
          chrome.test.assertEq(expectedSerializationError, e.message);
        }
        testsRun++;
      }

      chrome.test.assertEq(
          unserializableTestCases.length, testsRun,
          'Didn\'t run the expected number of tests.');
      chrome.test.succeed();
    },

    /**
     * Tests to/from v8 message serialization for long-lived connections in
     * known failure scenarios. It's expected that either the message send will
     * throw an error due to failure to serialize, or the response received will
     * not match the message sent.
     */
    async function connectMessageUnserializable() {
      let testsRun = 0;
      let port = connect();
      // If not all the test cases threw an error synchronously the response
      // listener will check the response for the expected behavior.
      port.onMessage.addListener((response) => {
        // If the message is able to be sent, it won't return the same.
        const test = unserializableTestCases[testsRun];
        chrome.test.assertTrue('expected' in test);
        chrome.test.assertNe(test.expected, response, `Test: ${test.name}`);
        testsRun++;
        if (testsRun === unserializableTestCases.length) {
          port.disconnect();
          chrome.test.succeed();
        }
      });

      for (const test of unserializableTestCases) {
        try {
          port.postMessage(test.message);
        } catch (e) {
          // If the message send fails it should be the same error.
          chrome.test.assertEq('Could not serialize message.', e.message);
          testsRun++;
        }
      }

      // If all test cases threw an error synchronously, the test is complete.
      // Otherwise we'll wait for the responses to return and the listener above
      // to check them.
      if (testsRun === unserializableTestCases.length) {
        port.disconnect();
        chrome.test.succeed();
      }
    },

  ];
}
