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
const userGestures = requireNative('user_gestures');

const GetModuleSystem = requireNative('v8_context').GetModuleSystem;

function handleException(message, error) {
  bindingUtil.handleException(message || 'Unknown error', error);
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

/**
 * Checks if two values are deeply equal.
 * @param expected The expected value.
 * @param actual The actual value.
 * @return True if the values are equal.
 */
function checkEq(expected, actual) {
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
      bindingUtil.setExceptionHandler(function(message, e) {
        if (e !== kFailureException) {
          chromeTest.fail(`uncaught exception: ${message}`);
        }
      });
      const result = $Function.call(currentTest);
      if (result instanceof Promise) {
        result.catch(e => handleException(e.message, e));
      }
    } catch (e) {
      handleException(e.message, e);
    }
  }

  // Helper function to get around the fact that function names in javascript
  // are read-only, and you can't assign one to anonymous functions.
  function testName(test) {
    return test ? (test.name || test.generatedName) : '(no test)';
  }

  function testDone() {
    environmentSpecificBindings.testDone(runNextTest);
  }

  function allTestsDone() {
    if (testsFailed == 0) {
      chromeTest.notifyPass();
    } else {
      chromeTest.notifyFail(`Failed ${testsFailed} of ${testCount} tests`);
    }
  }

  // Helper function for boolean asserts. Compares |test| to |expected|.
  function assertBool(test, expected, message) {
    if (test !== expected) {
      if (typeof test == 'string') {
        if (message) {
          message = `${test}\n${message}`;
        } else {
          message = test;
        }
      }
      chromeTest.fail(message);
    }
  }

  apiFunctions.setHandleRequest('callbackAdded', function() {
    pendingCallbacks++;

    let called = null;
    return function() {
      if (called != null) {
        const redundantPrefixLength = 'Error\n'.length;
        chromeTest.fail(
          'Callback has already been run. ' +
          'First call:\n' +
          $String.slice(called, redundantPrefixLength) + '\n' +
          'Second call:\n' +
          $String.slice(new Error().stack, redundantPrefixLength));
      }
      called = new Error().stack;

      pendingCallbacks--;
      if (pendingCallbacks == 0) {
        chromeTest.succeed();
      }
    };
  });

  apiFunctions.setHandleRequest('fail', function failHandler(message) {
    chromeTest.log(`(  FAILED  ) ${testName(currentTest)}`);

    let stack = {};
    // NOTE(devlin): captureStackTrace() populates a stack property of the
    // passed-in object with the stack trace. The second parameter (failHandler)
    // represents a function to serve as a relative point, and is removed from
    // the trace (so that everything doesn't include failHandler in the trace
    // itself). This (and other APIs) are documented here:
    // https://github.com/v8/v8/wiki/Stack%20Trace%20API. If we wanted to be
    // really fancy, there may be more sophisticated ways of doing this.
    Error.captureStackTrace(stack, failHandler);

    if (!message) {
      message = 'FAIL (no message)';
    }

    message += '\n' + stack.stack;
    console.log(`[FAIL] ${testName(currentTest)}: ${message}`);
    testsFailed++;
    testDone();

    // Interrupt the rest of the test.
    throw kFailureException;
  });

  apiFunctions.setHandleRequest('succeed', function() {
    chromeTest.assertEq(
        0, pendingPromiseRejections,
        'Test had pending promise rejections. This is likely the result of ' +
        'not waiting for the promise returned by `assertPromiseRejects()` to ' +
        'resolve. Instead, use `await assertPromiseRejects(...)` or ' +
        '`assertPromiseRejects(...).then(...).`.');
    console.log(`[SUCCESS] ${testName(currentTest)}`);
    chromeTest.log('(  SUCCESS )');
    testDone();
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

  apiFunctions.setHandleRequest('checkDeepEq', function(expected, actual) {
    return checkEq(expected, actual);
  });

  apiFunctions.setHandleRequest(
      'assertEq', function(expected, actual, message) {
        if (chromeTest.checkDeepEq(expected, actual)) {
          return;
        }

        let errorMsg = 'API Test Error in ' + testName(currentTest);
        if (message) {
          errorMsg += ': ' + message;
        }

        if (typeof expected == 'object' || typeof actual == 'object') {
          errorMsg += '\nActual: ' + ($JSON.stringify(actual) || '' + actual) +
              '\nExpected: ' + ($JSON.stringify(expected) || '' + expected);
        } else {
          errorMsg += `\nActual: ${actual}\nExpected: ${expected}`;
          if (typeof expected != typeof actual) {
            errorMsg += ` (type mismatch)\nActual Type: ${
                typeof actual}\nExpected Type:${typeof expected}`;
          }
        }
        chromeTest.fail(errorMsg);
      });

  apiFunctions.setHandleRequest(
      'assertNe', function(expected, actual, message) {
        if (!chromeTest.checkDeepEq(expected, actual)) {
          return;
        }

        let errorMsg = 'API Test Error in ' + testName(currentTest);
        if (message) {
          errorMsg += ': ' + message;
        }

        errorMsg += '\nExpected unequal values, but both are ' +
            $JSON.stringify(expected);
        chromeTest.fail(errorMsg);
      });

  apiFunctions.setHandleRequest('assertNoLastError', function() {
    if (chrome.runtime.lastError != undefined) {
      chromeTest.fail('lastError.message == ' +
                       chrome.runtime.lastError.message);
    }
  });

  apiFunctions.setHandleRequest('assertLastError', function(expectedError) {
    chromeTest.assertEq(typeof expectedError, 'string');
    chromeTest.assertTrue(
        chrome.runtime.lastError != undefined,
        `No lastError, but expected ${expectedError}`);
    chromeTest.assertEq(expectedError, chrome.runtime.lastError.message);
  });

  apiFunctions.setHandleRequest('assertThrows',
                                function(fn, self, args, message) {
    chromeTest.assertTrue(typeof fn == 'function');
    try {
      fn.apply(self, args);
      chromeTest.fail('Did not throw error: ' + fn);
    } catch (e) {
      if (e != kFailureException && message !== undefined) {
        if (message instanceof RegExp) {
          chromeTest.assertTrue(message.test(e.message),
                                e.message + ' should match ' + message)
        } else {
          chromeTest.assertEq(message, e.message);
        }
      }
    }
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
          chromeTest.fail(
              'Promise did not reject. Expected error: ' + expectedMessage);
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
    chromeTest.tests = tests;
    testCount = chromeTest.tests.length;
    runNextTest();
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

  environmentSpecificBindings.registerHooks(api);
});
