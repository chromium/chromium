# Using the chrome.test API

[TOC]

## What is It?
The `chrome.test` extension API is a limited testing framework implemented as
an extension API.  It is primarily used in order to provide testing
functionality used in writing extension API tests by exercising an API directly
in JS.  See also [writing extension tests].

### Basic JS-Based Tests
All tests must have some limited C++ portion (in order to kick off and drive
the test).  In the most basic form, this C++ test only needs to load the
extension and wait for the response from the testing framework.  This is easily
accomplished through either `ExtensionApiTest::RunExtensionTest()` or
`ExtensionApiTest::RunExtensionSubtest()` (and their variants).

`ExtensionApiTest::RunExtensionTest()` loads a test extension and then waits
for the result from the testing framework.
`ExtensionApiTest::RunExtensionSubtest()` loads a test extension, navigates to
a subpage of that extension, and then waits for the result from the testing
framework.

### Advanced Tests
More advanced tests may require more synchronization between the C++ and the JS
(e.g., to set up or verify state on the C++ side before continuing on the JS
side).  To do this, leverage either `extensions::ResultCatcher`, which waits
for the next result from the testing framework (sent using either
`chrome.test.notifyPass()`, `chrome.test.notifyFail()`, or waiting for the
result of `chrome.test.runTests()`, which calls `notifyPass()` or
`notifyFail()` automatically), or `extensions::ExtensionTestMessageListener`,
which waits for a message sent by `chrome.test.sendMessage()`.  See
[writing extension tests] for more information.

## Using the API
The API provides a variety of different methods, used for different purposes.

### notifyPass() and notifyFail()
`chrome.test.notifyPass()` and `chrome.test.notifyFail()` are used in order to
pass the result of running the JS test to the C++.  This is the result that
`RunExtensionTest()`, `RunExtensionSubtest()`, and the `ResultCatcher` wait on.
In order to explicitly pass this result, call `chrome.test.notifyPass()` or
`chrome.test.notifyFail()`.

```js
chrome.tabs.create(() => {
  <Verify state>
  chrome.test.notifyPass();
});
```

`notifyPass()` and `notifyFail()` may also be implicitly called by the testing
API, as described in the sections below.

### test.runTests()
`chrome.test.runTests()` is used to run a sequence of individual, smaller JS
tests, and then passes the result to the browser by **automatically** calling
`chrome.test.notifyPass()` or `chrome.test.notifyFail()`.  `notifyPass()` will
be called if and only if all individual tests pass; `notifyFail()` will be
called if any test fails.  A test may fail if an assertion fails, if there is
an unexpected runtime error, or if `chrome.test.fail()` is called explicitly.

`chrome.test.runTests()` takes an array of functions, and runs them serially.
This means that these functions may be independent, or may implicitly rely on
one another.  The output of running these individual tests is printed through
`console.log()`s, which enables tracing how far a test suite progresses.

