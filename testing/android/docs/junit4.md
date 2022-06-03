# JUnit 4 Migration

As of Android 24 (N), JUnit3 style javatests have been deprecated for the new
JUnit4-based [Android Testing Support Library][1].
We are in the process of changing all instrumentation tests in chromium to
JUnit4 style. This doc explains the differences between JUnit3 and JUnit4
instrumentation tests and how to write or convert them.

[TOC]

## Differences between JUnit3 and JUnit4 instrumentation tests

|              | JUnit3                                              | JUnit4                                   |
|--------------|-----------------------------------------------------|------------------------------------------|
| Inheritance  | Tests extends TestCase or child classes             | No inheritance.                          |
| Test methods | methods named with test prefix                      | methods annotated with @Test             |
| Set up       | setUp() method                                      | public method annotated with @Before     |
| Tear down    | tearDown() method                                   | public method annotated with @After      |
| Test runner  | declared within test apk AndroidManifest.xml        | Must specify `chromium-junit4:"true"`    |
| Class runner | N/A                                                 | @RunWith(XClassRunner.class)             |
| Assertion    | Extends from junit.framework.Assert, inherited APIs | Use static methods from org.junit.Assert |

> Please note that during the migration, we support running JUnit3 and JUnit4
> tests in the same apk. This requires two tags, one each for JUnit3 and JUnit4.
> The tag for the JUnit4 runner must specify `chromium-junit4:"true"`
> ([Example][2])

-   **Other JUnit4 features**:
    - Tests can be annotated to expect an exception, e.g.
      `@Test(expected=MyException.class)`. Tests annotated this way will
      fail if they do not throw the given exception.
   - **Test suite set up**: public static method annotated with `@BeforeClass`
   - **Test suite tear down**: public static method annotated with
                                      `@AfterClass`
-  **Replacement for JUnit3 test base classes**
    - [`TestRule`][3]:
        - TestRule is a class to **outsource your test setUp, tearDown, and
          utility methods**. Since there are no more interitance and TestBase classes,
          one should use TestRule for any API calls provided by its test base classes
          previously.
        - One test can declare multiple TestRules and the class runner will run all of
          them. If the order of the TestRule matters to you, use
          [`RuleChain`][8]

    - [`ActivityTestRule`][4]
        - `ActivityTestRule` is a special `TestRule` provided by Android Testing
          Support Library that allows tests to launch an Activity.
          ([Documentation][4])

## Example Code of JUnit3 test and JUnit4 test

JUnit3:

```java
public class MyTestClass extends MyActivityInstrumentationTestCase2<TestActivity> {
    @Override
    protected void setUp(){
        super.setUp();
        setActivityIntent(new Intent());
        getActivity();
    }

    @Override
    protected void tearDown() {
        specialActionFromSuper();
        super.tearDown();
    }

    public void testA() {
        assertEquals(1, 1);
    }
}
```

JUnit4:

```java
@RunWith(BaseJUnit4ClassRunner.class);
public class TestClass {
    @Rule public ActivityTestRule<TestActivity> mRule = new ActivityTestRule<>(TestActivity.class);

    @Before
    public void setUp() { //Must be public
        mRule.launchActivity(new Intent());
    }

    @After
    public void tearDown() { //Must be public
        mRule.specialActionFromActivityTestRule();
    }

    @Test
    public void testA() {
        Assert.assertEquals(1, 1);
    }
}
```

## Migration process

1.  Add required libraries to your target dependencies in BUILD.gn
    -   JUnit 4 library: `//third_party/junit`
    -   Android Testing Support Rules:
        -   `//third_party/android_support_test_runner:runner_java`
            (for `AndroidJUnitRunner`, etc)
        -   `//third_party/android_support_test_runner:rules_java`
            (for `ActivityTestRule`, etc)
1.  Add class runner to your test apk manifest.
    ([example][2])
    -   Keep in mind you can have multiple instrumentations in your manifest.
        Our test runner will run JUnit4 tests with JUnit4 runner and JUnit3
        tests with non-JUnit4 runner.
