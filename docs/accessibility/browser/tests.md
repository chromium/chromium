# Accessibility

Here's a quick overview of all of the locations in the codebase where you'll
find accessibility tests, and a brief overview of the purpose of all of them.

## Web Tests

This is the primary place where we test accessibility code in Blink. This code
should have no platform-specific code. Use this to test anything where there's
any interesting web platform logic, or where you need to be able to query things
synchronously from the renderer thread to test them.

Don't add tests for trivial features like ARIA attributes that we just expose
directly to the next layer up. In those cases the Blink tests are trivial and
it's more valuable to test these features at a higher level where we can ensure
they actually work.

Note that many of these tests are inherited from WebKit and the coding style has
evolved a lot since then. Look for more recent tests as a guide if writing a new
one.

Test files:
[third_party/blink/web_tests/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/accessibility/)

Source code to AccessibilityController and WebAXObjectProxy:
[content/web_test/renderer](https://source.chromium.org/chromium/chromium/src/+/main:content/web_test/renderer/)

First, you'll need to build the tests:
```
autoninja -C out/release blink_tests
```

Then, to run all accessibility web tests:

```
third_party/blink/tools/run_web_tests.py --build-directory=out --target=release accessibility/
```

Or, to run just one test by itself, without invoking the `run_web_tests.py` script:

```
out/release/content_shell \
  --run-web-tests third_party/blink/web_tests/accessibility/name-calc-inputs.html
```

For information on modifying or adding web tests, see the main
[web tests documentation](../../testing/web_tests.md).

## DumpAccessibilityTree tests

This is the best way to write both cross-platform and platform-specific tests
using only an input HTML file, some magic strings to describe what attributes
you're interested in, and one or more expectation files to enable checking
whether the resulting accessibility tree is correct or not. In particular,
almost no test code is required.

[More documentation on DumpAccessibilityTree](../../../content/test/data/accessibility/readme.md)

Test files:
[content/test/data/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:content/test/data/accessibility/)

Test runner:
[content/browser/accessibility/dump_accessibility_tree_browsertest.h](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/accessibility/dump_accessibility_tree_browsertest.h)

To run all tests:

```
autoninja -C out/release content_browsertests && \
  out/release/content_browsertests --gtest_filter="All/DumpAccessibilityTree*"
```

Expectation baselines for each OS can be generated via:

```
tools/accessibility/rebase_dump_accessibility_tree_tests.py
```

## Other content_browsertests

There are many other tests in content/ that relate to accessibility. All of
these tests work by launching a full multi-process browser shell, loading a web
page in a renderer, then accessing the resulting accessibility tree from the
browser process, and running some test from there.

To run all tests:

```
autoninja -C out/release content_browsertests && \
  out/release/content_browsertests --gtest_filter="*ccessib*"
```

## Accessibility unittests

This tests the core accessibility code that's shared by both web and non-web
accessibility infrastructure.

Code location:
[ui/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/)
and subdirectories; check the
[`*_unittest.cc` files](https://source.chromium.org/search?q=path:accessibility%20path:_unittest&sq=&ss=chromium%2Fchromium%2Fsrc:ui%2Faccessibility%2F),
next to every file that it's being tested.

List of sources:
[ui/accessibility/BUILD.gn](https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/BUILD.gn?q=%22test(%22accessibility_unittests%22)%20%7B%22)

To run all tests:

```
autoninja -C out/release accessibility_unittests && \
  out/release/accessibility_unittests
```

## ChromeVox tests

ChromeVox tests are part of the browser_tests suite. You must build with
`target_os = "chromeos"` in your GN args.

To run all tests:

```
autoninja -C out/release browser_tests && \
  out/release/browser_tests --test-launcher-jobs=20 --gtest_filter=ChromeVox*
```

### Select-To-Speak tests

```
autoninja -C out/Default unit_tests browser_tests && \
  out/Default/unit_tests --gtest_filter=*SelectToSpeak* && \
  out/Default/browser_tests --gtest_filter=*SelectToSpeak*
```

## Performance tests

We also have a page on [Performance Tests](./perf.md).

## Other locations of accessibility tests:

Even this isn't a complete list. The tests described above cover more than 90%
of the accessibility tests, and the remainder are scattered throughout the
codebase. Here are a few other locations to check:

*   [chrome/android/javatests/src/org/chromium/chrome/browser/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:chrome/android/javatests/src/org/chromium/chrome/browser/accessibility/)
*   [chrome/browser/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/accessibility/)
*   [chrome/browser/ash/accessibility/](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ash/accessibility/)
*   [ui/chromeos](https://source.chromium.org/chromium/chromium/src/+/main:ui/chromeos/)
*   [ui/views/accessibility](https://source.chromium.org/chromium/chromium/src/+/main:ui/views/accessibility/)

## Helpful flags:

Across all tests there are some helpful flags that will make testing easier.

*   Run tests multiple times in parallel (useful for finding flakes):
    `--test-launcher-jobs=10`

*   Filter which tests to run: `--gtest_filter="*Cats*"`

## Manual testing:
The Chromium accessibility pipeline supports an ecosystem of various assistive
technologies through the platform accessibility APIs. When we make changes that
modify the output of a platform accessibility interface, we should test the
major assistive technologies (JAWS, NVDA, Narrator, VoiceOver, etc.) that rely
on those interfaces to reduce the risk of introducing regressions. Below is a
list of key features to validate for each AT:

### Narrator:
Narrator relies only on UIA -- it does not use MSAA/IA2. When modifying the UIA
APIs, we should ensure that these key features still work:
* Scan Mode (toggle it with Narrator + space)
* Navigation by text unit (Narrator + CTRL + up/down arrow to change the text
  unit, then Narrator + left/right arrow to navigate backward or forward).
  Here are the text units to validate:
  * Character navigation
  * Word navigation
  * Line navigation
  * Sentence navigation
  * Paragraph navigation
  * Item navigation (as in list items, bullet points)
  * Heading navigation
  * Link navigation
  * Form field navigation
  * Landmark navigation
  * Table navigation
* Search (Narrator + CTRL + F)
* List of links (Narrator + F7)
* List of headings (Narrator + F6)
* List of landmarks (Narrator + F5)
* Caret navigation (with Scan Mode off) in a text field
  * The character that follows the caret should be read.
  * "Blank" or similar should be announced when moving the character after the
    last character of the text field.
  * Spelling and grammar errors should be announced properly when moving using
    the arrow keys.
    * For web content, errors can be added using the aria-invalid attribute or
      CSS highlights. Chromium's implementations for each of them is different
      for UIA, so both should be tested.
    * Errors should be announced when the caret is positioned at the very
      beginning of an error range. We should test arrow key navigations and word
      navigation (CTRL + right/left arrow key).
    * All errors in the same line should be announced when arrowing up/down at
      the start of a line.

This section is continuously updated. Feel free to edit it to add more core
features to test when making a potentially breaking change, or to add testing
instructions for other assistive technologies.
