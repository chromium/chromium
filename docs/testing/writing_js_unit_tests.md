<!--* freshness: { owner: 'olsen' reviewed: '2019-06-07' } *-->

<!-- Originally published June 7, 2019 -->

# Writing a JavaScript unit test

You have written some JS code and put it somewhere in chromium src. Now you want
to write unit tests for it.

[TOC]

## Testing JS code with no dependencies

Let's say you have written a file `awesome.js`. Next you will write your unit
tests in a file in the same directory called `awesome_test.unitjs`. The
`.unitjs` suffix simply means a javascript unit test. (There are a couple of
other suffix options, but .unitjs is the most common, and it will do for most
purposes).

### Writing the test itself

Here is an example to show how your test might look:

```js
/** Tests for awesome.js */

GEN_INCLUDE([
  'awesome.js'  // Include the file under test.
]);

AwesomeUnitTest = class extends testing.Test {}

const EXPECTED_AWESOME_NUMBER = 1e6;

TEST_F('AwesomeUnitTest', 'HowAwesomeExactly', function () {
  // Some asserts to make sure the file under test is working as expected.
  assertEquals(EXPECTED_AWESOME_NUMBER, awesome.find_awesome_number(), 'If this fails, contact Larry');
});
```

Note: This example shows the shorter way of writing a test, using the class
keyword (available since ECMAScript 6). There is an alternative but equivalent
syntax that uses the prototype keyword instead, which has a bit more
boilerplate. Codesearch shows that the prototype has been used more often in
the past, but prefer to use the shorter class syntax going forward.

### Writing the BUILD rules

