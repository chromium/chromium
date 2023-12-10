# Writing Web Tests

[TOC]

## Overview

Web tests should be used to accomplish one of the following goals:

1. The entire surface of Blink that is exposed to the Web should be covered by
   tests that we contribute to [web-platform-tests](./web_platform_tests.md)
   (WPT). This helps us avoid regressions, and helps us identify Web Platform
   areas where the major browsers don't have interoperable implementations.
   Furthermore, by contributing to projects such as WPT, we share the burden of
   writing tests with the other browser vendors, and we help all the browsers
   get better. This is very much in line with our goal to move the Web forward.
2. When a Blink feature cannot be tested using the tools provided by WPT, and
   cannot be easily covered by
   [C++ unit tests](https://cs.chromium.org/chromium/src/third_party/blink/renderer/web/tests/?q=webframetest&sq=package:chromium&type=cs),
   the feature must be covered by web tests, to avoid unexpected regressions.
   These tests will use Blink-specific testing APIs that are only available in
   [content_shell](./web_tests_in_content_shell.md).

Note: if you are looking for a guide for the Web Platform Test, you should read
["Web platform tests"](./web_platform_tests.md) (WPT). This document does not
cover WPT specific features/behaviors. **The WPT is recommended today rather than
test types mentioned below!**

*** promo
If you know that Blink web tests are upstreamed to other projects, such as
[test262](https://github.com/tc39/test262), please update this document. Most
importantly, our guidelines should to make it easy for our tests to be
upstreamed. The
[blink-dev mailing list](https://groups.google.com/a/chromium.org/forum/#!forum/blink-dev)
will be happy to help you harmonize our current guidelines with communal test
repositories.
***

### Test Types

There are four broad types of web tests, listed in the order of preference.

* *JavaScript Tests* are the web test implementation of
  [xUnit tests](https://en.wikipedia.org/wiki/XUnit). These tests contain
  assertions written in JavaScript, and pass if the assertions evaluate to
  true.
* *Reference Tests* render a test page and a reference page, and pass if the two
  renderings are identical, according to a pixel-by-pixel comparison. These
  tests are less robust, harder to debug, and significantly slower than
  JavaScript tests, and are only used when JavaScript tests are insufficient,
  such as when testing paint code.
* *Pixel Tests* render a test page and compare the result against a pre-rendered
  baseline image in the repository. Pixel tests are less robust than the
  first two types, because the rendering of a page is influenced by
  many factors such as the host computer's graphics card and driver, the
  platform's text rendering system, and various user-configurable operating
  system settings. For this reason, it is common for a pixel test to have a
  different reference image for each platform that Blink is tested on, and
  the reference images are
  [quite cumbersome to manage](./web_test_expectations.md). You
  should only write a pixel test if you cannot use a reference test.
* *Text Tests* output pure text which represents the DOM tree, the DOM inner
  text, internal data structure of Blink like layout tree or graphics layer
  tree, or any custom text that the text wants to output. The test passes if the
  output matches a baseline text file in the repository. Text tests outputting
  internal data structures are used as a last resort to test the internal quirks
  of the implementation, and they should be avoided in favor of one of other
  options.
* *Audio tests* output audio results.

*** aside
A JavaScript test is actually a special kind of text test, but its text
baseline can be often omitted.
***

*** aside
A test can be a reference/pixel test and a text test at the same time.
***

## General Principles

Tests should be written under the assumption that they will be upstreamed
to the WPT project. To this end, tests should follow the
[WPT guidelines](https://web-platform-tests.org/writing-tests/).


There is no style guide that applies to all web tests. However, some projects
have adopted style guides, such as the
[ServiceWorker Tests Style guide](https://www.chromium.org/blink/serviceworker/testing).

Our [document on web tests tips](./web_tests_tips.md) summarizes the most
important WPT guidelines and highlights some JavaScript concepts that are worth
paying attention to when trying to infer style rules from existing tests. If
you're unopinionated and looking for a style guide to follow, the document also
suggests some defaults.

## JavaScript Tests

Whenever possible, the testing criteria should be expressed in JavaScript. The
alternatives, which will be described in future sections, result in slower and
less reliable tests.

All new JavaScript tests should be written using the
[testharness.js](https://github.com/web-platform-tests/wpt/tree/master/resources)
testing framework. This framework is used by the tests in the
[web-platform-tests](https://github.com/web-platform-tests/wpt) repository,
which is shared with all the other browser vendors, so `testharness.js` tests
are more accessible to browser developers.

See the [API documentation](https://web-platform-tests.org/writing-tests/testharness-api.html)
for a thorough introduction to `testharness.js`.

Web tests should follow the recommendations of the above documentation.
Furthermore, web tests should include relevant
[metadata](https://web-platform-tests.org/writing-tests/css-metadata.html). The
specification URL (in `<link rel="help">`) is almost always relevant, and is
incredibly helpful to a developer who needs to understand the test quickly.

Below is a skeleton for a JavaScript test embedded in an HTML page. Note that,
in order to follow the minimality guideline, the test omits the tags `<html>`,
`<head>`, and `<body>`, as they can be inferred by the HTML parser.

```html
<!doctype html>
<title>JavaScript: the true literal is immutable and equal to itself</title>
<link rel="help" href="https://tc39.github.io/ecma262/#sec-boolean-literals">
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
<script>
'use strict';

// Synchronous test example.
test(() => {
  const value = true;
  assert_true(value, 'true literal');
  assert_equals(value.toString(), 'true', 'the string representation of true');
}, 'The literal true in a synchronous test case');

// Asynchronous test example.
async_test(t => {
  const originallyTrue = true;
  setTimeout(t.step_func_done(() => {
    assert_equals(originallyTrue, true);
  }), 0);
}, 'The literal true in a setTimeout callback');

// Promise test example.
promise_test(() => {
  return new Promise((resolve, reject) => {
    resolve(true);
  }).then(value => {
    assert_true(value);
  });
}, 'The literal true used to resolve a Promise');

</script>
```

Some points that are not immediately obvious from the example:

* When calling an `assert_` function that compares two values, the first
  argument is the actual value (produced by the functionality being tested), and
  the second argument is the expected value (known good, golden). The order
  is important, because the testing harness relies on it to generate expressive
  error messages that are relied upon when debugging test failures.
* The assertion description (the string argument to `assert_` methods) conveys
  the way the actual value was obtained.
    * If the expected value doesn't make it clear, the assertion description
      should explain the desired behavior.
    * Test cases with a single assertion should omit the assertion's description
      when it is sufficiently clear.
* Each test case describes the circumstance that it tests, without being
  redundant.
    * Do not start test case descriptions with redundant terms like "Testing"
      or "Test for".
    * Test files with a single test case should omit the test case description.
      The file's `<title>` should be sufficient to describe the scenario being
      tested.
* Asynchronous tests have a few subtleties.
    * The `async_test` wrapper calls its function with a test case argument that
      is used to signal when the test case is done, and to connect assertion
      failures to the correct test.
    * `t.done()` must be called after all the test case's assertions have
      executed.
    * Test case assertions (actually, any callback code that can throw
      exceptions) must be wrapped in `t.step_func()` calls, so that
      assertion failures and exceptions can be traced back to the correct test
      case.
    * `t.step_func_done()` is a shortcut that combines `t.step_func()` with a
      `t.done()` call.

*** promo
Web tests that load from `file://` origins must currently use relative paths
to point to
[/resources/testharness.js](../../third_party/blink/web_tests/resources/testharness.js)
and
[/resources/testharnessreport.js](../../third_party/blink/web_tests/resources/testharnessreport.js).
This is contrary to the WPT guidelines, which call for absolute paths.
This limitation does not apply to the tests in `web_tests/http`, which rely on
an HTTP server, or to the tests in `web_tests/external/wpt`, which are
imported from the [WPT repository](https://github.com/web-platform-tests/wpt).
***

### WPT Supplemental Testing APIs

Some tests simply cannot be expressed using the Web Platform APIs. For example,
some tests that require a user to perform a gesture, such as a mouse click,
cannot be implemented using Web APIs. The WPT project covers some of these cases
via supplemental testing APIs.

When writing tests that rely on supplemental testing APIs, please consider the
cost and benefits of having the tests
[gracefully degrade to manual tests](./web_tests_with_manual_fallback.md) in
the absence of the testing APIs.

*** promo
In many cases, the user gesture is not actually necessary. For example, many
event handling tests can use
[synthetic events](https://developer.mozilla.org/docs/Web/Guide/Events/Creating_and_triggering_events).
***

### Relying on Blink-Specific Testing APIs

Tests that cannot be expressed using the Web Platform APIs or WPT's testing APIs
use Blink-specific testing APIs. These APIs are only available in
[content_shell](./web_tests_in_content_shell.md), and should only be used as
a last resort.

A downside of Blink-specific APIs is that they are not as well documented as the
Web Platform features. Learning to use a Blink-specific feature requires finding
other tests that use it, or reading its source code.

For example, the most popular Blink-specific API is `testRunner`, which is
implemented in
[content/web_test/renderer/test_runner.h](../../content/web_test/renderer/test_runner.h)
and
[content/web_test/renderer/test_runner.cc](../../content/web_test/renderer/test_runner.cc).
By skimming the `TestRunnerBindings::Install` method, we learn that the
testRunner API is presented by the `.testRunner` etc. objects. Reading the
`TestRunnerBindings::GetObjectTemplateBuilder` method tells us what properties
are available on the `testRunner` object.

Another popular Blink-specific API 'internals' defined in
[third_party/blink/renderer/core/testing/internals.idl](../../third_party/blink/renderer/core/testing/internals.idl)
contains more direct access to blink internals.

*** note
If possible, a test using blink-specific testing APIs should be written not to
depend on the APIs, so that it can also work directly in a browser. If the test
does need the APIs to work, it should still check if the API is available before
using the API. Note that though we omit the `window.` prefix when using the
APIs, we should use the qualified name in the `if` statement:
```javascript
  if (window.testRunner)
    testRunner.waitUntilDone();
```
***

*** note
`testRunner` is the most popular testing API because it is also used indirectly
by tests that stick to Web Platform APIs. The `testharnessreport.js` file in
`testharness.js` is specifically designated to hold glue code that connects
`testharness.js` to the testing environment. Our implementation is in
[third_party/blink/web_tests/resources/testharnessreport.js](../../third_party/blink/web_tests/resources/testharnessreport.js),
and uses the `testRunner` API.
***

See the [content/web_test/renderer/](../../content/web_test/renderer/) directory and
[WebKit's LayoutTests guide](https://trac.webkit.org/wiki/Writing%20Layout%20Tests%20for%20DumpRenderTree)
for other useful APIs. For example, `eventSender`
([content/shell/renderer/web_test/event_sender.h](../../content/web_test/renderer/event_sender.h)
and
[content/shell/renderer/web_test/event_sender.cc](../../content/web_test/renderer/event_sender.cc))
has methods that simulate events input such as keyboard / mouse input and
drag-and-drop.

Here is a UML diagram of how the `testRunner` bindings fit into Chromium.

[![UML of testRunner bindings configuring platform implementation](https://docs.google.com/drawings/u/1/d/1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg/export/svg?id=1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg&pageid=p)](https://docs.google.com/drawings/d/1KNRNjlxK0Q3Tp8rKxuuM5mpWf4OJQZmvm9_kpwu_Wwg/edit)

### Text Test Baselines

By default, all the test cases in a file that uses `testharness.js` are expected
to pass. However, in some cases, we prefer to add failing test cases to the
repository, so that we can be notified when the failure modes change (e.g., we
want to know if a test starts crashing rather than returning incorrect output).
In these situations, a test file will be accompanied by a baseline, which is an
`-expected.txt` file that contains the test's expected output.

The baselines are generated automatically when appropriate by
`run_web_tests.py`, which is described [here](./web_tests.md), and by the
[rebaselining tools](./web_test_expectations.md).

Text baselines for `testharness.js` should be avoided, as having a text baseline
associated with a `testharness.js` test usually indicates the presence of a bug.
For this reason, CLs that add text baselines must include a
[crbug.com](https://crbug.com) link for an issue tracking the removal of the
text expectations.

* When creating tests that will be upstreamed to WPT, and Blink's current
  behavior does not match the specification that is being tested, a text
  baseline is necessary. Remember to create an issue tracking the expectation's
  removal, and to link the issue in the CL description.
* Web tests that cannot be upstreamed to WPT should use JavaScript to
  document Blink's current behavior, rather than using JavaScript to document
  desired behavior and a text file to document current behavior.

*** promo
Because of [baseline fallback](./web_test_baseline_fallback.md), it may not be
possible to [represent a platform-specific all-`PASS`
status](https://crbug.com/1324638) by the platform baseline's absence. In such
rare cases, `blink_tool.py rebaseline-cl` will generate a dummy baseline
indicating to `run_web_tests.py` that all subtests are meant to pass:

```
This is a testharness.js-based test.
All subtests passed and are omitted for brevity.
See https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/writing_web_tests.md#Text-Test-Baselines for details.
Harness: the test ran to completion.
```

`blink_tool.py optimize-baselines` will automatically remove these dummy
baselines once all platforms are all-`PASS`.
***

### The js-test.js Legacy Harness

*** promo
For historical reasons, older tests are written using the `js-test` harness.
This harness is **deprecated**, and should not be used for new tests.
***

If you need to understand old tests, the best `js-test` documentation is its
implementation at
[third_party/blink/web_tests/resources/js-test.js](../../third_party/blink/web_tests/resources/js-test.js).

`js-test` tests lean heavily on the Blink-specific `testRunner` testing API.
In a nutshell, the tests call `testRunner.dumpAsText()` to signal that the page
content should be dumped and compared against a text baseline (an
`-expected.txt` file). As a consequence, `js-test` tests are always accompanied
by text baselines. Asynchronous tests also use `testRunner.waitUntilDone()` and
`testRunner.notifyDone()` to tell the testing tools when they are complete.

### Tests that use an HTTP Server

By default, tests are loaded as if via `file:` URLs. Some web platform features
require tests served via HTTP or HTTPS, for example absolute paths (`src=/foo`)
or features restricted to secure protocols.

HTTP tests are those under `web_tests/http/tests` (or virtual variants). Use a
locally running HTTP server (Apache) to run them. Tests are served off of ports
8000 and 8080 for HTTP, and 8443 for HTTPS. If you run the tests using
`run_web_tests.py`, the server will be started automatically. To run the server
manually to reproduce or debug a failure:

```bash
cd src/third_party/blink/tools
./run_blink_httpd.py
```

The web tests will be served from `http://127.0.0.1:8000`. For example, to
run the test `http/tests/serviceworker/chromium/service-worker-allowed.html`,
navigate to
`http://127.0.0.1:8000/serviceworker/chromium/service-worker-allowed.html`. Some
tests will behave differently if you go to 127.0.0.1 instead of localhost, so
use 127.0.0.1.

To kill the server, hit any key on the terminal where `run_blink_httpd.py` is
running, or just use `taskkill` or the Task Manager on Windows, and `killall` or
Activity Monitor on MacOS.

The test server sets up an alias to the `web_tests/resources` directory. In
HTTP tests, you can access the testing framework at e.g.
`src="/resources/testharness.js"`.

TODO: Document [wptserve](http://wptserve.readthedocs.io/) when we are in a
position to use it to run web tests.

## Reference Tests (Reftests)

Reference tests, also known as reftests, perform a pixel-by-pixel comparison
between the rendered image of a test page and the rendered image of a reference
page. Most reference tests pass if the two images match, but there are cases
where it is useful to have a test pass when the two images do _not_ match.

Reference tests are more difficult to debug than JavaScript tests, and tend to
be slower as well. Therefore, they should only be used for functionality that
cannot be covered by JavaScript tests.

New reference tests should follow the
[WPT reftests guidelines](https://web-platform-tests.org/writing-tests/reftests.html).
The most important points are summarized below.

* &#x1F6A7; The test page declares the reference page using a
  `<link rel="match">` or `<link rel="mismatch">`, depending on whether the test
  passes when the test image matches or does not match the reference image.
* The reference page must not use the feature being tested. Otherwise, the test
  is meaningless.
* The reference page should be as simple as possible, and should not depend on
  advanced features. Ideally, the reference page should render as intended even
  on browsers with poor CSS support.
* Reference tests should be self-describing.
* Reference tests do _not_ include `testharness.js`.

&#x1F6A7; Our testing infrastructure was designed for the
[WebKit reftests](https://trac.webkit.org/wiki/Writing%20Reftests) that Blink
has inherited. The consequences are summarized below.

* Each reference page must be in the same directory as its associated test.
  Given a test page named `foo` (e.g. `foo.html` or `foo.svg`),
    * The reference page must be named `foo-expected` (e.g.,
      `foo-expected.html`) if the test passes when the two images match.
    * The reference page must be named `foo-expected-mismatch` (e.g.,
      `foo-expected-mismatch.svg`) if the test passes when the two images do
      _not_ match.
* Multiple references and chained references are not supported.

The following example demonstrates a reference test for
[`<ol>`'s reversed attribute](https://developer.mozilla.org/en-US/docs/Web/HTML/Element/ol).
The example assumes that the test page is named `ol-reversed.html`.

```html
<!doctype html>
<link rel="match" href="ol-reversed-expected.html">

<ol reversed>
  <li>A</li>
  <li>B</li>
  <li>C</li>
</ol>
```

The reference page, which must be named `ol-reversed-expected.html`, is below.

```html
<!doctype html>

<ol>
  <li value="3">A</li>
  <li value="2">B</li>
  <li value="1">C</li>
</ol>
```

*** promo
The method for pointing out a test's reference page is still in flux, and is
being discussed on
[blink-dev](https://groups.google.com/a/chromium.org/d/topic/blink-dev/XsR6PKRrS1E/discussion).
***

## Pixel Tests

A test creates an image result by default unless some `testRunner` API is
called (e.g. `testRunner.dumpAsText()`, `testRunner.dumpAsLayout()`, see
[text tests](#text-tests)) to suppress the image result. A test is a
**pixel test** if it creates an image result but is not a reference test.
The image result is compared against an image baseline, which is an
`-expected.png` file associated with the test, and the test passes if the
image result is identical to the baseline, according to a pixel-by-pixel
comparison.

Pixel tests should still follow the principles laid out above. Pixel tests pose
unique challenges to the desire to have *self-describing* and *cross-platform*
tests. The
[WPT rendering test guidelines](https://web-platform-tests.org/writing-tests/rendering.html)
contain useful guidance. The most relevant pieces of advice are below.

* Whenever possible, use a green paragraph / page / square to indicate success.
  If that is not possible, make the test self-describing by including a textual
  description of the desired (passing) outcome.
* Only use the red color or the word `FAIL` to highlight errors. This does not
  apply when testing the color red.
* &#x1F6A7; Use the
  [Ahem font](https://www.w3.org/Style/CSS/Test/Fonts/Ahem/README) to reduce the
  variance introduced by the platform's text rendering system. This does not
  apply when testing text, text flow, font selection, font fallback, font
  features, or other typographic information.

*** promo
The default size of the image result of a pixel test is 800x600px, because test
pages are rendered in an 800x600px viewport by default. Normally pixel tests
that do not specifically cover scrolling should fit in an 800x600px viewport
without creating scrollbars.
***

*** promo
The recommendation of using Ahem in pixel tests is being discussed on
[blink-dev](https://groups.google.com/a/chromium.org/d/topic/blink-dev/XsR6PKRrS1E/discussion).
***

The following snippet includes the Ahem font in a web test.

```html
<style>
body {
  font: 10px Ahem;
}
</style>
<script src="/resources/ahem.js"></script>
```

*** promo
Tests outside `web_tests/http` and `web_tests/external/wpt` currently need
to use a relative path to
[/third_party/blink/web_tests/resources/ahem.js](../../third_party/blink/web_tests/resources/ahem.js)
***

### Tests that need to paint, raster, or draw a frame of intermediate output

A web test does not actually draw frames of output until the test exits.
Tests that need to generate a painted frame can use `runAfterLayoutAndPaint()`
defined in [third_party/blink/web_tests/resources/run-after-layout-and-paint.js](../../third_party/blink/web_tests/resources/run-after-layout-and-paint.js)
which will run the machinery to put up a frame, then call the passed callback.
There is also a library at
[third_party/blink/web_tests/paint/invalidation/resources/text-based-repaint.js](../../third_party/blink/web_tests/paint/invalidation/resources/text-based-repaint.js)
to help with writing paint invalidation and repaint tests.

### Tests for scrolling animations

Some web tests need to ensure animations such as middle-click auto-scroll,
fling, etc. get performed properly. When testing in display compositor pixel
dump mode (now the standard), the standard behavior for tests is to
synchronously composite without rastering (to save time). However, animations
run upon surface activation, which only happens once rasterization is performed.
Therefore, for these tests, an additional setting needs to be set. Near the
beginning of these tests, call `setAnimationRequiresRaster()` defined in
[third_party/blink/web_tests/resources/compositor-controls.js](../../third_party/blink/web_tests/resources/compositor-controls.js)
which will enable full rasterization during the test.

## Text tests

A **text test** outputs text result. The result is compared against a text
baseline which is an `-expected.txt` file associated with the test, and the
test passes if the text result is identical to the baseline. A test isn't a
text test by default until it calls some `testRunner` API to instruct the
test runner to output text. A text test can be categorized based on what kind of
information that the text result represents.

### Layout tree test

If a test calls `testRunner.dumpAsLayout()` or
`testRunner.dumpAsLayoutWithPixelResults()`, The text result will be a
textual representation of Blink's
[layout tree](https://developers.google.com/web/fundamentals/performance/critical-rendering-path/render-tree-construction)
(called the render tree on that page) of the main frame of the test page.
With `testRunner.dumpChildFrames()` the text result will also include layout
tree of child frames.

Like pixel tests, the output of layout tree tests may depend on
platform-specific details, so layout tree tests often require per-platform
baselines. Furthermore, since the tests obviously depend on the layout tree
structure, that means that if we change the layout tree you have to rebaseline
each layout tree test to see if the results are still correct and whether the
test is still meaningful. There are actually many cases where the layout tree
output is misstated (i.e., wrong), because people didn't want to have to update
existing baselines and tests. This is really unfortunate and confusing.

For these reasons, layout tree tests should **only** be used to cover aspects
of the layout code that can only be tested by looking at the layout tree. Any
combination of the other test types is preferable to a layout tree test.
Layout tree tests are
[inherited from WebKit](https://webkit.org/blog/1456/layout-tests-practice/), so
the repository may have some unfortunate examples of layout tree tests.

The following page is an example of a layout tree test.

```html
<!doctype html>
<style>
body { font: 10px Ahem; }
span::after {
  content: "pass";
  color: green;
}
</style>
<script src="/resources/ahem.js"></script>
<script>
  if (window.testRunner)
    testRunner.dumpAsLayout();
</script>
<p><span>Pass if a green PASS appears to the right: </span></p>
```

The test page produces the text result below.

```
layer at (0,0) size 800x600
  LayoutView at (0,0) size 800x600
layer at (0,0) size 800x30
  LayoutBlockFlow {HTML} at (0,0) size 800x30
    LayoutBlockFlow {BODY} at (8,10) size 784x10
      LayoutBlockFlow {P} at (0,0) size 784x10
        LayoutInline {SPAN} at (0,0) size 470x10
          LayoutText {#text} at (0,0) size 430x10
            text run at (0,0) width 430: "Pass if a green PASS appears to the right: "
          LayoutInline {<pseudo:after>} at (0,0) size 40x10 [color=#008000]
            LayoutTextFragment (anonymous) at (430,0) size 40x10
              text run at (430,0) width 40: "pass"
```

Notice that the test result above depends on the size of the `<p>` text. The
test page uses the Ahem font (introduced above), whose main design goal is
consistent cross-platform rendering. Had the test used another font, its text
baseline would have depended on the fonts installed on the testing computer, and
on the platform's font rendering system. Please follow the pixel tests
guidelines and write reliable layout tree tests!

WebKit's layout tree is described in
[a series of posts](https://webkit.org/blog/114/webcore-rendering-i-the-basics/)
on WebKit's blog. Some of the concepts there still apply to Blink's layout tree.

### Text dump test

If `testRunner.dumpAsText()` or `testRunner.dumpAsTextWithPixelResults()`
is called from a test, the test will dump the text contents of the main frame
of the tested page. With `testRunner.dumpChildFrames()` the text
result will also include text contents of child frames. Actually a JavaScript
test is a special kind of text dump test which can often omit the text baseline.

A test can override the default text dump by calling
`testRunner.setCustomTextOutput(string)`. The string parameter can be any
text that the test wants to output. The [`internals` API](../../third_party/blink/renderer/core/testing/internals.idl]
provides methods to get textual representations of internal data structures that
can be used as the parameter of `testRunner.setCustomTextOutput()`.

### Markup dump test

If a test calls `testRunner.dumpAsMarkup()`, the text result will be the DOM
of the main frame of the test. With `testRunner.dumpChildFrames()` the text
result will also include DOM of child frames.

## Audio tests

If a test calls `testRunner.setAudioData(array_buffer)`, the test will
create an audio result. The result will be compared against an audio baseline
which is an `-expected.wav` file associated with the test, and the test passes
if the audio result is identical to the baseline.

## Tests that are both pixel/reference tests and text tests

If a test calls `testRunner.dumpAsTextWithPixelResults()` or
`testRunner.dumpAsLayoutWithPixelResults()`, the test is both a
pixel/reference test and a text test. It will output both pixel result and text
result.

For a test that is both a pixel/reference test and a text test, both pixel and
text results will be compared to baselines, and the test passes if each result
matches the corresponding baseline.

Many of the [paint invalidation tests](../../third_party/blink/web_tests/paint/invalidation)
are of this type. The pixel results (compared against `-expected.png` or
`-expected.html`) ensure correct rendering, and the text results (compared
against `-expected.txt`) ensure correct compositing and raster invalidation
(without unexpected over and under invalidations).

For a layout tree test, whether you want a pixel test and/or a text test depends
on whether you care about the visual image, the details of how that image was
constructed, or both. It is possible for multiple layout trees to produce
the same pixel output, so it is important to make it clear in the test
which outputs you really care about.

## Directory Structure

The [web_tests directory](../../third_party/blink/web_tests) currently
lacks a strict, formal structure. The following directories have special
meaning:

* The `http/` directory hosts tests that require an HTTP server (see above).
* The `resources/` subdirectory in every directory contains binary files, such
  as media files, and code that is shared by multiple test files.

*** note
Some web tests consist of a minimal HTML page that references a JavaScript
file in `resources/`. Please do not use this pattern for new tests, as it goes
against the minimality principle. JavaScript and CSS files should only live in
`resources/` if they are shared by at least two test files.
***
