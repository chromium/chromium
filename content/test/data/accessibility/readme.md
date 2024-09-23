# Dump Accessibility Tests

There are several types of dump accessibility tests:

* `tree tests` to test accessible trees;
* `node tests` to test a single accessible node;
* `script tests` to run a script and compare its output to expected results;
* `event tests` to test accessible events.

All these tests are backed by the same engine and linked with the same idea:
a test generates an output and then the output gets compared to expected
results. Such approach has a great benefit: the tests can be rebaselined easily
by making a test to generate expectations itself.

`Tree tests` are designed to test accessible tree. It loads an HTML file, waits
for it to load, then dumps the accessible tree. The dumped tree is compared
to an expectation file. The tests are driven by `DumpAccessibilityTreeTest`
testing class.

`Node tests` are used to run a test for a single node, for example, to check
a specific property. The test loads an HTML file, waits for it to load, then
dump a single accessible node for a DOM element whose `id` or `class` attribute
is `test`. There is no support for multiple "test" nodes and the output will be
for the first match located. The tests are driven by `DumpAccessibilityNodeTest`
testing class.

`Script tests` are used to run a script and test its output against
expectations. The tests is driven by `DumpAccessibilityScriptTest` testing
class.

`Event tests` tests use a similar format but the events are dumped after
the document finishes loading, and an optional go() function runs. See more on
this below.

Each test is parameterized to run multiple times.  Most platforms dump in the
"blink" format (the internal data), and again in a "native" (platform-specific)
format.  The Windows platform has two native formats, "uia" and "ia2", so the
test is run three times.  The test name indicates which test pass was run, e.g.,
`DumpAccessibilityTreeTest.TestName/blink`.  (Note: for easier identification,
the Windows, Mac, Linux, and Android platforms rename the "native" pass to
"ia2", "mac", "linux" and "android", respectively.)

The test output is a compact text representation of the accessible node(s)
for that format, and it should be familiar if you're familiar with the
accessibility protocol on that platform, but it's not in a standardized
format - it's just a text dump, meant to be compared to expected output.

The exact output can be filtered so it only dumps the specific attributes
you care about for a specific test.

Once the output has been generated, it compares the output to an expectation
file in the same directory. If an expectation file for that test for that
platform is present, it must match exactly or the test fails. If no
expectation file is present, the test passes. Most tests don't have
expectations on all platforms.

## Compiling and running the tests

Most of the tests are hosted by `content_browsertests` testsuite.
```
autoninja -C out/Debug content_browsertests
out/Debug/content_browsertests --gtest_filter="All/DumpAccessibility*"
```

PDF accessibility tests though are running under `browsertest` testsuite.

```
autoninja -C out/Default browser_tests
out/Default/browser_tests --gtest_filter="PDFExtensionAccessibilityTreeDumpTest*"
```

## File structure

* `foo.html` -- a file to be tested
* `foo-expected-[platform].txt` -- a file to be tested
* `foo-node-expected-[platform].txt` - a a file containing a node test
   expectations

Supported platforms are:
* `android` -- expected Android AccessibilityNodeInfo output
* `auralinux` -- expected Linux ATK output
* `auralinux-xenial` -- expected Linux ATK output (Version Specific Expected File)
* `blink` -- representation of internal accessibility tree
* `blink-cros` -- representation of internal accessibility tree
  (Version Specific Expected File for Chrome OS and Lacros)
* `mac` -- expected Mac NSAccessibility output
* `win` -- expected Win IAccessible/IAccessible2 output
* `uia-win` -- expected Win UIA output
* `uia-win7` -- expected Win7 UIA output (Version Specific Expected File)

Note, a single HTML test files can be used both for `tree tests` and
`node tests`. In this case the expectations files for `node tests` portion will
have an expectations-file qualifier of `-node` inserted immediately before
`expected`. Thus for `foo.html`, there could be:

* `foo-expected-mac.txt` -- expected Mac NSAccessibility output for the
  entire accessibility tree
* `foo-node-expected-mac.txt` -- expected Mac NSAccessibility output for
  just the node in `foo.html` whose `class` is `test`

## Expected files format

* Blank lines and lines beginning with `#` are ignored
* Skipped files: if first line of file begins with `#<skip` then the
  test passes. This can be used to indicate desired output with a link
  to a bug, or as a way to temporarily disable a test during refactoring.
* Use 2 plus signs for indent to show hierarchy

## Version Specific Expected Files

