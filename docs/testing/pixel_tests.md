# Testing Chrome browser UI with Pixel Tests

Pixel Tests compare one or more screenshots with already-approved images using
[Skia Gold](/docs/ui/learn/glossary.md#skia-gold). They guarantee that the UI
does not change its appearance unexpectedly and are a good addition to a
portfolio of regression tests.

There are two ways that pixel tests can be written:
 - Kombucha, using the `Screenshot` test verbs (preferred)
 - Using the `TestBrowserUi` API (provided for legacy support; do not use)

[TOC]

## Common Requirements

All pixel tests must be placed in
[pixel_tests.filter](/testing/buildbot/filters/pixel_tests.filter).

Tests in `browser_tests` will also be run in the `pixel_browser_tests` job on
eligible builders. Tests in `interactive_ui_tests` will also be run in the
`pixel_interactive_ui_tests` job on eligible builders.

### Baseline CLs

All methods of taking screenshots allow you to set a baseline CL. This ensures
that old expected images are thrown away when the UI is modified. If the
baseline were not set, a regression that caused some new UI to appear might pass
a pixel test because it matched an older version of the UI surface.

The procedure for baseline CLs is:
1. Put in a placeholder string.
1. Upload your CL to Gerrit.
1. Find the number of your new CL (e.g. from the URL)
1. Replace the placeholder with the number.
1. Re-upload the CL.

### Running Tests Locally

To run a pixel test locally, use:
```sh
test_executable --gtest_filter=Test.Name --browser-ui-tests-verify-pixels --enable-pixel-output-in-tests --test-launcher-retry-limit=0
```
Where `<test_executable>` is either `browser_tests` or `interactive_ui_tests`,
and `<Test.Name>` is the full name of your test, with dot.

If you want to see UI as it's being screenshot, replace
`--enable-pixel-output-in-tests` with `--test-launcher-interactive` - this will
freeze the test just after the screenshot is taken. Dismiss the UI to continue
the test.

### Diagnosing Test Failures

Failed pixel tests will have a URL link to Skia Gold in the test output through
which you can view the expected and actual screenshot images. Log in and either
accept or reject images that do not match the baseline. Once you've accepted a
new image, future tests that produce the same output won't fail.

## Writing Pixel Tests with Kombucha

[Kombucha](/chrome/test/interaction/README.md) tests use a declarative syntax to
perform interaction testing on the browser. They are the preferred way to create
end-to-end interaction and critical user journey regression tests for Chrome
Desktop.

Nearly all Kombucha tests can derive directly from
[InteractiveBrowserTest](/chrome/test/interaction/interactive_browser_test.h).
`InteractiveBrowserTest` is a strict superset of `InProcessBrowserTest` so it is
usually safe to simply swap one for the other.

### Taking Screenshots

To use Kombucha to pixel-test UI, use the `Screenshot` or `ScreenshotSurface`
verb. Because these tests will also be run in non-pixel-test mode, you will need
to precede the first screenshot with `SetOnIncompatibleAction()` with an option
other than `kFailTest` (which is the default).

The `Screenshot` verb takes a picture of _exactly the UI element specified_.

The `ScreenshotSurface` verb takes a picture of the entire dialog or window
containing the element. Be careful not to capture other elements that are likely
to change on their own!

Example:

```cpp
// Inherit from InteractiveBrowserTest[Api]:
class MyNewDialogUiTest : public InteractiveBrowserTest { ... };

// Baseline Gerrit CL number of the most recent CL that modified the UI.
constexpr char kScreenshotBaselineCL[] = "12345678";

// Screenshot the feature's entrypoint button, then click it, wait for the
// feature's dialog, then screenshot the entire dialog.
IN_PROC_BROWSER_TEST_F(MyNewDialogUiTest, OpenAndVerifyContents) {
  RunTestSequence(
    SetOnIncompatibleAction(
        OnIncompatibleAction::kIgnoreAndContinue
        "Screenshots not supported in all testing environments."),

    // Grab a screenshot of the toolbar button that is the entry point for the feature.
    Screenshot(kMyNewToolbarButtonElementId,
               /*screenshot_name=*/"entry_point",
               /*baseline_cl=*/kScreenshotBaselineCL)

    PressButton(kMyNewToolbarButtonElementId),

    WaitForShow(MyNewDialog::kDialogElementId),

    // Grab a screenshot of the entire dialog that pops up.
    ScreenshotSurface(MyNewDialog::kDialogElementId,
                      /*screenshot_name=*/"whole_dialog",
                      /*baseline_cl=*/kScreenshotBaselineCL));
}
```

Note that a test can take multiple screenshots; they must have unique names. In
the above example, `entry_point` and `whole_dialog` will be treated as two
separate screenshots to be compared against separate Skia Gold masters.

## Writing Pixel Tests with TestBrowserUi

`UiBrowserTest` and `DialogBrowserTest` provide an alternate (and older) method
of capturing a surface for pixel testing. These are also base classes your test
harness needs to inherit from, and also replace `InProcessBrowserTest`.

For example, assume the existing
`InProcessBrowserTest` is in `foo_browsertest.cc`:
```
    class FooUiTest : public InProcessBrowserTest { ...
```
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

###  Running the tests

List the available `TestBrowserUi` tests:

    $ ./browser_tests --gtest_filter=BrowserUiTest.Invoke
    $ ./interactive_ui_tests --gtest_filter=BrowserInteractiveUiTest.Invoke

E.g. `FooUiTest.InvokeUi_default` should be listed. To show the UI
interactively, run

    # If FooUiTest is a browser test.
    $ ./browser_tests --gtest_filter=BrowserUiTest.Invoke \
      --test-launcher-interactive --ui=FooUiTest.InvokeUi_default

    # If FooUiTest is an interactive UI test.
    $ ./interactive_ui_tests --gtest_filter=BrowserInteractiveUiTest.Invoke \
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

`BrowserInteractiveUiTest` is the equivalent of `BrowserUiTest` for
interactive_ui_tests.

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

*   Automatically generate screenshots (for each platform, in various languages)
    *    Build upon [CL 2008283002](https://codereview.chromium.org/2008283002/)

*   (maybe) Try removing the subprocess
    *    Probably requires altering the browser_test suite code directly rather
         than just adding a test case as in the current approach

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
