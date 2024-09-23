<style>
.note::before {
  content: 'Note: ';
  font-variant: small-caps;
  font-style: italic;
}

.doc h1 {
  margin: 0;
}
</style>

# Testing WebUI Pages

Chromium WebUI pages should be tested using Mocha test suites.
[Mocha](https://mochajs.org) is a flexible, lightweight test framework with
strong async support. With Mocha, we can write small unit tests for individual
features of components instead of grouping everything into a monolithic browser
test.

There are a few pieces of Chromium WebUI infrastructure designed to make
setting up WebUI Mocha browser tests easier:

* `build_webui_tests` is a gn build rule that preprocesses test `.ts` files,
  compiles them to JavaScript, and places them in a `.grdp` file. This file
  can then be included in the WebUI test `.grd` so that the files can be
  served at runtime. This rule is further documented at
  [WebUI Build Configuration](./webui_build_configuration.md)
* `WebUIMochaBrowserTest` is a test base class that WebUI tests should
  inherit from. It handles browser test setup and teardown, injects Mocha,
  loads the `chrome://` page being tested, and imports the test files. This
  class also ensures code coverage metrics are collected when JS code
  coverage is enabled. The API for this class is documented in its
  [header
  file](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/base/web_ui_mocha_browser_test.h)

Note: For the default test loading logic in `WebUIMochaBrowserTest` to work,
`test_loader.html`, `test_loader_util.js` and `test_loader.js` should be
registered with the production Web UI's data source. This can be done manually,
or (most commonly) can be done by calling `SetupWebUIDataSource()`. If using
the test loader is not feasible for a given test - for example, test relies on
loading a specific URL such that loading `chrome://foo-bar/test_loader.html`
would break the test - see [Unusual Test Setups](#unusual-test-setups)

### Adding the BrowserTest
UIs should add a `.cc` file with the definition of their BrowserTest class in
`chrome/test/data/webui/my_webui_dir_name`. The directory structure
of `chrome/test/data/webui` should mirror that of
`chrome/browser/resources/` where the WebUI TypeScript/HTML code being tested
resides. The BrowserTest class defined in the `.cc` file should inherit from
`WebUIMochaBrowserTest`.
Example:

```c++
// chrome/test/data/webui/foo_bar/foo_bar_browsertest.cc

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"

class FooBarBrowserTest : public WebUIMochaBrowserTest {
 protected:
  FooBarBrowserTest() {
    set_test_loader_host(chrome::kChromeUIFooBarHost);
  }

 private:
  // Necessary if you have features (or the UI itself) behind a feature flag.
  base::test::ScopedFeatureList scoped_feature_list_{foo_bar::kSomeNewFeature};
};

```

Note: If testing a `chrome-untrusted://` UI, also call:
```
set_test_loader_scheme(content::kChromeUIUntrustedScheme);
```
in the constructor.

After defining the class, create one or more `IN_PROC_BROWSER_TEST_F`
functions. Each should run one or more Mocha tests.

``` c++
typedef FooBarBrowserTest FooBarAppTest;
IN_PROC_BROWSER_TEST_F(FooBarAppTest, All) {
  // Run all Mocha test suites found in app_test
  RunTest("foo_bar/app_test.js", "mocha.run();");
}

```
Should you write a large number of tests in a single file (e.g., for
one particular custom element), it may be necessary to run different suites or
even individual tests from that same file in separate `IN_PROC_BROWSER_TEST_F`
invocations, to avoid having one very long test that is at risk of timing out.

Example:

```c++
// This class defines a convenience method to run one Mocha test suite in the
// search_box_test.js file at a time.
class FooBarSearchBoxTest: public FooBarBrowserTest {
 protected:
  void RunTestSuite(const std::string& suiteName) {
    FooBarBrowserTest::RunTest(
        "foo_bar/search_box_test.js",
        base::StringPrintf("runMochaSuite('%s');", suiteName.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(FooBarSearchBoxTest, SearchNonEmpty) {
  RunTestSuite("SearchNonEmpty");
}

IN_PROC_BROWSER_TEST_F(FooBarSearchBoxTest, SearchEmpty) {
  RunTestCase("SearchEmpty");
}

// This class defines a convenience method to run one Mocha test from the
// FooBarChildDialogTest suite at a time.
class FooBarChildDialogTest: public FooBarBrowserTest
 protected:
  void RunTestCase(const std::string& testCase) {
    FooBarBrowserTest::RunTest(
        "foo_bar/child_dialog_test.js",
        base::StringPrintf("runMochaTest('FooBarChildDialogTest', '%s');",
                           testCase.c_str()));
  }
};

IN_PROC_BROWSER_TEST_F(FooBarChildDialogTest, OpenDialog) {
  RunTestCase("OpenDialog");
}

IN_PROC_BROWSER_TEST_F(FooBarChildDialogTest, ClickButtons) {
  RunTestCase("ClickButtons");
}
```

### Adding the Mocha test files
Mocha test files should be added alongside the `.cc` file in the same
directory. Each test file should contain one or more Mocha test suites. Often
each major custom element of a UI will have its own Mocha test file, as will
many of the data classes.

``` ts
// chrome/test/data/webui/foo_bar/search_box_test.ts

import 'chrome://foo-bar/foo_bar.js';

import {FooBarSearchBoxElement} from 'chrome://foo-bar/foo_bar.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

suite('SearchNonEmpty', function() {
  let searchBox: FooBarSearchBoxElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const searchBox = document.createElement('foo-bar-search-box');
    document.body.appendChild(searchBox);

    // More setup goes here
  });

  test('search abc', async() {
    const search = searchBox.shadowRoot!.querySelector('input');
    assertTrue(!!search);
    search.value = 'abc';
    await eventToPromise('search-change', searchBox);

    // Do some assertions
  });

  test('search def', async () => {
    // etc
  });
});

suite('SearchEmpty', function() {
  let searchBox: FooBarSearchBoxElement;

  setup(function() {
    document.body.innerHTML = window.trustedTypes!.emptyHTML;
    const searchBox = document.createElement('foo-bar-search-box');
    document.body.appendChild(searchBox);

    // Different setup goes here
  });

  test('search abc when empty', async() {
    const search = searchBox.shadowRoot!.querySelector('input');
    assertTrue(!!search);
    search.value = 'abc';
    await eventToPromise('search-change', searchBox);

    // Do some assertions
  });
});

```
### Setting up the build
To build your tests as part of the `browser_tests` target, add a BUILD.gn file
in your folder:

```
# chrome/test/data/webui/foo_bar/BUILD.gn

import("../build_webui_tests.gni")

assert(!is_android)

build_webui_tests("build") {
  # Test files go here
  files = [
    "app_test.ts",
    "child_dialog_test.ts",
    "search_box_test.ts",
  ]

  # Path mapping the WebUI host URL to the path where the WebUI's compiled
  # TypeScript files are placed by the TS compiler.
  ts_path_mappings =
      [ "chrome://foo-bar/*|" +
        rebase_path("$root_gen_dir/chrome/browser/resources/foo_bar/tsc/*",
                    target_gen_dir) ]
  ts_deps = [
    "//chrome/browser/resources/foo_bar:build_ts",
  ]
}
```

Note: If testing a `chrome-untrusted://` UI, also set the
`is_chrome_untrusted` parameter for `build_webui_tests()` to `true`, to allow
importing shared test files from `chrome-untrusted://webui-test/` paths.

You then need to hook up the targets generated by this file, and list your
browsertest.cc source file, in `chrome/test/data/webui/BUILD.gn`:

```
# chrome/test/data/webui/BUILD.gn

source_set("browser_tests") {
  testonly = true

  sources = [
    # Lots of files
    # Big list, add your .cc file in alphabetical order
    "foo_bar/foo_bar_browsertest.cc",
    # Lots more files
  ]

  # Conditionally added sources (e.g., platform-specific)

}

# Further down in the file...

generate_grd("build_grd") {
  testonly = true
  grd_prefix = "webui_test"
  output_files_base_dir = "test/data/grit"
  out_grd = "$target_gen_dir/resources.grd"

  deps = [
    ":build_chai_grdp",
    ":build_mocha_grdp",
    # Lots of other deps here
    # Add your build_grdp target:
    "foo_bar:build_grdp",
    # Lots more other deps
  ]

  grdp_files = [
    # Big list of grdp files
    # Add your grdp in alphabetical order
    "$target_gen_dir/foo_bar/resources.grdp",
    # Lots more grdp files
  ]

  # Conditionally added (e.g. platform-specific) grdps and deps go here
}

```
### Running the tests

You can build the `browser_tests` target and run your WebUI tests just like
other browser tests. You may filter tests by class or test name (the arguments
to `IN_PROC_BROWSER_TEST_F`) via `--gtest_filter`:

```sh
autoninja -C out/Release browser_tests
./out/Release/browser_tests --gtest_filter=FooBarSearchBoxTest.Empty
```

### Unusual Test Setups
Some UIs may need to rely on loading a particular URL, such that using the
default `chrome://foo-bar/test_loader.html` URL that is used to dynamically
import the test modules may not work. Other UIs need to do additional setup
on the C++ side of the test, and simply want to run a test on a specified
WebContents instance. These cases are accommodated by additional methods in
`WebUIMochaBrowserTest`. A few brief examples are provided below.

Example: loading a different URL
```c++
IN_PROC_BROWSER_TEST_F(FooBarBrowserTest, NonExistentUrl) {
  // Invoking the test from a non existent URL chrome://foo-bar/does-not-exist/.
  set_test_loader_host(
      std::string(chrome::kChromeFooBarHost) + "/does-not-exist");
  RunTestWithoutTestLoader("foo_bar/non_existent_url_test.js", "mocha.run()");
}
```

Example: Advanced setup required in C++
```c++
IN_PROC_BROWSER_TEST_F(FooDialogBrowserTest, MyTest) {
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kSomeOtherBaseUrl),
      WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  FooDialog* my_dialog = FooDialog::ShowConstrained(
      kDummyParam,
      browser()->tab_strip_model()->GetActiveWebContents(),
      browser()->window()->GetNativeWindow());
  content::WebContents* web_contents = my_dialog->webui_->GetWebContents();

  ASSERT_TRUE(RunTestOnWebContents(
      web_contents, "foo_dialog/foo_dialog_test.js", "mocha.run()",
      /*skip_test_loader=*/true));
}
```

### Common errors

Tests that rely on focus, blur, or other input-related events need to be added
to the interactive_ui_tests build target rather than the browser_tests target.
browser_tests are run in parallel, and the window running the test will
gain/lose activation at unpredictable times.

Tests that touch production logic that depends on onFocus, onBlur or similar
events also needs to be added to interactive_ui_tests
