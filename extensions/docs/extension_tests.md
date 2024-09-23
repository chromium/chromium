# Writing Extensions Tests

[TOC]

## Overview
This describes testing in the Chromium extensions system, including
common test types (and when to use each), test utilities and test suites, and
gives examples of writing extension API tests.

## Common Test Types
### Unit Tests
Unit tests in Chromium (such as the `unit_tests` and `extensions_unittests`
targets) refer to tests that run in a single process.  This process may be the
browser process (the main “Chrome” process), a renderer process (such as a
website process or an extension process), or a utility process (such as one
used to unpack an extension).  Unit tests in Chromium can be multi-threaded,
but cannot span multiple processes.  Many pieces of the environment are either
mocked or stubbed out, or simply omitted, in unit tests.

Unit tests are generally smaller, faster, and significantly less flaky than
other test types.  This results in fewer tests getting disabled.  However, unit
tests have two significant drawbacks.  First, because they operate in a
significantly pared-down environment, they may obscure real bugs that can be
hit.  Second, since they are single process, they are incompatible with
anything that requires both a renderer and a browser (such as an extension
process running and the browser process handling its input).

### Browser Tests
Browser tests in Chromium (such as the `browser_tests` and
`extensions_browsertests` targets) are multi-process, and instantiate a "real"
browser.  That is, the majority of the environment is set up, and it much more
closely resembles an environment that the Chrome browser normally operates in.

Browser tests are useful when a test needs multi-process integration.  This is
typically “browser + renderer”, such as when you need to exercise the behavior
of the browser in response to renderer parsing and input (and can’t suitably
mock it out).  Browser tests are significantly more expensive (and frequently
more flaky, due to the amount of state and interaction they entail) than unit
tests, but also exercise systems in a more end-to-end fashion, potentially
giving more confidence that something "actually works".

### Interactive UI Tests
Interactive UI tests (built with the `interactive_ui_tests`
target) are tests that are multi-process - like browser tests - but execute
serially.  This allows for user interaction and blocking event loops, such as
opening menus, performing click-and-drag events, etc.

Interactive UI tests should only be used if they're really necessary, such as
when testing focus, blocking UI, or drag-and-drop interactions.

### A Good General Practice
In general, prefer the order of Unit Tests > Browser Tests > Interactive UI
Tests.  If a test is equally good as any, it should ideally be a unit test.

A good practice is to use a combination of unit tests, with additional browser
tests if appropriate.  Unit tests are good for exercising behavior that is
isolated to the system under test, and are the best place to test various edge
cases and a variety of different scenarios.  Browser tests provide more
end-to-end coverage and integration-style testing, and are useful when a system
has components in multiple processes (such as extension APIs, that are called
from the browser but handled in the renderer).

As a practical example, a new API being tested might have unit tests for each
API method that pass in various different inputs in different scenarios and
test the state and return values, with one or two browser tests that ensure the
API works end-to-end (from being called in the renderer, processed in the
browser, and returning to the renderer).

Of course, this may not apply in every situation - any test that needs both
renderer and browser involvement requires a browser test.

## Common Extension Testing Tools
### Test Utilities
Below are a handful of the most common test utilities.  There are others - when
in doubt, search the code for similar behavior to see if something already
exists (or ask a member of the extensions team).

#### ExtensionBuilder
`ExtensionBuilder` lets you construct an Extension object easily.  This
Extension can then be passed around to various systems or added to the profile
(typically through a method like `ExtensionService::AddExtension()`).  However,
this extension has no backing files on the filesystem - this means it cannot be
reloaded, won't have a (functional) background process, etc.  ExtensionBuilder
is primarily useful in unit tests.

**Example Usage**
```c++
scoped_refptr<const Extension> extension =
    ExtensionBuilder("my extension name")
        .AddPermission("tabs")
        .AddContentScript("script.js", {"*://*.example/*"})
        .SetVersion("1.0")
        .Build();
```

#### ChromeTestExtensionLoader
`ChromeTestExtensionLoader` takes a file path, and loads an extension (adding it
to the Profile).  It works in both browser tests and unit tests, with both
packed (.crx) and unpacked extensions, and has various different options for
customization.

**Example Usage**
```c++
scoped_refptr<const Extension> extension =
    ChromeTestExtensionLoader(profile()).LoadExtension(path_to_extension);
```

#### TestExtensionDir
`TestExtensionDir` lets you create your very own file-backed extension right in
the body of your test.  This allows for adding a "real" extension to a profile,
which will work with functions like reloading and (if in a browser test) having
a real extension process.  Under the hood, TestExtensionDir uses a
ScopedTempDir, which will be automatically cleaned up.

Prefer TestExtensionDirs when the test extension is relatively short, as it
saves readers of the code from having to bounce around between the various
files that comprise an extension.  If the test extension is more verbose,
however, it can be cleaner to put the extension's code in the test data
directory (e.g. `//chrome/test/data/extensions` or
`//chrome/test/data/extensions/api_test`).

