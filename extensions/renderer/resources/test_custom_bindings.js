// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// test_custom_bindings.js
// mini-framework for ExtensionApiTest browser tests

const environmentSpecificBindings =
    require('test_environment_specific_bindings');
const GetExtensionAPIDefinitionsForTest =
    requireNative('apiDefinitions').GetExtensionAPIDefinitionsForTest;
const GetAPIFeatures = requireNative('test_features').GetAPIFeatures;
const GetUseStandardizedApiBehavior =
    requireNative('test_api_standardized_behavior')
        .GetUseStandardizedApiBehavior;
const userGestures = requireNative('user_gestures');
const logging = requireNative('logging');

const GetModuleSystem = requireNative('v8_context').GetModuleSystem;

// A flag to determine adapt testing behavior to comply with the W3C
// browser.test proposal
// (github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md).
const useStandardizedApiBehavior = GetUseStandardizedApiBehavior();

function handleException(message, error) {
  bindingUtil.handleException(message || 'Unknown error', error);
}

// The quotes in the function are from the W3C browser.test.assertEq proposal
// (github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md).
function isPlainObject(obj) {
  // "2. Plain objects (with prototype Object.prototype or null)""
  if (typeof obj !== 'object' || obj === null) {
    return false;
  }
  let proto = $Object.getPrototypeOf(obj);
  // test_custom_bindings.js is only used in a testing context, so we don't
  // need to worry about malicious or buggy extension scripts clobbering the
  // global Object. Therefore, it's safe to use the strict `Object.prototype`
  // equality check.
  return proto === null || proto === Object.prototype;
}

// The quotes in the function are from the W3C browser.test.assertEq proposal
// (github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md).
function checkPlainObject(expected, actual) {
  if (!isPlainObject(actual)) {
    return false;
  }

  // "2.1) Only own enumerable properties are considered, unordered."
  let expectedProps = Object.keys(expected);
  let actualProps = Object.keys(actual);

  // "2) Plain objects ... are also compared recursively."
  if (!checkEq(expectedProps, actualProps)) {
    return false;
  }

  for (let prop of expectedProps) {
    if (!checkEq(expected[prop], actual[prop])) {
      return false;
    }
  }

  return true;
}

// The quotes in the function are from the W3C browser.test.assertEq proposal
// (github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md).
function checkArray(expected, actual) {
  // "1) Plain arrays (Array.isArray() returns `true`) are compared for deep
  // equality."
  if (!$Array.isArray(actual)) {
    return false;
  }

  // "1.1) Their length and all items are recursively compared by these same
  // rules."
  if (expected.length !== actual.length) {
    return false;
  }
  for (let i = 0; i < expected.length; ++i) {
    if (!checkEq(expected[i], actual[i])) {
      return false;
    }
  }
  return true;
}

// The quotes in the function are from the W3C browser.test.assertEq proposal
// (github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md).
function checkEqStandardized(expected, actual) {
  if ($Array.isArray(expected)) {
    return checkArray(expected, actual);
  }

  // "2) Plain objects (with prototype Object.prototype or null) are also
  // compared recursively."
  if (isPlainObject(expected)) {
    return checkPlainObject(expected, actual);
  }

  // "3) Everything else is compared using Object.is() same value algorithm"
  return Object.is(expected, actual);
}

/**
 * Checks if two collections (Maps or Sets) are deeply equal, ignoring order.
 * @param expected The expected collection.
 * @param actual The actual collection.
 * @param isMap True if the collections are Maps, false if Sets.
 * @return True if the collections are equal.
 */
