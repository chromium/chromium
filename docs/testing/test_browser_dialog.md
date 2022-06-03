# Testing Chrome browser UI with TestBrowserUi

\#include "[chrome/browser/ui/test/test_browser_ui.h]"

`TestBrowserUi` (and convenience class `TestBrowserDialog`) provide ways to
register an `InProcessBrowserTest` testing harness with a framework that invokes
Chrome browser UI in a consistent way. They optionally provide a way to invoke
UI "interactively". This allows screenshots to be generated easily, with the
same test data, to assist with UI review. `TestBrowserUi` also provides a UI
registry so pieces of UI can be systematically checked for subtle changes and
regressions.

[TOC]

## How to register UI

If registering existing UI, there's a chance it already has a test harness
inheriting, using, or with `typedef InProcessBrowserTest` (or a descendant of
it). If so, using `TestBrowserDialog` (for a dialog) is straightforward, and
`TestBrowserUi` (for other types of UI) relatively so. Assume the existing
`InProcessBrowserTest` is in `foo_browsertest.cc`:

    class FooUiTest : public InProcessBrowserTest { ...

Change this to inherit from `DialogBrowserTest` (for dialogs) or `UiBrowserTest`
(for non-dialogs), and override `ShowUi(std::string)`. For non-dialogs, also
override `VerifyUi()` and `WaitForUserDismissal()`. See
[Advanced Usage](#Advanced-Usage) for details.

```cpp
class FooUiTest : public UiBrowserTest {
 public:
  ..
  // UiBrowserTest:
  void ShowUi(const std::string& name) override {
    /* Show Ui attached to browser() and leave it open. */
  }
  // These next two are not necessary if subclassing DialogBrowserTest.
  bool VerifyUi() override {
    /* Return true if the UI was successfully shown. */
  }
  void WaitForUserDismissal() override {
    /* Block until the UI has been dismissed. */
  }
  ..
};
```

Finally, add test invocations using the usual GTest macros, in
`foo_browsertest.cc`:

```cpp
IN_PROC_BROWSER_TEST_F(FooUiTest, InvokeUi_default) {
  ShowAndVerifyUi();
}
```

Notes:

*   The body of the test is always just "`ShowAndVerifyUi();`".
*   "`default`" is the `std::string` passed to `ShowUi()` and can be
    customized. See
    [Testing additional UI "styles"](#Testing-additional-ui-styles).
*   The text before `default` (in this case) must always be "`InvokeUi_`".

### Concrete examples

*   [chrome/browser/ui/ask_google_for_suggestions_dialog_browsertest.cc]
*   [chrome/browser/infobars/infobars_browsertest.cc]

##  Running the tests

List the available pieces of UI with

    $ ./browser_tests --gtest_filter=BrowserUiTest.Invoke

E.g. `FooUiTest.InvokeUi_default` should be listed. To show the UI
interactively, run

    $ ./browser_tests --gtest_filter=BrowserUiTest.Invoke \
      --test-launcher-interactive --ui=FooUiTest.InvokeUi_default

### Implementation

`BrowserUiTest.Invoke` searches for gtests that have "`InvokeUi_`"  in their
names, so they can be collected in a list. Providing a `--ui` argument will
invoke that test case in a subprocess. Including `--test-launcher-interactive`
will set up an environment for that subprocess that allows interactivity, e.g.,
to take screenshots. The test ends once the UI is dismissed.

The `FooUiTest.InvokeUi_default` test case **will still be run in the usual
browser_tests test suite**. Ensure it passes, and isn’t flaky. This will
give your UI some regression test coverage. `ShowAndVerifyUi()` checks to ensure
UI is actually created when it invokes `ShowUi("default")`.

### BrowserUiTest.Invoke

This is also run in browser_tests but, when run that way, the test case just
lists the registered test harnesses (it does *not* iterate over them). A
subprocess is never created unless --ui is passed on the command line.

## Advanced Usage

If your test harness inherits from a descendant of `InProcessBrowserTest` (one
example: [ExtensionBrowserTest]) then the `SupportsTestUi<>` and
`SupportsTestDialog` templates are provided. E.g.

```cpp
class ExtensionInstallDialogViewTestBase : public ExtensionBrowserTest { ...
```

becomes

```cpp
class ExtensionInstallDialogViewTestBase :
    public SupportsTestDialog<ExtensionBrowserTest> { ...
```

If you need to do any setup before `ShowUi()` is called, or any teardown in the
non-interactive case, you can override the `PreShow()` and `DismissUi()
methods.

### Testing additional UI "styles"

Add additional test cases, with a different string after "`InvokeUi_`".
Example:

```cpp
IN_PROC_BROWSER_TEST_F(CardUnmaskViewBrowserTest, InvokeUi_expired) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(CardUnmaskViewBrowserTest, InvokeUi_valid) {
  ShowAndVerifyUi();
}
```

The strings "`expired`" or “`valid`” will be given as arguments to
`ShowUi(std::string)`.

## Rationale

Bug reference: [Issue 654151](http://crbug.com/654151).

Chrome has a lot of browser UI; often for obscure use-cases and often hard to
invoke. It has traditionally been difficult to be systematic while checking UI
for possible regressions. For example, to investigate changes to shared layout
parameters which are testable only with visual inspection.

For Chrome UI review, screenshots need to be taken. Iterating over all the
"styles" that UI may appear with is fiddly. E.g. a login or particular web
server setup may be required. It’s important to provide a consistent “look” for
UI review (e.g. same test data, same browser size, anchoring position, etc.).

Some UI lacks tests. Some UI has zero coverage on the bots. UI elements can have
tricky lifetimes and common mistakes are repeated. TestBrowserUi runs simple
"Show UI" regression tests and can be extended to do more.

Even discovering the full set of UI present for each platform in Chrome is
[difficult](http://crbug.com/686239).

### Why browser_tests?

*   `browser_tests` already provides a `browser()->window()` of a consistent
    size that can be used as a dialog anchor and to take screenshots for UI
    review.
    *   UI review have requested that screenshots be provided with the entire
        browser window so that the relative size of the UI element/change under
        review can be assessed.

*   Some UI already has a test harness with appropriate setup (e.g. test data)
    running in browser_tests.
    *   Supporting `BrowserUiTest` should require minimal setup and minimal
        ongoing maintenance.

*   An alternative is to maintain a working end-to-end build target executable
    to do this, but this has additional costs (and is hard).
    *    E.g. setup/teardown of low-level functions
         (`InitializeGLOneOffPlatform()`, etc.).

*   Why not chrome.exe?
    *   E.g. a scrappy chrome:// page with links to invoke UI would be great!
    *   But...
        *   UI may have test data (e.g. credit card info) which shouldn’t be in
        the release build.
        *   UI may use EmbeddedTestServer.
        *   Higher maintenance cost - can’t leverage existing test harnesses.

## Future Work

*   Opt in more UI!
    *    Eventually, all of it.
    *    A `DialogBrowserTest` for every descendant of `views::DialogDelegate`.

*   Automatically generate screenshots (for each platform, in various languages)
    *    Build upon [CL 2008283002](https://codereview.chromium.org/2008283002/)

*   (maybe) Try removing the subprocess
    *    Probably requires altering the browser_test suite code directly rather
         than just adding a test case as in the current approach

*   An automated test suite for UI
    *    Test various ways to dismiss or hide UI, especially dialogs
         *    e.g. native close (via taskbar?)
         *    close parent window (possibly via task bar)
         *    close parent tab
         *    switch tabs
         *    close via `DialogClientView::AcceptWindow` (and `CancelWindow`)
         *    close via `Widget::Close`
         *    close via `Widget::CloseNow`
    *    Drag tab off browser into a new window
    *    Fullscreen that may create a new window/parent

*   Find obscure workflows for invoking UI that has no test coverage and causes
    crashes (e.g. [http://crrev.com/426302](http://crrev.com/426302))
    *   Supporting window-modal dialogs with a null parent window.

*   Find memory leaks, e.g. [http://crrev.com/432320](http://crrev.com/432320)
    *   "Fix memory leak for extension uninstall dialog".

## Appendix: Sample output

**$ ./out/gn_Debug/browser_tests --gtest_filter=BrowserUiTest.Invoke**
```
Note: Google Test filter = BrowserUiTest.Invoke
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from BrowserUiTest
[ RUN      ] BrowserUiTest.Invoke
[26879:775:0207/134949.118352:30434675...:INFO:browser_ui_browsertest.cc(46)
Pass one of the following after --ui=
        AppInfoDialogBrowserTest.InvokeUi_default
        AskGoogleForSuggestionsDialogTest.DISABLED_InvokeUi_default
        BluetoothChooserBrowserTest.InvokeUi_ConnectedBubble
        BluetoothChooserBrowserTest.InvokeUi_ConnectedModal
/* and many more */
[       OK ] BrowserUiTest.Invoke (0 ms)
[----------] 1 test from BrowserUiTest (0 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (1 ms total)
[  PASSED  ] 1 test.
[1/1] BrowserUiTest.Invoke (334 ms)
SUCCESS: all tests passed.
```

**$ ./out/gn_Debug/browser_tests --gtest_filter=BrowserUiTest.Invoke
--ui=CardUnmaskPromptViewBrowserTest.InvokeUi_expired**

```
Note: Google Test filter = BrowserUiTest.Invoke
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from BrowserUiTest
[ RUN      ] BrowserUiTest.Invoke
Note: Google Test filter = CardUnmaskPromptViewBrowserTest.InvokeDefault
[==========] Running 1 test from 1 test case.
[----------] Global test environment set-up.
[----------] 1 test from CardUnmaskPromptViewBrowserTest, where TypeParam =
[ RUN      ] CardUnmaskPromptViewBrowserTest.InvokeUi_expired
/* 7 lines of uninteresting log spam */
[       OK ] CardUnmaskPromptViewBrowserTest.InvokeUi_expired (1324 ms)
[----------] 1 test from CardUnmaskPromptViewBrowserTest (1324 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (1325 ms total)
[  PASSED  ] 1 test.
[       OK ] BrowserUiTest.Invoke (1642 ms)
[----------] 1 test from BrowserUiTest (1642 ms total)
[----------] Global test environment tear-down
[==========] 1 test from 1 test case ran. (1642 ms total)
[  PASSED  ] 1 test.
[1/1] BrowserUiTest.Invoke (2111 ms)
SUCCESS: all tests passed.
```

**$ ./out/gn_Debug/browser_tests --gtest_filter=BrowserUiTest.Invoke
--ui=CardUnmaskPromptViewBrowserTest.InvokeUi_expired
--test-launcher-interactive**
```
/*
 * Output as above, except the test are not interleaved, and the browser window
 * should remain open until the UI is dismissed
 */
```

[chrome/browser/ui/test/test_browser_ui.h]: https://cs.chromium.org/chromium/src/chrome/browser/ui/test/test_browser_ui.h
[chrome/browser/ui/test/test_browser_dialog.h]: https://cs.chromium.org/chromium/src/chrome/browser/ui/test/test_browser_dialog.h
[chrome/browser/ui/ask_google_for_suggestions_dialog_browsertest.cc]: https://cs.chromium.org/chromium/src/chrome/browser/ui/ask_google_for_suggestions_dialog_browsertest.cc?l=18&q=ShowUi
[chrome/browser/infobars/infobars_browsertest.cc]: https://cs.chromium.org/chromium/src/chrome/browser/infobars/infobars_browsertest.cc?l=134&q=UiBrowserTest
[ExtensionBrowserTest]: https://cs.chromium.org/chromium/src/chrome/browser/extensions/extension_browsertest.h?q=extensionbrowsertest&l=40