**Example Usage**
```c++
TestExtensionDir test_dir;
constexpr char kManifest[] =
    R"({
         "name": "My Extension",
         "version": "0.1",
         "manifest_version": 2,
         "background": { "scripts": ["background.js"] },
         "permissions": ["storage"]
       })";
constexpr char kBackgroundJs[] =
    R"(chrome.storage.local.set({foo: 'bar'}, () => {
         chrome.test.sendMessage('storage set');
       });)";
test_dir.WriteManifest(kManifest);
test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), kBackgroundJs);
const Extension* extension = LoadExtension(test_dir.UnpackedPath());
```

#### ExtensionTestMessageListener
`ExtensionTestMessageListener` is a helper class to coordinate between C++ and
an extension's running JS, either for passing data or for forcing synchronicity
between events.  This class is only useful in browser tests.

**Example Usage**

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

Another common use for an ExtensionTestMessageListener is to ensure that an
extension has performed any initial set up necessary before continuing along in
the C++.

```c++
// test.cc:
IN_PROC_BROWSER_TEST_F(...) {
  ExtensionTestMessageListener listener("ready");
  LoadExtension(...);
  ASSERT_TRUE(listener.WaitUntilSatisfied());
  // The extension is now ready!
  ...
}
```

```js
// extension_script.js:
chrome.storage.local.get('config', (config) => {
  performSetup(config).then(() => {
    chrome.test.sendMessage('ready');
  });
});
```

#### ResultCatcher
A helper class to wait for the success or failure result of an extension test
from an extension using the `chrome.test` API. This class is only useful in
browser tests.

**Example Usage**

```c++
// test.cc:
IN_PROC_BROWSER_TEST_F(...) {
  LoadExtension(...);
  ResultCatcher result_catcher;
  ClickAction();
  ASSERT_TRUE(result_catcher.GetNextResult()) << result_catcher.message();
  ...
}
```

```js
// extension_script.js:
chrome.action.onClicked.addListener(() => { chrome.test.notifyPass(); });
```

#### ScriptResultQueue
A `ScriptResultQueue` is queue of results passed from
`chrome.test.sendScriptResult()`. Conceptually, it's somewhat similar to an
`ExtensionTestMessageListener` (they each allow getting data from running
JS), with a few key differences:
- It's one-directional (it does not allow communicating back with the JS),
- It takes any serializable argument, so can be used to pass objects, ints,
  etc, and
- It queues up multiple results.

**Example Usage**

```c++
// test.cc
IN_PROC_BROWSER_TEST_F(...) {
  LoadExtension(...);
  ScriptResultQueue result_queue;
  ClickAction();
  base::Value first_result = result_queue.GetNextResult();
  ClickAction();
  base::Value second_result = result_queue.GetNextResult();
}
```

```js
// extension_script.js
chrome.action.onClicked.addListener((tab) => {
  chrome.test.sendScriptResult(tab);
});
```

#### BackgroundScriptExecutor
A `BackgroundScriptExecutor` is used to execute Javascript in the context of a
test extension's background context. This class works with both service
worker-based and background page-based extensions. It can internally leverage
`ScriptResultQueue` to capture asynchronous results.

**Example Usage**

```c++
// test.cc
IN_PROC_BROWSER_TEST_F(...) {
  static constexpr char kManifest[] =
      R"({
           "name": "test ext",
           "manifest_version": 2,
           "version": "0.1",
           "background": {"service_worker": "background.js"},
           "permissions": ["scripting"],
           "host_permissions": ["<all_urls>"]
         })";
  TestExtensionDir test_dir;
  test_dir.WriteManifest(kManifest);
  test_dir.WriteFile(FILE_PATH_LITERAL("background.js"), "// Empty"));
  const Extension* extension = LoadExtension(test_dir.UnpackedPath());
  SetUpTestPage();
  static constexpr char kScript[] =
      R"((async () => {
           let injectionResults =
               await chrome.scripting.executeScript(
                   {
                     target: {tabId: %d},
                     func: () => { return document.title; }
                   });
           let title = injectionResults[0].result;
           chrome.test.sendScriptResult(title);
         })())";
  base::Value script_result =
      BackgroundScriptExecutor::ExecuteScript(
          extension->id(),
          base::StringPrintf(kScript, GetActiveTabId()),
          BackgroundScriptExecutor::ResultCapture::kSendScriptResult);
  EXPECT_THAT(script_result,
              base::test::IsJson(R"("My Page Title")"));
}
```

### Test Suites
#### ExtensionServiceTestBase
A somewhat poorly-named common unit test suite class that sets up an extension
environment in a unit test.

#### ExtensionBrowserTest
The crux of most extension browser tests, this class provides handy methods,
primarily focusing on loading extensions.

#### ExtensionApiTest
A subclass of ExtensionBrowserTest, ExtensionApiTest provides infrastructure to
load an extension, let it run JS-based tests, and await the pass / fail result.