Each individual test function passed to `runTests()` will execute, and then
wait for that specific function to pass or fail.  Passing is indicated by
calling `chrome.test.succeed()` within each test function (**not**
`chrome.test.notifyPass()`, which will automatically indicate the entire JS
test passes, and may mask failures - see also the [Do's And Don't's]. Failure
is indicated by calling `chrome.test.fail()`, a failed assertion, or through
an unexpected runtime error or API error (indicated in
`chrome.runtime.lastError`). Each test function must signal success or failure;
otherwise the test will hang (and eventually timeout).

A sample test suite may look like this.

```js
let tabId;

chrome.test.runTests([
  function createNewTab() {
    chrome.tabs.create({url: 'http://example.com'}, (tab) => {
      chrome.test.assertNoLastError();
      <verify |tab| properties>
      tabId = tab.id;
      chrome.test.succeed();
    });
  },
  function queryTab() {
    chrome.tabs.query({url: 'http://example.com'}, (tabs) => {
      chrome.test.assertNoLastError();
      <verify |tabs|>
      chrome.test.succeed();
    });
  },
  function removeTab() {
    chrome.tabs.remove(tabId, () => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },
]);
```

### Assertions
The ``chrome.test API`` provides a number of basic assertion methods.

#### assertTrue(condition, message?)
Asserts that the given condition is true, printing out the optional error
message if it is not.

#### assertFalse(condition, message?)
Asserts that the given condition is false, printing out the optional error
message if it is not.

#### assertEq(expected, actual, message?)
Asserts that the provided value matches the expected value.  If `expected` is
an object, this will perform a deep-equals check (i.e., verifying that two
objects are logically equivalent, rather than have the same address).  If the
expected value does not match the actual value, this will print out the
expected and actual values (through `JSON.stringify()` for objects).

#### assertNoLastError()
Asserts that `chrome.runtime.lastError` is undefined, printing out the error
otherwise.

#### assertLastError(expectedError)
Asserts that `chrome.runtime.lastError.message` is equivalent to
`expectedError`, printing out the expected and actual errors otherwise.

#### assertThrows(fn, self?, args[], expectedError?)
Asserts that executing `fn` with the context object of `self` (if defined) and
the specified `args` array throws a runtime error, which is then validated
against `expectedError`.  `expectedError` may be either a string (which must
match exactly) or a `RegExp`.

### callbackPass() and callbackFail()
**Important Notes:**
- `callbackPass()` and `callbackFail()` should absolutely **only** be used
  inside of `chrome.test.runTests()`.
- `callbackPass()` and `callbackFail()` are no longer as useful as they were,
  and sometimes result in less readable, more surprising code.  Think twice
  (or even thrice!) before using them.

`callbackPass()` and `callbackFail()` were primarily used when tests needed to
wait on two non-deterministic functions.  For instance, consider the
(contrived) following test:

```js
chrome.test.runTests([
  function step1() {
    chrome.tabs.create({url: 'http://example.com'}, () => {
      <verify state>
    });
    chrome.storage.local.set({foo: 'bar'}, () => {
      <verify state>
    });
    // Somehow end the test when both the tab creation and storage set have
    // finished.
  },
  function step2() {
    ...
  }
]);
```

Here, we want to have `step1()` finish after both the new tab has been created
and a storage value has been set (again, this is admittedly contrived).  There
is no hard guarantee about which function will finish first, so putting a
`chrome.test.succeed()` call in either may result in succeeding and continuing
to the next step too early.  Putting a `chrome.test.succeed()` call in both will
result in badness (see the [Do's And Don't's]).

`callbackPass()` lets the testing infrastructure handle this.  `callbackPass()`
takes a function to invoke as an argument, and wraps it.  When `callbackPass()`
is used (or any other function that increments an internal callback counter),
the testing infrastructure keeps track of that callback and waits for all
outstanding callbacks to be called before automatically continuing to the next
test.  A version of this test using `callbackPass()` may look like this.

```js
chrome.test.runTests([
  function step1() {
    chrome.tabs.create({url: 'http://example.com'}, callbackPass(() => {
      <verify state>
    }));
    chrome.storage.local.set({foo: 'bar'}, callbackPass(() => {
      <verify state>
    }));
  },
  function step2() {
    ...
  }
]);
```

Now, `chrome.test.succeed()` will be internally called by the testing
infrastructure once both callbacks have been invoked.

`callbackFail()` behaves similarly to `callbackPass()`, except that it
predominantly takes an expected error message that will be set in
`chrome.runtime.lastError` (though it takes a secondary argument of an optional
function to invoke).

#### Advantages
The most obvious advantage to using `callbackPass()` and `callbackFail()` is
that it eliminates the need for more complex callback management.  In addition
to keeping track of the callbacks, callbackPass() also automatically checks
that there was no API error raised in the callback, obviating the need for a
call to `chrome.test.assertNoLastError()`.  This can lead to more succinct
code.

#### Disadvantages
`callbackPass()` and `callbackFail()` can both lead to much less readable and
obvious code.  Extension APIs (and JavaScript in general) are already heavily
asynchronous, and use of `callbackPass()` and `callbackFail()` can lead to even
more confusion about when a test will finish, or which order items will be
executed in.  Coupled with the internal callback counter, hazards around
multiple calls to `chrome.test.succeed()` or `chrome.test.fail()`, and others,
and it is often much more clear to avoid using `callbackPass()` and
`callbackFail()`.  By comparison, having a single call to
`chrome.test.succeed()` in an extension test function makes it more clear when
success is met, and what the final operation is.  Additionally, there's no good
way to sequence multiple operations - there is only a single internal "callback
counter", which all callback-based utility methods share.  Finally, having
multiple ways to signal a test is "done" leads to frequent developer and
reviewer confusion and misuse (e.g., calling `chrome.test.succeed()` within a
`callbackPass()` - see the [Do's And Don't's]).

#### Alternatives
##### Restructure the Test
In the example above, it's unclear why the `storage.set()` call and
`tabs.create()` call need to be within the same test.  They could instead be
two separate test functions, passed serially in the array to
`chrome.test.runTests()`.

```js
chrome.test.runTests([
  function createTab() {
    chrome.tabs.create({url: 'http://example.com'}, () => {
      <verify state>
      chrome.test.succeed();
    });
  },
  function initializeStorage() {
    chrome.storage.local.set({foo: 'bar'}, () => {
      <verify state>
      chrome.test.succeed();
    });
  },
  function nextStep() {
    ...
  }
]);
```

##### Use Promises
The `chrome.test` callback mechanism was created before [Promises] were
available as a language feature in JavaScript.  Promises can provide similar
guarantees while providing more explicit direction to the reader about when a
test will finish.

```js
chrome.test.runTests([
  function step1() {
    let tabPromise = new Promise((resolve) => {
      chrome.tabs.create({url: 'http://example.com'}, () => {
        <verify state>
        resolve();
      });
    });
    let storagePromise = new Promise((resolve) => {
      chrome.storage.local.set({foo: 'bar'}, () => {
        <verify state>
        resolve();
      });
    });
    Promise.all([tabPromise, storagePromise]).then(() => {
      chrome.test.succeed();
    });
  },
  function step2() {
    ...
  }
]);
```

This solution is somewhat more verbose, but makes it more clear on which
criteria the test is waiting, and can allow for different "sequences" of
waiting.  This will also be much more streamlined when test APIs themselves can
return promises.

##### Use Async Functions
The `chrome.test.runTests()` allows for [asynchronous functions] and using the
[await] keyword. In conjunction with the Extension system's asynchronous APIs,
this can lead to reasonably concise and readable code, such as the example
below:

```js
const tab =
    await new Promise(resolve => chrome.tabs.create({url: url}, resolve));
<verify state>
```

For Promise-based calls in MV3 tests this can be simplified even more:

```js
const tab = await chrome.tabs.create({url: url});
<verify state>
```

### listenOnce() and listenForever()
`chrome.test.listenOnce()` and `chrome.test.listenForever()` are utility
functions used when waiting on different events.  Like `callbackPass()` and
`callbackFail()` above, they use the test API's internal callback counter,
allowing the test to finish automatically once all callbacks have been invoked.
Naturally, they share all the same disadvantages as well.

`listenOnce()` waits for the event to be invoked a single time, and then
removes the listener and reduces the internal callback counter.  Calling
`listenForever()` adds the listener and returns a function to be invoked at any
point; invoking this function will remove the listener and decrement the
callback counter.

### getConfig()
`chrome.test.getConfig()` retrieves the current configuration of the test
setup.  This may be necessary for a variety of reasons - a common one is
needing the port on which the test server is running in order to construct
URLs, as below.

```js
chrome.test.getConfig((config) => {
  let url = `http://example.com:${config.testServer.port}/simple.html`;
  createTab(url);
});
```

### sendMessage()
`chrome.test.sendMessage()` is used to communicate with the C++ side of the
browser test, allowing us to force synchronicity if necessary.  It should be
used with [ExtensionTestMessageListener] on the C++ side.

```c++
// test.cc:
IN_PROC_BROWSER_TEST_F(...) {
  LoadExtension(...);
  GURL url = GetASpecialURL();
  ExtensionTestMessageListener listener("clicked", ReplyBehavior::kWillReply);
  ClickAction();
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  listener.Reply(url.spec());
  ...
}
```

```js
// extension_script.js:
chrome.action.onClicked.addListener(() => {
  chrome.test.sendMessage('clicked', (specialUrl) => {
    useSpecialUrl(specialUrl);
  });
});
```

## Do's and Don't's
### **Do** Write Small, "B[i|y]te-Size" Tests
One of the advantages of having the `chrome.test.runTests()` method and
infrastructure is that we can write small, unit-test style tests without
needing to pay the cost of an extra browser test execution (expensive) for
each individual test case.  Because of this, we can write very targeted,
easy-to-consume, understandable test cases, rather than a single behemoth test
case.

### **Do** Prefer More Specific Asserts
`assertTrue()` and `assertFalse()` provide the least information in the logs
("expected true, found false"-style messages).  Assert-style methods like
`assertEq()`, `assertLastError()`, and `assertNoLastError()` can provide much
more detail (such as the actual value for `assertEq()`, or the error for
`assertNoLastError()`).

### **Do** Use Modern(ish) JS
Many tests were written years and years ago.  Modern JS practices, such as
`let` and `const`, [arrow functions], [template literals], and more, generally
increase readability.  Don't feel shy about using them just because other
examples don't.  (The caveat to this is that these tests should not use
anything that isn't [approved](/styleguide/web/es.md) for Chromium JS use,
unless the use of it is explicitly necessary for the test.)

### **Don't** Mix chrome.test.notifyPass() and chrome.test.runTests()
`chrome.test.notifyPass()` (or `chrome.test.notifyFail()`) will finish the
entire suite.  It should not be used with `chrome.test.runTests()`.  Instead,
use `chrome.test.succeed()`.

### **Don't** Mix chrome.test.callbackPass() et al. and chrome.test.succeed()
If the callback counter is incremented anywhere in a test function, the test
infrastructure will automatically invoke `chrome.test.succeed()` when the
counter reaches zero.  Calling `chrome.test.succeed()` in addition to
`callbackPass()`, `callbackFail()`, `listenOnce()`, or `listenForever()` can
result in unpredictable behavior, or masking failures.

### **Don't** Call other asynchronous functions after callbackPass/Fail()
Consider the following code:

```js
chrome.foo.asyncFunction(..., callbackPass(() => {
  chrome.foo.asyncFunction2(..., () => {
    // Extra stuff
  });
});
```

In this case, the callback counter will wait for the call to `asyncFunction()`
to complete and execute the inner function, and will then immediately end the
test case. The callback from `asyncFunction2()` will be invoked after the test
case is over, which can lead to flakiness or false-negatives.

If you have to have nested functions, each should use `callbackPass()` or
`callbackFail()`. Better yet, migrate away from `callbackPass()` and
`callbackFail()` with [these alternatives](#Alternatives).

### **Don't** Have multiple "win" conditions
While it's possible to have multiple `notifyPass()` calls in a single C++
browser test (by using multiple calls to `RunExtensionTest()` or using multiple
`ResultCatchers`), and by extension possible to mix `runTests()` with
`notifyPass()` with `sendMessage()` with everything else, it's generally a bad
idea.  It makes the flow of control incredibly difficult to parse for author,
reviewer, and future reader, and frequently leads to subtle bugs being missed.

Instead, if a complex chain of steps is needed, use either
`chrome.test.runTests()` or `chrome.test.sendMessage()` (if C++ coordination is
needed).

### **Don't** Go overboard with runTests()
It can be tempting to set up a 20-step sequence in
`chrome.test.runTests()`, but this makes it very difficult to understand the
total flow, harder to debug, and increases the risk of timing out (from simply
doing too much).  If a test relies on multiple steps in `runTests()`, have a
dedicated browser test for those related steps, and restrict them to a
reasonable number.  If a test has a series of unrelated steps, keep them small
and targeted (in good unit testing behavior).

[writing extension tests]: extension_tests.md
[Do's And Don't's]: #Dos-and-Dont_s
[Promises]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Promise
[asynchronous functions]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Statements/async_function
[await]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Operators/await
[ExtensionTestMessageListener]: ../test/extension_test_message_listener.h
[arrow functions]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Functions/Arrow_functions
[template literals]: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Template_literals