1.  Refactor TestBase class to a TestRule class.
    ([example CL](https://codereview.chromium.org/2632043002))
    -   +yolandyan will do this part, however, if you did refactoring yourself,
        please add him as a reviewer for your CL and enjoy his eternal appreciation!
1.  Use [auto migrate script][5] to or manually convert all JUnit3 tests to
    JUnit4 style in a your javatest directory
    -   we understand it's tedious to just manually write all the annotations,
        change modifiers, etc to convert all the javatest, so we created an auto
        change script that helps you to convert all the javatests in a certain
        directory. Please check its [README page][5]
        on instructions.


## Customized TestRule example

TestRule:

```java
public class MyRule implements TestRule {
    // 1: Add utility methods...

    @Override
    public Statement apply(final Statement base, Description desc) {
        return new Statement() {
            @Override
            public void evaluate() {
                // 2: Code here runs before @Before method
                base.evaluate()
                // 3: Code here runs after @After method
            }
        }
    }
}
```

## Command Line Flags

In our Junit3 tests command line flags (set by the CommandLineFlag annotations) were inherited from the
test base classes. As an example, ChromeActivityTestBase is annotated with:

```java
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE, ...
```

and as a result any test in a class derived from ChromeActivityTestBase will disable the first run experience.

The Junit4 tests classes are not however, derived from test base classes; instead their behavior is defined by
test rules. To support this our Junit4 test runner will examine the command line flag annotations on all rules
referenced with @Rule annotations in the test class. In addition, where one rule is derived from another, the
command line flags propogate through the hierarchy of rules. See, for example, [BottomSheetTestRule][11]

Note:- This has only recently been implemented, so is not yet used in all tests. See [this bug][12]

The CommandLineFlags annonations are more fully documented in the [CommandLineFlags class][13]

## Common Errors

1.  Instrumentation tests that rely on test thread to have message handler
    will not work. For example error message:

        java.lang.RuntimeException: Can't create handler inside thread that has not called Looper.prepare()

    or

        java.lang.IllegalStateException: The current thread must have a looper!

    Please utilize `ActivityTestRule.runOnUiThread(Runnable r)` to refactor
    these tests. For more, check this [GitHub issue][6]

1.  Use `@UiThreadTest` with caution!!
    -   Currently, **@UiThreadTest is only effective when UiThreadTestRule or
        ActivityTestRule is declared** in the test class.
    -   Please use **`android.support.test.annotation.UiThreadTest`, NOT
        `android.test.UiThreadTest`**.
    -   When using @UiThreadTest, **it would cause `setUp` and `tearDown` to
        run in Ui Thread** as well. Avoid that by calling [`runOnUiThread`][9]
        or [`runOnMainSync`][10] with a Runnable.

    ```java
    // Wrong test
    public class Test {
        @Rule
        public ActivityTestRule<MyActivity> mRule = new ActivityTestRule<>(
                MyActivity.class);

        @Before
        public void setUp() {
            // Cause failure because this also runs on Ui Thread, while it
            // is intended for Instrumentation worker thread
            mRule.launchActivity();
        }

        @UiThreadTest
        public void test() {
            actionThatNeedsUiThread();
        }
    }
    ```

    The correct thing to do is

    ```java
    // Correct test
    public class Test {
        @Rule
        public ActivityTestRule<MyActivity> mRule = new ActivityTestRule<>(
                MyActivity.class);

        @Before
        public void setUp() {
            mRule.launchActivity();
        }

        public void test() {
            mRule.runOnUiThread(new Runnable() {
                @Override
                public void run() {
                    actionThatNeedsUiThread();
                }
            });
        }
    }
    ```

1.  `assertEquals(float a, float b)` and `assertEquals(double a, double b)` are
    deprecated in JUnit4's Assert class. **Despite only generating a warning at
    build time, they fail at runtime.** Please use
    `Assert.assertEquals(float a, float b, float delta)`

1.  Errorprone expects all public methods starting with `test...` to be
    annotated with `@Test`. Failure to meet that expectation will cause
    errorprone to fail with something like this:

        [JUnit4TestNotRun] Test method will not be run; please add @Test annotation

    In particular, you may see this when attempting to disable tests. In that
    case, the test should be annotated with both @DisabledTest and @Test.

## Common questions

-   Q: Are `@Test` and `@LargeTest/@MediumTest/@SmallTest` annotation
    both necessary?
    -   A: Yes, both are required for now. We plan to refactor this in the
        future.
-   Q: Isn't the inheritance of the Test classes just migrated to inheritance
    of TestRules?
    -   A: Yes. During the migration, we plan to maintain a 1:1 mapping between
        the test base classes and TestRules (e.g. ContentShellTestBase to
        ContentShellTestRule in this
        [CL](https://codereview.chromium.org/2632043002)).
        This allows the auto convert script to replace API calls in any
        JUnit3 tests. After the migration, we plan to refactor the TestRules to
        be more modular.

If you have any other questions, feel free to report in [this bug][7].

## Links and Crbugs

- [Android Test Support Library documentation][1]
- [Auto change script][5]
- [Crbug for JUnit3 to JUnit4 migration][7]

[1]: https://developer.android.com/topic/libraries/testing-support-library/index.html
[2]: https://cs.chromium.org/chromium/src/android_webview/tools/system_webview_shell/layout_tests/AndroidManifest.xml?l=36
[3]: http://junit.org/junit4/javadoc/4.12/org/junit/rules/TestRule.html
[4]: https://developer.android.com/reference/android/support/test/rule/ActivityTestRule.html
[5]: https://github.com/yoland68/chromium-junit-auto-migrate
[6]: http://github.com/skyisle/android-test-kit/issues/121
[7]: https://bugs.chromium.org/p/chromium/issues/detail?id=640116
[8]: http://junit.org/junit4/javadoc/4.12/org/junit/rules/RuleChain.html
[9]: https://developer.android.com/reference/android/app/Instrumentation.html#runOnMainSync(java.lang.Runnable)
[10]: https://developer.android.com/reference/android/support/test/rule/UiThreadTestRule.html#runOnUiThread(java.lang.Runnable)
[11]: /chrome/test/android/javatests/src/org/chromium/chrome/test/BottomSheetTestRule.java
[12]: https://bugs.chromium.org/p/chromium/issues/detail?id=734553
[13]: /base/test/android/javatests/src/org/chromium/base/test/util/CommandLineFlags.java