UIA sometimes differs between windows 7 and later versions of
Windows. To account for these differences, the UIA accessibility
tree formatter will look for a version specific expected file first:
`foo-expected-uia-win7.txt`. If the version specific expected file
does not exist, the normal expected file will be used instead:
"`foo-expected-uia-win.txt`". There is no concept of version
specific filters.

In the case of Linux, the tests are run on several LTS
[releases](https://releases.ubuntu.com/) of Ubuntu:

* "Xenial Xerus": Ubuntu 16.04, ATK version 2.18 (bot: "linux-xenial-rel")
* "Bionic Beaver": Ubuntu 18.04, ATK version 2.28 (runs on multiple bots)

In many cases the expected results for `foo.html` will be the same for all
versions of Ubuntu, in which case `foo-expected-auralinux.txt` is all that is
needed. However, if the `foo.html` test passes on the Linux release build
("linux-rel"), but fails on "linux-xenial-rel", you will need an additional
`foo-expected-auralinux-xenial.txt` file.

At the present time there is no version-specific support for Bionic Beaver,
which is the current version run on "linux-rel".

The need for a version-specific expectations file on Chrome OS / Lacros is
extremely rare. However, there can be occasional differences in the internal
accessibility tree. For instance, the SVG `g` element is always included
in order to support select-to-speak functionality. If `foo.html` has a
`foo-expected-blink.txt` file which works on all platforms except the Chrome OS
and Lacros bots, create `foo-expected-blink-cros.txt`.

## Directives

Directives allow you to control test flow and test output. The directives are
defined inside the first comment block in the test's input file, one directive
per line. For example, in the case of an HTML file the directives are located in
between `<!--` and `-->`, in the case of a PDF file the directives are
preceding by `%` character designating a comment.

Directives have format of `@directive_name:directive_value`. Directives can be
spawned over multiple lines:
```
@directive_name:
  directive_value
  directive_value
```

Certain directives are platform dependent. If so, then such directives are
prefixed by a platform name:
* `@WIN-` applied to Windows platform, MSAA/IAccessible2 APIs;
* `@UIA-WIN-` applied to UIA on Windows;
* `@MAC-` applied to Mac platform, NSAccessibility API;
* `@BLINK-` applied to Chromium engine;
* `@ANDROID-` applied to Android platform;
* `@AURALINUX-` applied to Linux platform, ATK API.

### Filters

By default only some attributes of nodes in the accessibility tree, or
events fired (when running `event tests`), are output.
This is to keep the tests robust and not prone to failure when unrelated
changes affect the accessibility tree in unimportant ways.

You can use these filter types to match the attributes and/or attribute values
you want included in the output.

* `-ALLOW` filter means to include the attribute having non empty values;
* `-ALLOW-EMPTY` filter means to include the attribute even if its value is empty;
* `-DENY` filter means to exclude an attribute.

Filter directives are platform-dependent (see above).

Filters can contain simple wildcards (`*`) only, they're not
regular expressions. Examples:

* `@WIN-ALLOW:name` will output the `name` attribute on Windows
* `@WIN-ALLOW:name='Foo'` will only output the name attribute if it exactly
matches 'Foo'.
* `@WIN-DENY:name='X*` will skip outputting any name that begins with the letter X.
* `@WIN-ALLOW:*` to dump all attributes, useful for debuggin a test.

### Scripting

Note: Mac platform is supported only.

`Script tests` provide platform dependent `-SCRIPT` directive to indicate
a script to run. For example:

`@MAC-SCRIPT: input.AXName`

to dump accessible name of an accessible node for a DOM element having
`input` DOM id on Mac platform. You can also use `:LINE_NUM` syntax to indicate
an accessible object, where `LINE_NUM` is index of a line where
the accessible object is placed in the formatted tree. However you should avoid
using `:LINE_NUM` in a test as it may break the test automatic rebaseling.

You can put multiple instructions under the same `@MAC-SCRIPT` directive, for
example:
```
@MAC-SCRIPT:
  input.AXRole
  input.AXName
```

Calls can be chained. For example:

`input.AXFocusableAncestor.AXRole`

Note: The `.AXAttribute` will dump the accessible attribute for the node only
if the attribute is supported for that node.

To test for the support of the attribute in mac accessibility API, you can see
if the attribute is included in the accessibilityAttribute names using
`has()`. For example, the following will tell you whether the attribute
`AXInvalid` is supported on an accessible node, regardless of whether the
attribute has been provided by the web author.

```
@MAC-SCRIPT:
  input.accessibilityAttributeNames.has(AXInvalid)
```

Parameterized attributes are also supported. For example:

`paragraph.AXTextMarkerForIndex(0)`

`AXTextMarker` is serialized as a `{:LINE_NUM, offset, direction}` triple, for
example:
`textarea.AXPreviousWordStartTextMarkerForTextMarker({:3, 3, down})`

`AXTextMarkerRange` is serialized as a dictionary:
`{anchor: TEXT_MARKER, focus: TEXT_MARKER}`.

You can also retrieve `anchor` and `focus` text markers from a text marker range,
for example:

`p.AXTextMarkerRangeForUIElement(p).anchor` or
`p.AXTextMarkerRangeForUIElement(p).focus`

You can also use array operator[] to refer to an array element at a given index,
for example `paragraph.AXChildren[0]` will refer to the first child of the paragraph.

To set a settable attribute you can assign a value to the attribute. For example:
```
textarea_range:= textarea.AXTextMarkerRangeForUIElement(textarea)
textarea.AXSelectedTextMarkerRange = textarea_range
```

To pass a SEL as argument, you need to use the "@SEL:" prefix. For example:
```
@SCRIPT:
  slider.isAccessibilitySelectorAllowed(@SEL:setAccessibilityValue:)
```

You can use the `wait for` instruction to wait for a specific event before
the script scontinues. For example:

```
@SCRIPT:
  button.AXPerformAction(AXPress)
  wait for AXFocusedUIElementChanged
```

will trigger `AXPress` action on a button and will wait for
`AXFocusedUIElementChanged` event. You can also be more specific if you want to
and provide the event target. For example:
`wait for AXFocusedUIElementChanged on AXButton`

You can use `press` instruction to simulate key events.
The instruction accepts a single parameter which could be a character or
a key name (as specified in http://www.w3.org/TR/DOM-Level-3-Events-key/).
For example,
```
@SCRIPT:
  press Enter
  wait for AXValueChanged
```

You can use `print tree` to print a snapshot of an accessible tree. For example,
```
@SCRIPT:
  print tree
```

### Advanced directives

Normally the system waits for the document to finish loading before running
the test. You can tune the behavior up by the following directives.

#### @NO-LOAD-EXPECTED

Instructs to not wait for document load for url defined by the directive.

If you do not expect an iframe or object to load, (e.g. testing fallback), you
can use the `@NO-LOAD-EXPECTED:` to cause the test to not wait for that frame to
finish loading. For example the test would not wait for a url containing
"broken.jpg" to load:
`@NO-LOAD-EXPECTED:broken.jpg`
`<object data="./broken.jpg">Fallback</object`

#### @WAIT-FOR

Delays a test unitl a string defined by the directive is present in the dump.

Occasionally you may need to write a dump tree test that makes some changes to
the document before it runs the test. In that case you can use a special
`@WAIT-FOR:` directive. It should be in an HTML comment, just like
`@ALLOW-WIN:` directives. The `WAIT-FOR` directive just specifies a text
substring that should be present in the dump when the document is ready. The
system will keep blocking until that text appears.

You can add as many `@WAIT-FOR:` directives as you want, the test won't finish
until all strings appear.

#### @EXECUTE-AND-WAIT-FOR

Delays a test until a string returned by a script defined by the directive is
present in the dump.

You may also want to execute script and then capture a dump. Rather than use
`setTimeout` and `@WAIT-FOR:`, consider using the `@EXECUTE-AND-WAIT-FOR:`
directive. This directive expects a javascript function that returns a string to
wait for. If a string is not returned, the tree dumper will not wait.
`@EXECUTE-AND-WAIT-FOR:` directives are executed in order, after the document is
ready and all `@WAIT-FOR:` strings have been found.
Example: `@EXECUTE-AND-WAIT-FOR: foo()`

#### @DEFAULT-ACTION-ON

Invokes default action on an accessible object defined by the directive.

#### @NO_DUMP and @NO_CHILDREN_DUMP

To skip dumping a particular element, add `@NO_DUMP` to a property that will
be exposed as an ax::mojom::StringAttribute. For example
`<div class="@NO_DUMP"></div>`.

To skip dumping all children of a particular element, add `@NO_CHILDREN_DUMP`
to a property that will be exposed as an ax::mojom::StringAttribute. For example
`<div class="@NO_CHILDREN_DUMP"></div>`.

Note that setting the `aria-label` value to `@NO_DUMP` or `@NO_CHILDREN_DUMP`
is not guaranteed to work due to certain roles no longer supporting author-
provided naming in ARIA 1.2.

To load an iframe from a different site, forcing it into a different process,
use `/cross-site/HOSTNAME/` in the url. For example:
`<iframe src="cross-site/1.com/accessibility/html/frame.html"></iframe>`

## Generating expectations and rebaselining:

If you want to populate the expectation file directly rather than typing it
or copying-and-pasting it, first make sure the file exists (it can be empty),
then run the test with the `--generate-accessibility-test-expectations`
argument. For example:
```
  out/Debug/content_browsertests \
    --generate-accessibility-test-expectations \
    --gtest_filter="All/DumpAccessibilityTreeTest.AccessibilityAriaAtomic/*"
```
This will replace the `-expected-*.txt` file with the current output. It's
a great way to rebaseline a bunch of tests after making a change. Please
manually check the diff, of course!

The * is a wildcard and will match any substring, in this case all platforms.
To run on a single platform, replace the wildcard, e.g.:
```
  --gtest_filter="All/DumpAccessibilityTreeTest.AccessibilityAriaAtomic/linux"
```

To rebaseline all OSes at once, use:

```
tools/accessibility/rebase_dump_accessibility_tree_tests.py
```

For more information, see the detailed help with:
```
  out/Debug/content_browsertests --gtest_help
```

Note: For Android, generated expectations will replace the existing files on
the test device. For example, if running on an emulator, for an ARIA test
called `my-test.html`, the generated output can be found:
```
  /storage/emulated/0/chromium_tests_root/content/test/
     data/accessibility/aria/my-test-expected-android.txt
```

## Adding a new test

If you are adding a new test file remember to add a corresponding test case in:
* `content/browser/accessibility/dump_accessibility_events_browsertest.cc`; or
* `content/browser/accessibility/dump_accessibility_tree_browsertest.cc`

If you are adding a new events test, remember to add a corresponding test case
for Android, see more info below.

## More details on DumpAccessibilityEventsTest tests

These tests are similar to `DumpAccessibilityTreeTest` tests in that they first
load an HTML document, then dump something, then compare the output to
an expectation file. The difference is that what's dumped is accessibility
events that are fired.

To write a test for accessibility events, your document must contain a
JavaScript function called `go()`. This function will be called when the
document is loaded (or when the `@WAIT_FOR` directive passes), and any
subsequent events will be dumped. Filters apply to events just like in tree
dumps.

After calling `go()`, the system asks the page to generate a sentinel
accessibility event - one you're unlikely to generate in your test. It uses
that event to know when to "stop" dumping events. There isn't currently a
way to test events that occur after some delay, just ones that happen as
a direct result of calling `go()`.

### Duplicate Events on UIA
Windows will "translate" some IA2 events to UIA, and it is not
possible to turn this feature off. Therefore as our UIA behavior is in addition
to IA2, we will receive duplicated events for Focus, MenuOpened and MenuClosed.

### Including Tests for Android

The Android `DumpAccessibilityEventsTests` tests work differently than the other
platforms and are driven by the Java-side code. The tests all reside in the
[WebContentsAccessibilityEventsTest.java](https://source.chromium.org/chromium/chromium/src/+/main:content/public/android/javatests/src/org/chromium/content/browser/accessibility/WebContentsAccessibilityEventsTest.java)
class. The tests are controlled from the Java code so that they can leverage the
full accessibility suite and test the
[AccessibilityEvents](https://developer.android.com/reference/android/view/accessibility/AccessibilityEvent)
that are sent to downstream services. For this to work, when adding a new events
test, you must include a test line in the Java class.

Example: If you are adding a new events test, "example-test.html", you would
first create the html file as normal (content/test/data/accessibility/event/example-test.html),
and add the test to the existing `dump_accessibility_events_browsertests.cc`:

```
IN_PROC_BROWSER_TEST_P(DumpAccessibilityEventsTest, AccessibilityEventsExampleTest) {
  RunEventTest(FILE_PATH_LITERAL("example-test.html"));
}
```

To include this test on Android, you would add a similar block to the
`WebContentsAccessibilityEventsTest.java` class:

```
@Test
@SmallTest
public void test_exampleTest() {
    performTest("example-test.html", "example-test-expected-android.txt");
}
```

Some tests on Android won't produce any events. For these you do not need to
create an empty file, but can instead make the test line:

```
    performTest("example-test.html", EMPTY_EXPECTATIONS_FILE);
```

The easiest approach is to use the above line, run the tests, and if it fails,
the error message will give you the exact text to add to the
`-expected-android.txt` file. The `-expected-android.txt` file should go in the
same directory as the others (content/test/data/accessibility/event).

A PRESUBMIT check will give a non-blocking warning if you are adding, renaming,
or deleting an events test without a corresponding change for Android.