function checkCollectionEq(expected, actual, isMap) {
  if (expected.size !== actual.size) {
    return false;
  }

  const actualItems = Array.from(isMap ? actual.entries() : actual.values());
  const matchedIndices = new Set();

  const expectedIter = isMap ? expected.entries() : expected.values();
  for (const expItem of expectedIter) {
    let found = false;
    for (let i = 0; i < actualItems.length; ++i) {
      if (matchedIndices.has(i)) {
        continue;
      }
      const actItem = actualItems[i];
      let eq = false;
      if (isMap) {
        // For Maps, expItem and actItem are [key, value] pairs.
        eq = checkEq(expItem[0], actItem[0]) && checkEq(expItem[1], actItem[1]);
      } else {
        // For Sets, they are values.
        eq = checkEq(expItem, actItem);
      }

      if (eq) {
        matchedIndices.add(i);
        found = true;
        break;
      }
    }
    if (!found) {
      return false;
    }
  }
  return true;
}

function checkEqNonStandard(expected, actual) {
  if ((expected === null) != (actual === null)) {
    return false;
  }

  // Check for strict equality (handles primitives and same object
  // references).
  if (expected === actual) {
    return true;
  }

  if (Number.isNaN(expected) && Number.isNaN(actual)) {
    return true;
  }

  if (typeof expected !== typeof actual) {
    return false;
  }

  // If they are not objects, and not functions, and not strictly equal
  // (checked above), they are unequal primitives.
  if (typeof expected !== 'object' && typeof expected !== 'function') {
    return false;
  }

  if (typeof expected === 'function') {
    return expected.toString() === actual.toString();
  }

  // Initial checks for Arrays.
  if ($Array.isArray(expected) !== $Array.isArray(actual)) {
    return false;
  }
  if ($Array.isArray(expected) && expected.length !== actual.length) {
    return false;
  }

  // Handle the ArrayBuffer cases. Bail out in case of type mismatch, to
  // prevent the ArrayBuffer from being treated as an empty enumerable below.
  if ((actual instanceof ArrayBuffer) !== (expected instanceof ArrayBuffer)) {
    return false;
  }
  if ((actual instanceof ArrayBuffer) && (expected instanceof ArrayBuffer)) {
    if (actual.byteLength != expected.byteLength) {
      return false;
    }
    let actualView = new Uint8Array(actual);
    let expectedView = new Uint8Array(expected);
    for (let i = 0; i < actualView.length; ++i) {
      if (actualView[i] != expectedView[i]) {
        return false;
      }
    }
    return true;
  }

  // Check Date objects.
  if (expected instanceof Date) {
    if (!(actual instanceof Date)) {
      return false;
    }
    return expected.getTime() === actual.getTime() ||
        (Number.isNaN(expected.getTime()) && Number.isNaN(actual.getTime()));
  }

  // Check primitive wrappers.
  if ((expected instanceof String) || (expected instanceof Number) ||
      (expected instanceof Boolean)) {
    if (Object.getPrototypeOf(expected) !== Object.getPrototypeOf(actual)) {
      return false;
    }
    const expVal = expected.valueOf();
    const actVal = actual.valueOf();
    if (Number.isNaN(expVal) && Number.isNaN(actVal)) {
      return true;
    }
    return expVal === actVal;
  }

  // Check Maps.
  if (expected instanceof Map) {
    if (!(actual instanceof Map)) {
      return false;
    }
    return checkCollectionEq(expected, actual, /* isMap= */ true);
  }

  // Check Sets.
  if (expected instanceof Set) {
    if (!(actual instanceof Set)) {
      return false;
    }
    return checkCollectionEq(expected, actual, /* isMap= */ false);
  }

  // Standard Object / Array property checking.
  for (let p in actual) {
    if ($Object.hasOwnProperty(actual, p) &&
        !$Object.hasOwnProperty(expected, p)) {
      return false;
    }
  }
  for (let p in expected) {
    if ($Object.hasOwnProperty(expected, p) &&
        !$Object.hasOwnProperty(actual, p)) {
      return false;
    }
  }

  for (let p in expected) {
    if (!checkEq(expected[p], actual[p])) {
      return false;
    }
  }

  return true;
}