## Writing Extension API Tests
For the following examples, assume we have an API defined by this schema:
```
namespace frobulation {
  ...

  interface Functions {
    void frobulate(FrobulateOptions options,
                   optional FrobulateResultCallback callback);
  }
}
```

### Extension API Unit Tests
In line with the general practice described above, extension API unit tests are
well-suited to exercising a variety of inputs and outputs, edge cases, and
different scenarios.  A couple example unit test for this API might look like
this:

```c++
TEST_F(FrobulationApiUnitTest, CallingFrobulateKicksOffFrobulation) {
  FrobulatorService* service = FrobulatorService::Get();
  EXPECT_FALSE(service->IsFrobulating());

  Browser* browser = CreateTestBrowser();
  auto frobulate_function =
      base::MakeRefCounted<FrobulationFrobulateFunction>();
  std::optional<base::Value> result(
      api_test_utils::RunFunctionAndReturnSingleResult(
          frobulate_function.get(), R"([{"speed": 10, "target": "foo"}])",
          browser));
  ASSERT_TRUE(result);
  <validate |result| data>

  EXPECT_TRUE(service->IsFrobulating());
  <validate any additional state>
}

TEST_F(FrobulationApiUnitTest, CallingFrobulateFailsWithTooHighASpeed) {
  FrobulatorService* service = FrobulatorService::Get();
  EXPECT_FALSE(service->IsFrobulating());

  Browser* browser = CreateTestBrowser();
  auto frobulate_function =
      base::MakeRefCounted<FrobulationFrobulateFunction>();
  std::string error = api_test_utils::RunFunctionAndReturnError(
                          frobulate_function.get(),
                          R"([{"speed": 1000, "target": "foo"}])", browser));
  EXPECT_EQ("Speed too high!", error);
  EXPECT_FALSE(service->IsFrobulating());
}
```

### Extension API Browser Tests
Extension API browser tests frequently subclass the ExtensionApiTest class.  A
common pattern for these tests is to drive the test almost entirely from JS.
To do this, write a test extension, load it in the test, and leverage the
chrome.test API in order to perform assertions and break the test into subtests.

The advantage to this type of test is that it "really" uses the API.  The API
is being called by an extension installed in the Chromium browser, just as it
would be by a real-world extension.  This also exercises the extension bindings
code and renderer-side processing, which is not exercised by (browser-side) API
unit tests.  However, these tests are also frequently more expensive, flakier,
less readable, and more difficult to perform validation in.

```c++
using FrobulationApiTest = ExtensionApiTest;
IN_PROC_BROWSER_TEST_F(FrobulationApiTest, TestFrobulationWorks) {
  // This loads and runs an extension from
  //chrome/test/data/extensions/api_test/frobulation.
  ASSERT_TRUE(RunExtensionTest("frobulation")) << message();
  <possibly verify any extra state>
}
```

```json
//chrome/test/data/extensions/api_test/frobulation/manifest.json
{
  "name": "Frobulator Test",
  "version": "0.1",
  "manifest_version": 2,
  "background": {"scripts": ["background.js"]},
  "permissions": ["frobulation"]
}
```

```js
//chrome/test/data/extensions/api_test/frobulation/background.js
chrome.test.runTests([
  function callingFrobulateSucceeds() {
    chrome.frobulation.frobulate({speed: 10, target: "foo"}, (res) => {
      chrome.test.assertNoLastError(); chrome.test.succeed(); });
  },
  // More tests can go here.
]);
```

See [Using the chrome.test API](/extensions/docs/testing_api.md) for more
information on how to write extension API tests.

#### Test resources

ExtensionBrowserTest provides a mapping for requests to
chrome-extension://<id>/_test_resources/<path>, where the resource will be
loaded from //chrome/test/data/extensions/<path> instead of from the
extension's directory. This can be used to retrieve files from other
locations in //chrome/test/data/ and have them be treated as same-origin to
the extension (which is important for CSP and CORS rules).

One example of when to use this is to share resources between tests: a
common resource can live at //chrome/test/data/extensions/<common dir>
and be leveraged by multiple test extensions without needing to duplicate
the file into each test extension's directory.

Any of the following URLs can be used to access the same resource.html using
the same origin as extension. This extension lives in the `extension` dir. Note
that the string representation of the urls are not expected to be identical.

```js
//chrome/test/data/extensions/api_test/extension/service_worker.js
const url1 = '_test_resources/api_test/extension/resource.html';
const url2 = chrome.runtime.getURL('resource.html'),
const url3 = `chrome-extension://${chrome.runtime.id}/resource.html`;
```

The following is an example showcasing the use of a shared resource.
The resource will have the same origin as the extension.
```js
//chrome/test/data/extensions/shared/resource.html
This is a shared resource that lives outside of an extension.
```
```js
//chrome/test/data/extensions/api_test/extension/service_worker.js
fetch('_test_resources/shared/resource.html');
```
