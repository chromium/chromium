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
to an expectation file. The tests are driven by `DumpAccessibilityTree` testing
class.

`Node tests` are used to run a test for a single node, for example, to check
a specific property. The test loads an HTML file, waits for it to load, then
dump a single accessible node for a DOM element whose `id` or `class` attribute
is `test`. There is no support for multiple "test" nodes and the output will be
for the first match located. The tests are driven by `DumpAccessibilityNode`
testing class.

`Script tests` are used to run a script and test its output against
expectations. The tests is driven by `DumpAccessibilityScript` testing
class.

`Event tests` tests use a similar format but the events are dumped after
the document finishes loading, and an optional go() function runs. See more on
this below.

Each test is parameterized to run multiple times.  Most platforms dump in the
"blink" format (the internal data), and again in a "native" (platform-specific)
format.  The Windows platform has a second native format, "uia", so it runs a
third time.  The test name indicates which test pass was run, e.g.,
`DumpAccessibilityTreeTest.TestName/blink`.  (Note: for easier identification,
the Windows, Mac, Linux, and Android platforms rename the "native" pass to
"win", "mac", "linux" and "android", respectively.)

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
* `blink` -- representation of internal accessibility tree
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

## Directives

Directives allow you to control a test flow and test output. They can appear
anywhere but typically they're in an HTML comment block (or PDF comment block
in case of PDF tests), and must be one per line.

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

`Script tests` provide platform dependent `-SCRIPT` directive to indicate
a script to run. For example:

`MAC-SCRIPT: input.AXName`

to dump accessible name of an accessible node for a DOM element having
`input` DOM id on Mac platform. You can also you `:LINE_NUM` to indicate an
accessible object, where `LINE_NUM` is a number of a line where the accessible
object is placed at in the formatted tree.

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

#### -RUN-UNTIL-EVENT

Indicates event recording should continue at least until a specific event has
been received. This is a platform-dependent directive.

You may need to write an event test that keeps dumping events until a
specific event line. In this case, use `@WIN-RUN-UNTIL-EVENT` (or similar for
other platforms) with a substring that should occur in the event log, e.g.,
`@WIN-RUN-UNTIL-EVENT:IA2_EVENT_TEXT_CARET_MOVED`.
Note that `@*-RUN-UNTIL-EVENT` is only used in dump events tests, and not used
in dump tree tests.

If you add multiple `@*-RUN-UNTIL-EVENT` directives, the test will finish once
any of them are satisfied. Note that any other events that come along with the
last event will also be logged.

#### @DEFAULT-ACTION-ON

Invokes default action on an accessible object defined by the directive.

#### @NO_DUMP and @NO_CHILDREN_DUMP

To skip dumping a particular element, make its accessible name equal to
`@NO_DUMP`, for example `<div aria-label="@NO_DUMP"></div>`.

To skip dumping all children of a particular element, make its accessible
name equal to `@NO_CHILDREN_DUMP`, for example
`<div aria-label="@NO_CHILDREN_DUMP"></div>`.

To load an iframe from a different site, forcing it into a different process,
use `/cross-site/HOSTNAME/` in the url, for example:
`<iframe src="cross-site/1.com/accessibility/html/frame.html"></iframe>`

## Generating expectations and rebaselining:

If you want to populate the expectation file directly rather than typing it
or copying-and-pasting it, first make sure the file exists (it can be empty),
then run the test with the `--generate-accessibility-test-expectations`
argument, for example:
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

## More details on DumpAccessibilityEvents tests

These tests are similar to `DumpAccessibilityTree` tests in that they first
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