/**
 * Checks if two values are deeply equal.
 * @param expected The expected value.
 * @param actual The actual value.
 * @return True if the values are equal.
 */
function checkEq(expected, actual) {
  // TODO(crbug.com/493946949): merge these two impls together once more types
  // are supported.
  if (useStandardizedApiBehavior) {
    return checkEqStandardized(expected, actual);
  }
  return checkEqNonStandard(expected, actual);
}

apiBridge.registerCustomHook(function(api) {
  const kFailureException = 'chrome.test.failure';

  const chromeTest = api.compiledApi;
  const apiFunctions = api.apiFunctions;

  chromeTest.tests = chromeTest.tests || [];

  let currentTest = null;
  let lastTest = null;
  let testsFailed = 0;
  let testCount = 1;
  let pendingCallbacks = 0;
  let pendingPromiseRejections = 0;
  let runTestsResolve = null;
  let runTestsReject = null;
  // Stores the original, framework-defined `chrome.test.fail` function.
  // This is used to detect if a test has mocked `chrome.test.fail` (e.g. to
  // capture assertion failures without terminating the test). An example of
  // this is the `TestAPITest.ApiTest` test case which mocks `chrome.test.fail`
  // to verify `assertEq` failure messages.
  // TODO(crbug.com/519961697): Consider if we should adjust
  // `TestAPITest.ApiTest` to not require mocking to do its assertions.
  let originalChromeTestFail = null;

  function failInternal(message, failRelativePoint) {
    // If the test has overridden `chrome.test.fail`, delegate to the mock
    // so that the test's mock logic can capture the failure and continue
    // execution.
    if (originalChromeTestFail && chromeTest.fail !== originalChromeTestFail) {
      chromeTest.fail(message);
      return;
    }
    testsFailed++;
    chromeTest.log(`(  FAILED  ) ${testName(currentTest)}`);

    let stack = {};
    Error.captureStackTrace(stack, failRelativePoint || failInternal);

    const assertionDescription = String(message || 'Assertion FAIL');
    const fullMessage = `${assertionDescription} \n ${stack.stack}`;

    console.log(`[FAIL] ${testName(currentTest)}: ${fullMessage}`);
    if (chromeTest.onTestFinished) {
      chromeTest.onTestFinished.dispatch({
        testName: testName(currentTest),
        result: false,
        remainingTests: chromeTest.tests.length,
        assertionDescription: assertionDescription,
        message: fullMessage
      });
    }
    chromeTest.notifyTestFinished(
        testName(currentTest), /* result= */ false,
        /* remainingTests= */ chromeTest.tests.length, assertionDescription,
        /* message= */ fullMessage);
    testDone();
    throw kFailureException;
  }

  function succeedInternal() {
    chromeTest.assertEq(
        0, pendingPromiseRejections,
        'Test had pending promise rejections. This is likely the result of \
not waiting for the promise returned by `assertPromiseRejects()` to \
resolve. Instead, use `await assertPromiseRejects(...)` or \
`assertPromiseRejects(...).then(...).`.');
    console.log(`[SUCCESS] ${testName(currentTest)}`);
    chromeTest.log('(  SUCCESS )');
    if (chromeTest.onTestFinished) {
      chromeTest.onTestFinished.dispatch({
        testName: testName(currentTest),
        result: true,
        remainingTests: chromeTest.tests.length,
        assertionDescription: `${testName(currentTest)} PASS`
      });
    }
    chromeTest.notifyTestFinished(
        testName(currentTest), /* result= */ true,
        /* remainingTests= */ chromeTest.tests.length,
        /* assertionDescription= */ `${testName(currentTest)} PASS`,
        /* message= */ '');
    testDone();
  }

  function safeFunctionApply(func, args) {
    try {
      if (func) {
        return $Function.apply(func, undefined, args);
      }
    } catch (e) {
      if (e === kFailureException) {
        throw e;
      }
      handleException(e.message, e);
    }
  }

  function runNextTest() {
    // There may have been callbacks or promise rejections which were
    // interrupted by failure exceptions.
    pendingCallbacks = 0;
    pendingPromiseRejections = 0;

    lastTest = currentTest;
    currentTest = $Array.shift(chromeTest.tests);

    if (!currentTest) {
      allTestsDone();
      return;
    }

    try {
      chromeTest.log(`( RUN      ) ${testName(currentTest)}`);
      // Notify this script context (if it's listening) that the test started.
      if (chromeTest.onTestStarted) {
        chromeTest.onTestStarted.dispatch({testName: testName(currentTest)});
      }
      // Notify other renderer script contexts that the test started.
      chromeTest.notifyTestStarted(testName(currentTest));
      bindingUtil.setExceptionHandler(function(message, e) {
        if (e !== kFailureException) {
          failInternal(
              `Exception running ${testName(currentTest)}: ${message}`);
        }
      });
      const result = $Function.call(currentTest);

      if (useStandardizedApiBehavior) {
        handleImplicitTestResult(result);
      } else {
        handleExplicitTestResult(result);
      }
    } catch (e) {
      handleException(e.message, e);
    }
  }

  /**
   * Handles the result of a test function when implicit test results are
   * enabled.
   *
   * As specified in the W3C WebExtensions browser.test proposal:
   * https://github.com/w3c/webextensions/blob/main/proposals/browser_test_api.md
   * "Tests pass when they either return `undefined` or when a promise returned
   * by the test resolves. They fail if they:
   * - Throw an exception
   * - Return a promise that rejects
   * - Trigger an assertion failure"
   *
   * @param {*} result The return value of the test function.
   */
  function handleImplicitTestResult(result) {
    if (result instanceof Promise) {
      result
          .then(() => {
            succeedInternal();
          })
          .catch(e => handleException(e.message, e));
      return;
    }

    if (result !== undefined) {
      failInternal(
          'Test functions with implicit passing enabled must return ' +
          'undefined or a Promise.');
      return;
    }

    if (pendingCallbacks > 0) {
      failInternal(
          'Test returned undefined but has pending callbacks. ' +
          'Did you forget to return a Promise?');
      return;
    }

    succeedInternal();
  }

  /**
   * Handles the result of a test function when implicit test results is
   * disabled.
   *
   * This handles test failures for `async` test functions that throw errors.
   * The catch here ensures that we fail the test when this happens.
   *
   * @param {*} result The return value of the test function.
   */
  function handleExplicitTestResult(result) {
    if (result instanceof Promise) {
      result.catch(e => handleException(e.message, e));
    }
  }

  // Helper function to get around the fact that function names in javascript
  // are read-only, and you can't assign one to anonymous functions.
  function testName(test) {
    return (test && (test.name || test.generatedName)) || '(no test)';
  }

  function testDone() {
    environmentSpecificBindings.testDone(runNextTest);
  }

  function allTestsDone() {
    const resolve = runTestsResolve;
    const reject = runTestsReject;
    const errorMessage =
        testsFailed > 0 ? `Failed ${testsFailed} of ${testCount} tests` : null;

    // Reset all shared global state. This allows for subsequent runs of
    // chrome.test.runTests().
    currentTest = null;
    lastTest = null;
    testsFailed = 0;
    testCount = 1;
    pendingCallbacks = 0;
    pendingPromiseRejections = 0;
    runTestsResolve = null;
    runTestsReject = null;
    chromeTest.tests = [];

    if (!errorMessage) {
      chromeTest.notifyPass();
      if (resolve) {
        resolve();
      }
    } else {
      chromeTest.notifyFail(errorMessage);
      if (reject) {
        reject(new Error(errorMessage));
      }
    }
  }

  // Helper function for boolean asserts. Compares |test| to |expected|.
  function assertBool(test, expected, message) {
    logging.CHECK(typeof expected === 'boolean');
    if (typeof test !== 'boolean') {
      failInternal(
          `API Test Error in ${testName(currentTest)}: ` +
          'assertTrue and assertFalse require a boolean condition.');
    }
    if (test !== expected) {
      failInternal(message);
    }
  }

  apiFunctions.setHandleRequest('callbackAdded', function() {
    pendingCallbacks++;

    let previousCallStack = null;
    return function() {
      if (previousCallStack != null) {
        const prefixLength = 'Error\n'.length;
        const currentStack = new Error().stack;

        const trimmedPreviousStack =
            $String.slice(previousCallStack, prefixLength);
        const trimmedCurrentStack = $String.slice(currentStack, prefixLength);

        failInternal(
            'Callback has already been run. ' +
            `First call:\n${trimmedPreviousStack}\n` +
            `Second call:\n${trimmedCurrentStack}`);
        return;
      }
      previousCallStack = new Error().stack;

      pendingCallbacks--;
      if (pendingCallbacks == 0) {
        succeedInternal();
      }
    };
  });

  apiFunctions.setHandleRequest('fail', function failHandler(message) {
    if (useStandardizedApiBehavior) {
      let failureMessage =
          'chrome.test.fail() is not allowed when implicit test results are ' +
          'enabled. Use assertions, throw exceptions, or return a ' +
          'promise that rejects instead.';
      if (message) {
        failureMessage += ` Original failure message: ${message}`;
      }
      failInternal(failureMessage, failHandler);
    } else {
      failInternal(message, failHandler);
    }
  });

  apiFunctions.setHandleRequest('succeed', function() {
    if (useStandardizedApiBehavior) {
      failInternal(
          'chrome.test.succeed() is not allowed when implicit test results ' +
          'are enabled. Either return a promise that resolves or return ' +
          'undefined to indicate test passing.');
    } else {
      succeedInternal();
    }
  });

  apiFunctions.setHandleRequest('getModuleSystem', function(context) {
    return GetModuleSystem(context);
  });

  apiFunctions.setHandleRequest('assertTrue', function(test, message) {
    assertBool(test, true, message);
  });

  apiFunctions.setHandleRequest('assertFalse', function(test, message) {
    assertBool(test, false, message);
  });

  apiFunctions.setHandleRequest('checkDeepEq', function(value, other_value) {
    return checkEq(value, other_value);
  });

  apiFunctions.setHandleRequest(
      'assertEq', function(value, other_value, message) {
        if (chromeTest.checkDeepEq(value, other_value)) {
          return;
        }

        let errorMsg = 'API Test Error in ' + testName(currentTest);
        if (message) {
          errorMsg += ': ' + message;
        }

        if (typeof value == 'object' || typeof other_value == 'object') {
          errorMsg += '\n' + ($JSON.stringify(value) || '' + value) +
              ' did not deep-equal ' +
              ($JSON.stringify(other_value) || '' + other_value);
        } else {
          errorMsg += `\n${value} did not deep-equal ${other_value}`;
          if (typeof value != typeof other_value) {
            errorMsg += ` (type mismatch)\nType: ${typeof value}\nOther Type:${
                typeof other_value}`;
          }
        }
        failInternal(errorMsg);
      });

  apiFunctions.setHandleRequest(
      'assertNe', function(value, other_value, message) {
        if (!chromeTest.checkDeepEq(value, other_value)) {
          return;
        }

        let errorMsg = 'API Test Error in ' + testName(currentTest);
        if (message) {
          errorMsg += ': ' + message;
        }

        errorMsg +=
            '\nExpected unequal values, but both are ' + $JSON.stringify(value);
        failInternal(errorMsg);
      });

  apiFunctions.setHandleRequest('assertNoLastError', function() {
    if (chrome.runtime.lastError != undefined) {
      failInternal(`lastError.message == ${chrome.runtime.lastError.message}`);
    }
  });

  apiFunctions.setHandleRequest('assertLastError', function(expectedError) {
    chromeTest.assertEq(typeof expectedError, 'string');
    chromeTest.assertTrue(
        chrome.runtime.lastError != undefined,
        `No lastError, but expected ${expectedError}`);
    chromeTest.assertEq(expectedError, chrome.runtime.lastError.message);
  });

  /**
   * Helper function to check if the thrown error matches the expected error.
   *
   * @param {Error} e The thrown error object.
   * @param {string|RegExp|null|undefined} expectedError The expected error. If
   *     it's a string, it checks for exact match with the error message. If
   *     it's a RegExp, it tests the error message. If it's null or undefined,
   *     no check is performed.
   * @param {string=} customMessage An optional custom message to prepend to
   *     the failure message.
   */
  function checkThrownError(e, expectedError, customMessage) {
    if (e === kFailureException) {
      return;
    }
    if (expectedError === undefined || expectedError === null) {
      return;
    }

    const actualMessage =
        (e && typeof e === 'object' && 'message' in e) ? e.message : String(e);

    if (expectedError instanceof RegExp) {
      let failMessage = `${actualMessage} should match ${expectedError}`;
      if (customMessage) {
        failMessage = `${customMessage}\n${failMessage}`;
      }
      chromeTest.assertTrue(expectedError.test(actualMessage), failMessage);
      return;
    }

    if (actualMessage !== expectedError) {
      let baseMessage =
          `Expected error: "${expectedError}", actual: "${actualMessage}"`;
      let failMessage =
          customMessage ? `${customMessage}\n${baseMessage}` : baseMessage;
      failInternal(failMessage);
    }
  }

  apiFunctions.setHandleRequest(
      'assertThrows', function(fn, expectedError, message) {
        chromeTest.assertTrue(typeof fn == 'function');
        let thrownError;
        let threw = false;
        try {
          fn();
        } catch (e) {
          thrownError = e;
          threw = true;
        }

        // We need both `threw` and `thrownError` because JavaScript allows
        // throwing any value, including falsy ones like `undefined` or `null`.
        // If we only checked `thrownError`, throwing a falsy value would be
        // indistinguishable from not throwing at all.
        if (!threw) {
          let failMessage = `Did not throw error: ${fn}`;
          if (message) {
            failMessage = `${message}\n${failMessage}`;
          }
          failInternal(failMessage);
          return;
        }

        checkThrownError(thrownError, expectedError, message);
      });

  apiFunctions.setHandleRequest('loadScript', function(scriptUrl) {
    // Note: Importing scripts is different depending on if this script is
    // executing in a Service Worker context.
    const inServiceWorker = 'ServiceWorkerGlobalScope' in self;

    function createError(exception) {
      const errorStr = `Unable to load script: "${scriptUrl}"`;
      if (inServiceWorker) {
        return new Error(errorStr, {cause: exception});
      }
      return new Error(errorStr);
    }

    if (inServiceWorker) {
      try {
        importScripts(scriptUrl);
      } catch (e) {
        return Promise.reject(createError(e));
      }
      return Promise.resolve();
    }
    const script = document.createElement('script');
    const onScriptLoad = new Promise((resolve, reject) => {
      script.onload = resolve;
      function onError() {
        reject(createError());
      }
      script.onerror = onError;
    });
    script.src = scriptUrl;
    document.body.appendChild(script);
    return onScriptLoad;
  });

  apiFunctions.setHandleRequest('assertPromiseRejects',
                                function(promise, expectedMessage) {
    pendingPromiseRejections++;
    return promise.then(
        () => {
          pendingPromiseRejections--;
          chromeTest.assertTrue(pendingPromiseRejections >= 0,
                                'Negative pending promise rejection count!');
          failInternal(
              `Promise did not reject. Expected error: ${expectedMessage}`);
        },
        (e) => {
          pendingPromiseRejections--;
          chromeTest.assertTrue(pendingPromiseRejections >= 0,
                                'Negative pending promise rejection count!');
          if (expectedMessage instanceof RegExp) {
            chromeTest.assertTrue(
                expectedMessage.test(e.toString()),
                `'${e.message}' should match '${expectedMessage}'`);
          } else {
            chromeTest.assertEq('string', typeof expectedMessage);
            chromeTest.assertEq(expectedMessage, e.toString());
          }
        });
  });

  // Wrapper for generating test functions, that takes care of calling
  // assertNoLastError() and (optionally) succeed() for you.
  apiFunctions.setHandleRequest('callback', function(func, expectedError) {
    if (func) {
      chromeTest.assertEq(typeof func, 'function');
    }
    const callbackCompleted = chromeTest.callbackAdded();

    return function() {
      if (expectedError == null) {
        chromeTest.assertNoLastError();
      } else {
        chromeTest.assertLastError(expectedError);
      }

      let result;
      if (func) {
        result = safeFunctionApply(func, arguments);
      }

      callbackCompleted();
      return result;
    };
  });

  apiFunctions.setHandleRequest('listenOnce', function(event, func) {
    if (func) {
      // Callback-based.
      const callbackCompleted = chromeTest.callbackAdded();
      const listener = function() {
        event.removeListener(listener);
        safeFunctionApply(func, arguments);
        callbackCompleted();
      };
      event.addListener(listener);
    } else {
      // Promise-based.
      return new Promise((resolve) => {
        const listener = function() {
          event.removeListener(listener);

          // Resolve the promise. As a clunky convenience, we resolve the
          // promise directly with the event argument if there's only one. If
          // there's more than one, we supply the arguments as an array.
          let args = Array.from(arguments);
          if (args.length == 1) {
            resolve(args[0]);
          } else {
            resolve(args);
          }
        };

        event.addListener(listener);
      });
    }
  });

  apiFunctions.setHandleRequest('listenForever', function(event, func) {
    const callbackCompleted = chromeTest.callbackAdded();

    const listener = function() {
      safeFunctionApply(func, arguments);
    };

    const done = function() {
      event.removeListener(listener);
      callbackCompleted();
    };

    event.addListener(listener);
    return done;
  });

  apiFunctions.setHandleRequest('callbackPass', function(func) {
    return chromeTest.callback(func);
  });

  apiFunctions.setHandleRequest('callbackFail', function(expectedError, func) {
    return chromeTest.callback(func, expectedError);
  });

  apiFunctions.setHandleRequest('runTests', function(tests) {
    if (runTestsResolve !== null) {
      throw new Error('chrome.test.runTests is already running.');
    }
    chromeTest.tests = tests;
    testCount = chromeTest.tests.length;
    return new Promise((resolve, reject) => {
      runTestsResolve = resolve;
      runTestsReject = reject;
      runNextTest();
    });
  });

  apiFunctions.setHandleRequest('getApiDefinitions', function() {
    return GetExtensionAPIDefinitionsForTest();
  });

  apiFunctions.setHandleRequest('getApiFeatures', function() {
    return GetAPIFeatures();
  });

  apiFunctions.setHandleRequest('isProcessingUserGesture', function() {
    return userGestures.IsProcessingUserGesture();
  });

  apiFunctions.setHandleRequest('runWithUserGesture', function(callback) {
    chromeTest.assertEq(typeof(callback), 'function');
    return userGestures.RunWithUserGesture(callback);
  });

  apiFunctions.setHandleRequest('setExceptionHandler', function(callback) {
    chromeTest.assertEq(typeof(callback), 'function');
    bindingUtil.setExceptionHandler(callback);
  });

  // Store the original wrapper for mock detection.
  originalChromeTestFail = chromeTest.fail;

  environmentSpecificBindings.registerHooks(api);
});
