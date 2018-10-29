# DumpAccessibilityTreeTest and DumpAccessibilityEventsTest Notes:

Both sets of tests use a similar format for files.

DumpAccessibilityTree tests load an HTML file, wait for it to load, then
dump the accessibility tree in the "blink" format (the internal data),
and again in a platform-specific format.

The test output is a compact text representation of the accessibility tree
for that format, and it should be familiar if you're familiar with the
accessibility protocol on that platform, but it's not in a standardized
format - it's just a text dump, meant to be compared to expected output.

The exact output can be filtered so it only dumps the specific attributes
you care about for a specific test.

One the output has been generated, it compares the output to an expectation
file in the same directory. If an expectation file for that test for that
platform is present, it must match exactly or the test fails. If no
expectation file is present, the test passes. Most tests don't have
expectations on all platforms.

DumpAccessibilityEvent tests use a similar format but dump events fired after
the document finishes loading. See more on this below.

Compiling and running the tests:
ninja -C out/Debug content_browsertests
out/Debug/content_browsertests --gtest_filter="DumpAccessibility*"

Files used:

* foo.html -- a file to be tested
* foo-expected-android.txt -- expected Android AccessibilityNodeInfo output
* foo-expected-auralinux.txt -- expected Linux ATK output
* foo-expected-blink.txt -- representation of internal accessibility tree
* foo-expected-mac.txt -- expected Mac NSAccessibility output
* foo-expected-win.txt -- expected Win IAccessible/IAccessible2 output

Format for expected files:

* Blank lines and lines beginning with # are ignored
* Skipped files: if first line of file begins with #<skip then the
  test passes. This can be used to indicate desired output with a link
  to a bug, or as a way to temporarily disable a test during refactoring.
* Use 2 plus signs for indent to show hierarchy

Filters:

* By default only some attributes of nodes in the accessibility tree, or
  events fired (when running DumpAccessibilityEvents) are output.
  This is to keep the tests robust and not prone to failure when unrelated
  changes affect the accessibility tree in unimportant ways.
* Filters contained in the HTML file can be used to control what is output.
  They can appear anywhere but typically they're in an HTML comment block,
  and must be one per line.
* Filters are platform-specific:
```
    @WIN-
    @MAC-
    @BLINK-
    @ANDROID-
    @AURALINUX-
```
* To dump all attributes while writing or debugging a test, add this filter:
    @WIN-ALLOW:*
  (and similarly for other platforms).
* Once you know what you want to output, you can use filters to match the
  attributes and attribute values you want included in the output. An
  ALLOW filter means to include the attribute, and a DENY filter means to
  exclude it. Filters can contain simple wildcards ('*') only, they're not
  regular expressions. Examples:
```
  - @WIN-ALLOW:name* - this will output the name attribute on Windows
  - @WIN-ALLOW:name='Foo' - this will only output the name attribute if it
      exactly matches 'Foo'.
  - @WIN-DENY:name='X* - this will skip outputting any name that begins with
      the letter X.
```
* By default empty attributes are skipped. To output the value an attribute
  even if it's empty, use @WIN-ALLOW-EMPTY:name, for example, and similarly
  for other platforms.

## Advanced:

Normally the system waits for the document to finish loading before dumping
the accessibility tree.

Occasionally you may need to write a dump tree test that makes some changes to
the document before it runs the test. In that case you can use a special
@WAIT-FOR: directive. It should be in an HTML comment, just like
@ALLOW-WIN: directives. The WAIT-FOR directive just specifies a text substring
that should be present in the dump when the document is ready. The system
will keep blocking until that text appears.

You can add as many @WAIT-FOR: directives as you want, the test won't finish
until all strings appear.

Or, you may need to write an event test that keeps dumping events until a
specific event line. In this case, use @RUN-UNTIL-EVENT with a substring that
should occur in the event log, e.g. @RUN-UNTIL-EVENT:IA2_EVENT_TEXT_CARET_MOVED.
Note that @RUN-UNTIL-EVENT is only used in dump events tests, and not used in
dump tree tests.

If you add multiple @RUN-UNTIL-EVENT directives, the test will finish once any
of them are satisfied. Note that any other events that come along with the last
event will also be logged.

To skip dumping a particular element, make its accessible name equal to
@NO_DUMP, for example <div aria-label="@NO_DUMP"></div>.

To skip dumping all children of a particular element, make its accessible
name equal to @NO_CHILDREN_DUMP.

To load an iframe from a different site, forcing it into a different process,
use /cross-site/HOSTNAME/ in the url, for example:
  <iframe src="cross-site/1.com/accessibility/html/frame.html"></iframe>

## Generating expectations and rebaselining:

If you want to populate the expectation file directly rather than typing it
or copying-and-pasting it, first make sure the file exists (it can be empty),
then run the test with the --generate-accessibility-test-expectations
argument, for example:

  out/Debug/content_browsertests \
    --generate-accessibility-test-expectations
    --gtest_filter="DumpAccessibilityTreeTest.AccessibilityA"

This will replace the -expected-*.txt file with the current output. It's
a great way to rebaseline a bunch of tests after making a change. Please
manually check the diff, of course!

## Adding a new test:

If you are adding a new test file remember to add a corresponding test case in
content/browser/accessibility/dump_accessibility_events_browsertest.cc
or
content/browser/accessibility/dump_accessibility_tree_browsertest.cc

More details on DumpAccessibilityEvents tests:

These tests are similar to DumpAccessibilityTree tests in that they first
load an HTML document, then dump something, then compare the output to
an expectation file. The difference is that what's dumped is accessibility
events that are fired.

To write a test for accessibility events, your document must contain a
JavaScript function called go(). This function will be called when the document
is loaded (or when the @WAIT_FOR directive passes), and any subsequent
events will be dumped. Filters apply to events just like in tree dumps.

After calling go(), the system asks the page to generate a sentinel
accessibility event - one you're unlikely to generate in your test. It uses
that event to know when to "stop" dumping events. There isn't currently a
way to test events that occur after some delay, just ones that happen as
a direct result of calling go().
