# JUnit Tests

JUnit tests are Java unit tests. These tests run locally on your workstation.

[TOC]

## Writing a JUnit test

When writing JUnit tests, you must decide whether you need to use Android code.
If you want to use Android code you must write a [Robolectric](http://robolectric.org/) test.

### JUnit tests (without Android)

Build these types of test using the `robolectric_binary` GN template.

If you don't need to use any Android code in your tests, you can write plain,
old JUnit tests. Some more documentation about writing JUnit tests can be
found [here](https://github.com/junit-team/junit4/wiki/Getting-started).

#### Example Code

```java
package org.chromium.sample.test;

import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.BlockJUnit4ClassRunner;

@RunWith(BlockJUnit4ClassRunner.class)
public class MyJUnitTest {

    @Test
    public void exampleTest() {
        boolean shouldWriteMoreJUnitTests = true;
        assertTrue(shouldWriteMoreJUnitTests);
    }
}
```

#### Example within Chromium

See the [junit_unit_tests](https://cs.chromium.org/chromium/src/testing/android/junit/BUILD.gn) test suite.

### JUnit tests with Robolectric

Build these types of test using the `robolectric_binary` GN template.

Robolectric is a unit testing framework that lets you run tests with Android
code on your workstation. It does this by providing a special version of the
Android SDK jar that can run in your host JVM. Some more information about
Robolectric can be found [here](http://robolectric.org/).

One on the main benefits of using Robolectric framework are [shadow classes](http://robolectric.org/extending/).
Robolectric comes with many prebuilt shadow classes and also lets you define
your own. Whenever an object is instantiated within a Robolectric test,
Robolectric looks for a corresponding shadow class (marked by
`@Implements(ClassBeingShadowed.class)`). If found, any time a method is invoked
on the object, the shadow class's implementation of the method is invoked first.
This works even for static and final methods.

#### Useful Tips

* Use `@RunWith(BaseRobolectricTestRunner.class)` for all Chromium Robolectric tests.
* You can specify the Android SDK to run your test with with `@Config(sdk = ??)`.

> Currently, only SDK levels 18, 21, and 25 are supported in Chromium
> but more can be added on request.

#### Example Code

```java
package org.chromium.sample.test;

import static org.junit.Assert.assertTrue;

import android.text.TextUtils;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

// Be sure to specify to run tests with a RobolectricTestRunner. The
// default JUnit test runner won't load the Robolectric Android code properly.
// BaseRobolectricTestRunner will do some common initializations. If this is
// not desired, then RobolectricTestRunner could be used directly.
@RunWith(BaseRobolectricTestRunner.class)
// Can specify some Robolectric related configs here.
// More about configuring Robolectric at http://robolectric.org/configuring/.
// SDK will default to the latest we support in Chromium.
@Config(manifest = Config.NONE, sdk = 21)
public class MyRobolectricJUnitTest {

    @Test
    public void exampleTest() {
        String testString = "test";

        // Even though these tests runs on the host, Android classes are
        // available to use thanks to Robolectric.
        assertTrue(TextUtils.equals(testString, "test"));
    }
}
```

#### Example robolectric_binary build template.

```python
robolectric_binary("my_robolectric_tests") {

    sources = [
        "java/src/foo/bar/MyJUnitTest.java"
    ]

    deps = [
        "//my/test:dependency",
    ]

    # Sets app's package name in Robolectric tests. You need to specify
    # this variable in order for Robolectric to be able to find your app's
    # resources.
    package_name = manifest_package
}
```

#### Example within Chromium

See the [content_junit_tests](https://cs.chromium.org/chromium/src/content/public/android/BUILD.gn) test suite.

## Running JUnit tests

After writing a test, you can run it by:

1. Adding the test file to a `robolectric_binary` GN target.
2. Rebuild.
3. GN will generate binary `<out_dir>/bin/run_<suite name>` which
   can be used to run your test.

For example, the following can be used to run chrome_junit_tests.

```bash
# Build the test suite after adding our new test.
ninja -C out/Debug chrome_junit_tests

# Run the test!
out/Debug/bin/run_chrome_junit_tests
```