Next you need to add a
[js2gtest](https://cs.chromium.org/chromium/src/chrome/test/base/js2gtest.gni)
rule in the same package. Let's say it's called `package_one`. Here is an
example to show how your BUILD rules might look:

```build
js2gtest("package_one_unitjs_tests") {
  test_type = "unit"
  sources = [
    "awesome_test.unitjs",
    # ... for simplicity, put all .unitjs files in this package into this one rule.
  ]
  gen_include_files = [
    "awesome.js",
  ]
}

source_set("unit_tests") {
  testonly = true
  deps = [":package_one_unitjs_tests"]
}
```

And finally, you will have to modify the huge `//chrome/test:unit_tests` build
rule to depend (directly or indirectly) on your new `//package_one:unit_tests`
target.

Once this is done, you can build and run the tests like normal:

```shell
chromium/src$ out/Default/unit_tests --gtest_filter="AwesomeUnitTest*"
```

## Testing JS code which depends on another library

You may have some other way that the dependencies are being correctly included
in production. However, it will probably be simplest not to try and make the
`js2gtest` rule recognize the dependency management system you are using, but
instead just declare all JS dependencies explicitly when writing the test. The
way to do this is by adding the needed JS source files to both `GEN_INCLUDE` in
the test and `gen_include_files` in the BUILD rule. These files will be included
in the order specified in the JS file. These files that are included can also
have their own calls to GEN_INCLUDE to transitively include other files, if
needed - these would then all need to be listed in the gen_include_files list in
the BUILD rule.

### Writing a test which depends on another library

```js {highlight="lines:4"}
/** Tests for awesome.js */

GEN_INCLUDE([
  '//some/dependency.js',
  'awesome.js',
]);

AwesomeUnitTest = class extends testing.Test {}
// ... etc ...
```

### Writing the BUILD rule for a test that depends on another library

```build {highlight="lines:7"}
js2gtest("package_one_unitjs_tests") {
  test_type = "unit"
  sources = [
    "awesome_test.unitjs",
  ]
  gen_include_files = [
    "//some/dependency.js",
    "awesome.js",
  ]
}
# ... etc ...
```

See
[SamlTimestampsTest](https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/login/saml_timestamps_test.unitjs?q=//ui/webui/resources/js/cr.js)
and its
[BUILD rule](https://cs.chromium.org/chromium/src/chrome/browser/resources/chromeos/login/BUILD.gn?q=//ui/webui/resources/js/cr.js)
for an example of how it includes the `cr.js` library.

## Testing JS code which depends on the browser / the DOM {#browser-dep}

The tests above are run using a V8 interpreter. However, they are not run from
within a browser. That means all JS language features are available, but no
browser features are available. Specifically, the browser context (the global
object called `window`) is left undefined, and anything to do with the DOM, the
UI, rendering, parsing, event handling, mouse clicks, HTML, XML, SVG, canvas,
etc is not supported. It may be that you need some of this - perhaps you are
trying to write and test a web-based UI, or perhaps your code just expects an
XML parser to be available.

If you are creating an web-based UI, what you are now writing is called a
`webui` test. This document is about how to write JS unit tests generally -
to test UI in particular, see
[Testing WebUI with Mocha](https://www.chromium.org/developers/updating-webui-for-material-design/testing-webui-with-mocha).

If on the other hand, you are writing some JS functionality that just happens to
use a feature that is part of the browser, and not the language (such as the XML
parser), you should follow the instructions in this section. The current
best-practice is to write your unit test as before, but to declare it as a
`webui` test and add it to the `browser_tests` build rule. Ideally there would
be a different category for unit tests that don't have any UI and so aren't
webui, but simply need a particular browser feature, but using webui works for
now.

### Changes to your test to make it a webui test

```js
AwesomeUnitTest = class extends testing.Test {

  /** @override */
  get browsePreload() {
    return DUMMY_URL;
  }

  // No need to run these checks unless you are testing an actual user interface.
  /** @override */
  get runAccessibilityChecks() {
    return false;
  }
}
```

### Changes to your build rule to make it a webui test

```build {highlight="lines:2,9,12"}
js2gtest("package_one_unitjs_tests") {
  test_type = "webui"
  sources = [
    "awesome_test.unitjs",
  ]
  gen_include_files = [
    "awesome.js",
  ]
  defines = [ "HAS_OUT_OF_PROC_TEST_RUNNER" ]
}

source_set("browser_tests") {
  testonly = true
  deps = [":package_one_unitjs_tests"]
}
```

FIXME: It might be nice if "HAS_OUT_OF_PROC_TEST_RUNNER" was automatically
inferred from the test type.

And the final change is to remove your test from the `//chrome/test:unit_tests`
build rule, and add it instead to the `//chrome/test:browser_tests`. As you
would expect, you now run your test like so:

```shell
chromium/src$ out/Default/browser_tests --gtest_filter="AwesomeUnitTest*"
```

And now the browser context and global `window` object should be available in
your test.

Note: If your test is declared as a `unit` test, it must be part of
`//chrome/test:unit_tests`, and if your test is declared as a `webui` test, it
must be part of `//chrome/test:browser_tests`. If your test is included in the
wrong build rule, it will not compile, since it will be missing some necessary
dependencies.

## Troubleshooting

### js2gtest.js: Error reading file

Perhaps one of the files you are trying to read does not exist, has the wrong
name, or has not been properly declared in the list of `gen_include_files` in
`BUILD.gn`. The file that could not be found or read should be part of the error
message, but if for some reason it is not clear, you can narrow it down by
removing files from the `GEN_INCLUDE` directive one by one.

### ReferenceError: window is not defined

Or: document is not defined, DOMParser is not defined, frames is not defined,
alert is not defined...

This sounds like your unit test depends on some functionality that is part of
the browser, and not part of the JS language itself. See the
[section about depending on the browser](#browser-dep)

### Test passes locally but fails on the commit queue

This is probably due to a dependency not being properly declared - for instance,
you have a file in the `GEN_INCLUDE` directive, that is not included in the list
of `gen_include_files` in `BUILD.gn`, When run locally, the test may be able to
find the appropriate file, but the swarming bots that run the submit queue will
not necessarily be able to find files that have not been explicitly declared as
dependencies.

You can try running your test locally but in a more isolated way, so as to
reproduce the problem locally. Something like:

```shell
chromium/src$ tools/mb/mb.py run //out/Default browser_tests -- --gtest_filter="AwesomeUnitTest*"
```

### Duplicate output file {#duplicate}

The `js2gtest` rule copies various JS files to testdata, where they are read as
data when the test is run. This causes problems if two different js2gtest rule
instances both try to copy the same file to the same place in testdata. Having
two rules both copy the same source file to the same destination is a build
error - every file must be written only once.

It should be safe to have multiple js2gtest rules which have files in common in
the `gen_include_files` list, since these files are not copied. But, it will
cause a build error to have multiple js2gtest rules which have files in common
in either the `sources` list or the `extra_js_files` list, since all of these
files are copied to testdata.

To avoid this, only create one js2gtest rule per package which has all the
necessary sources in, and to include files from outside the package, use
gen_include_files and not extra_js_files.

The duplicate output file build error could look something like this:

```
ERROR at //chrome/test/base/js2gtest.gni: Duplicate output file.
    copy(copy_target_name) {
    ^-----------------------
Two or more targets generate the same output:
  test_data/ui/webui/resources/js/cr.js
```

If you encounter such a build error and fix it, you could still end up with
warnings on subsequent builds - something like the following:

```
warning: multiple rules generate test_data/ui/webui/resources/js/cr.js. builds involving this target will not be correct; continuing anyway [-w dupbuild=warn]
```

Get rid of this warning by doing a clean build.

## Advanced topics

### GEN_INCLUDE alternative

There is an alternative to `gen_include_files` that is called `extra_js_files`.
These are JS files that are not read during compilation, but are loaded at
runtime. Because they are not read at compile time, they can contain no
directives to be executed at compile time.

Compare this to `gen_include_files` which are both read at compile time, and
included at runtime. This means they support compile time directives like `GEN`
and `GEN_INCLUDE` which can generate code and include further files - and those
further files could have their own directives, and so on.

Whether or not compile time directives are needed or used, these two rules work
differently - extra_js_files copies source files into the `testdata` directory,
but gen_include_files leaves the source files where they are.

Warning: Avoid using `extra_js_files` because all files listed there are copied
into the testdata directory, which can easily lead to the "duplicate output
file" build error described in the [previous section](#duplicate).

However, if you find you need to use extra_js_files instead of
gen_include_files, this is how it is done:

#### Changes to your test to make it use extra_js_files

```js {highlight="lines:4-7"}
AwesomeUnitTest.prototype = {
  __proto__: testing.Test.prototype,

  extraLibraries: [
    "awesome.js",
    "//some/dependency.js",
  ],
};
```

#### Changes to your build rule to make it use extra_js_files

```build {highlight="lines:6"}
js2gtest("package_one_unitjs_tests") {
  test_type = "unit"
  sources = [
    "awesome_test.unitjs",
  ]
  extra_js_files = [
    "awesome.js",
    "//some/dependency.js"
  ]
}
```

### Including custom C++ code in the generated C++ test

There are a number of ways to add to the C++ code that is generated by your
.unitjs file.

Calling
[GEN(code)](https://cs.chromium.org/chromium/src/chrome/test/base/js2gtest.js?q=function.GEN%5C%28)
in your .unitjs file will cause the C++ code to be included verbatim in the
generated C++ test.

The function `TEST_F` (or alternative form `TEST_F_WITH_PREAMBLE`) can take a
[preamble](https://cs.chromium.org/chromium/src/chrome/test/base/js2gtest.js?q=preamble)
argument - the preamble is extra C++ code that is included in the generated C++
test, immediately before the code which makes the interpreter run the JS part of
the test.

You can define functions on your JS Test prototype, `testGenPreamble()` and
`testGenPostamble()`. The testGenPreamble function is used to generate code that
is included immediately before the JS part of your test, and the
testGenPostamble function is used to generate code that goes immediately after -
so you could for example put an "#ifdef" in the preamble and an "#endif" in the
postamble. If you define these functions, they should call GEN to output the
necessary C++ code.

Since these are relatively complex to use, it may help you to codesearch for
examples. Here are some useful examples:

[oobe_webui_browsertest.js](https://cs.chromium.org/chromium/src/chrome/test/data/chromeos/oobe_webui_browsertest.js) -
makes the browser fullscreen before running the JS test.

[saml_password_attributes_test.unitjs](https://cs.chromium.org/chromium/src/chrome/browser/resources/gaia_auth_host/saml_password_attributes_test.unitjs) -
loads an XML file into a JS global variable before running the JS test.

Another thing that will make your life a little bit easier is being able to
check the C++ code that is generated. Look for it in
`out/Default/path/to/package_one/awesome_test-gen.cc` or equivalent.

Warning: Overuse of custom C++ code can make your test difficult to understand -
at this point you are running JS, that generates custom C++ - which will much
later be compiled and executed, in order to help set up the test environment,
within which the JS test itself will be run.

Tip: To avoid this complexity, consider if it would be clearer to simply write a
C++ test directly, instead of generating the C++ test from a JS file. You can
always run JS code from a C++ test, even if the C++ test was not generated from
a JS file. You could even use the generated output from a js2gtest rule as a
starting point for writing a custom C++ test which you then check in.

