# Chromium Java Style Guide

_For other languages, please see the [Chromium style
guides](https://chromium.googlesource.com/chromium/src/+/main/styleguide/styleguide.md)._

Chromium follows the [Android Open Source style
guide](http://source.android.com/source/code-style.html) unless an exception
is listed below.

You can propose changes to this style guide by sending an email to
`java@chromium.org`. Ideally, the list will arrive at some consensus and you can
request review for a change to this file. If there's no consensus,
[`//styleguide/java/OWNERS`](https://chromium.googlesource.com/chromium/src/+/main/styleguide/java/OWNERS)
get to decide.

[TOC]

## Java Language Features

### Type Deduction using "var" {#var}

A variable declaration can use the `var` keyword in place of the type (similar
to the `auto` keyword in C++). In line with the [guidance for
C++](https://google.github.io/styleguide/cppguide.html#Type_deduction), the
`var` keyword may be used when it aids readability and the type of the value is
already clear (ex. `var bundle = new Bundle()` is OK, but `var something =
returnValueIsNotObvious()` may be unclear to readers who are new to this part of
the code).

The `var` keyword may also be used in try-with-resources when the resource is
not directly accessed (or when it falls under the previous guidance), such as:

```java
try (var ignored = StrictModeContext.allowDiskWrites()) {
    // 'var' is permitted so long as the 'ignored' variable is not used directly
    // in the code.
}
```

### Exceptions

A quick primer:

* `Throwable`: Base class for all exceptions
  * `Error`: Base class for exceptions which are meant to crash the app.
  * `Exception`: Base class for exceptions that make sense the `catch`.
    * `RuntimeException`: Base class for exceptions that do not need to be
       declared as `throws` ("unchecked exceptions").

#### Broad Catch Handlers {#broad-catches}

Use catch statements that do not catch exceptions they are not meant to.
 * There is rarely a valid reason to `catch (Throwable t)`, since that
   includes the (generally unrecoverable) `Error` types.

Use `catch (Exception e)` when working with OS APIs that might throw
(assuming the program can recover from them).
 * There have been many cases of crashes caused by `IllegalStateException` /
   `IllegalArgumentException` / `SecurityException` being thrown where only
   `RemoteException` was being caught. Unless catch handlers will differ
   based on exception type, just catch `Exception`.

Do not use `catch (RuntimeException e)`.
 * It is useful to extend `RuntimeException` to make unchecked exception
   types, but the type does not make much sense in `catch` clauses, as
   there are not times when you'd want to catch all unchecked exceptions,
   but not also want to catch all checked exceptions.

#### Exception Messages {#exception-messages}

Avoid adding messages to exceptions that do not aid in debugging. For example:

```java
try {
    somethingThatThrowsIOException();
} catch (IOException e) {
    // Bad - message does not tell you more than the stack trace does:
    throw new RuntimeException("Failed to parse a file.", e);
    // Good - conveys that this block failed along with the "caused by" exception.
    throw new RuntimeException(e);
    // Good - adds useful information.
    throw new RuntimeException(String.format("Failed to parse %s", fileName), e);
}
```

#### Wrapping with RuntimeException {#throw-unchecked}

It is common to wrap a checked exception with a RuntimeException for cases
where a checked exception is not recoverable, or not possible. In order to
reduce the number of stack trace "caused by" clauses, and to save on binary
size, use [`JavaUtils.throwUnchecked()`] instead.

```java
try {
    somethingThatThrowsIOException();
} catch (IOException e) {
    // Bad - RuntimeException adds no context and creates longer stack traces.
    throw new RuntimeException(e);
    // Good - Original exception is preserved.
    throw JavaUtils.throwUnchecked(e);
}
```

*** note
Do not use `throwUnchecked()` when the exception may want to be caught.
***


[`JavaUtils.throwUnchecked()`]: https://source.chromium.org/search?q=symbol:JavaUtils.throwUnchecked

### Asserts

The build system:
 * strips asserts in release builds (via R8),
 * enables them in debug builds,
 * and enables them in report-only mode for Canary builds.

```java
// Code for assert expressions & messages is removed when asserts are disabled.
assert someCallWithoutSideEffects(param) : "Call failed with: " + param;
```

Use your judgement for when to use asserts vs exceptions. Generally speaking,
use asserts to check program invariants (e.g. parameter constraints) and
exceptions for unrecoverable error conditions (e.g. OS errors). You should tend
to use exceptions more in privacy / security-sensitive code.

Do not add checks when the code will crash anyways. E.g.:

```java
// Don't do this.
assert(foo != null);
foo.method(); // This will throw anyways.
```

For multi-statement asserts, use [`BuildConfig.ENABLE_ASSERTS`] to guard your
code (similar to `#if DCHECK_IS_ON()` in C++). E.g.:

```java
import org.chromium.build.BuildConfig;

...

if (BuildConfig.ENABLE_ASSERTS) {
    // Any code here will be stripped in release builds by R8.
    ...
}
```

[`BuildConfig.ENABLE_ASSERTS`]: https://source.chromium.org/search?q=symbol:BuildConfig%5C.ENABLE_ASSERTS

#### DCHECKS vs Java Asserts  {#asserts}

`DCHECK` and `assert` are similar, but our guidance for them differs:
 * CHECKs are preferred in C++, whereas asserts are preferred in Java.

This is because as a memory-safe language, logic bugs in Java are much less
likely to be exploitable.

### toString()  {#toString}

Use explicit serialization methods (e.g. `toDebugString()` or `getDescription()`)
instead of `toString()` when dynamic dispatch is not required.

1. R8 cannot detect when `toString()` is unused, so overrides will not be stripped
   when unused.
2. R8 cannot optimize / inline these calls as well as non-overriding methods.

### Records & AutoValue {#records}

```java
// Banned.
record Rectangle(float length, float width) {}
```

**Rationale:**
 * To avoid dead code:
   * Records and `@AutoValue` generate `equals()`, `hashCode()`, and `toString()`,
     which `R8` is unable to remove when unused.
   * When these methods are required, implement them explicitly so that the
     intention is clear.
 * Also - supporting `record` requires build system work ([crbug/1493366]).

Example with `equals()` and `hashCode()`:

```java
public class ValueClass {
    private final SomeClass mObjMember;
    private final int mIntMember;

    @Override
    public boolean equals(Object o) {
        return o instanceof ValueClass vc
                && Objects.equals(mObjMember, vc.mObjMember)
                && mIntMember == vc.mIntMember;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mObjMember, mIntMember);
    }
}
```

[crbug/1493366]: https://crbug.com/1493366

### Enums

Banned. Use [`@IntDef`](#intdefs) instead.

**Rationale:**

Java enums generate a lot of bytecode. Use constants where possible. When a
custom type hierarchy is required, use explicit classes with inheritance.

### Finalizers

In line with [Google's Java style guide] and [Android's Java style guide],
never override `Object.finalize()`.

Custom finalizers:
* are called on a background thread, and at an unpredicatble point in time,
* swallow all exceptions (asserts won't work),
* causes additional garbage collector jank.

Classes that need destructor logic should provide an explicit `destroy()`
method. Use [LifetimeAssert](https://chromium.googlesource.com/chromium/src/+/main/base/android/java/src/org/chromium/base/LifetimeAssert.java)
to ensure in debug builds and tests that `destroy()` is called.

[Google's Java style guide]: https://google.github.io/styleguide/javaguide.html#s6.4-finalizers
[Android's Java style guide]: https://source.android.com/docs/setup/contribute/code-style#dont-use-finalizers

## Java Library APIs

Android provides the ability to bundle copies of `java.*` APIs alongside
application code, known as [Java Library Desugaring]. However, since this
bundling comes with a performance cost, Chrome does not use it. Treat `java.*`
APIs the same as you would `android.*` ones and guard them with
`Build.VERSION.SDK_INT` checks [when necessary]. The one exception is if the
method is [directly backported by D8] (these are okay to use, since they are
lightweight). Android Lint will fail if you try to use an API without a
corresponding `Build.VERSION.SDK_INT` guard or `@RequiresApi` annotation.

[Java Library Desugaring]: https://developer.android.com/studio/write/java8-support-table
[when necessary]: https://developer.android.com/reference/packages
[directly backported by D8]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/r8/backported_methods.txt

### Logging

* Use `org.chromium.base.Log` instead of `android.util.Log`.
  * It provides `%s` support, and ensures log stripping works correctly.
* Minimize the use of `Log.w()` and `Log.e()`.
  * Debug and Info log levels are stripped by ProGuard in release builds, and
    so have no performance impact for shipping builds. However, Warning and
    Error log levels are not stripped.
* Function calls in log parameters are *not* stripped by ProGuard.

```java
Log.d(TAG, "There are %d cats", countCats());  // countCats() not stripped.
```

### Streams

Most uses of [Java streams] are discouraged. If you can write your code as an
explicit loop, then do so. The primary reason for this guidance is because the
lambdas (and method references) needed for streams almost always result in
larger binary size ([example](https://chromium-review.googlesource.com/c/chromium/src/+/4329952).

The `parallel()` and `parallelStream()` APIs are simpler than their loop
equivalents, but are are currently banned due to a lack of a compelling use case
in Chrome. If you find one, please discuss on `java@chromium.org`.

[Java streams]: https://docs.oracle.com/javase/8/docs/api/java/util/stream/package-summary.html

### AndroidX Annotations {#annotations}

* Use them liberally. They are [documented here](https://developer.android.com/studio/write/annotations).
  * They generally improve readability.
  * Many make lint more useful.
* `javax.annotation.Nullable` vs `androidx.annotation.Nullable`
  * Always prefer `androidx.annotation.Nullable`.
  * It uses `@Retention(SOURCE)` rather than `@Retention(RUNTIME)`.

#### IntDefs {#intdefs}

Values can be declared outside or inside the `@interface`. Chromium style is
to declare inside.

```java
@IntDef({ContactsPickerAction.CANCEL, ContactsPickerAction.CONTACTS_SELECTED,
        ContactsPickerAction.SELECT_ALL, ContactsPickerAction.UNDO_SELECT_ALL})
@Retention(RetentionPolicy.SOURCE)
public @interface ContactsPickerAction {
    int CANCEL = 0;
    int CONTACTS_SELECTED = 1;
    int SELECT_ALL = 2;
    int UNDO_SELECT_ALL = 3;
    int NUM_ENTRIES = 4;
}
// ...
void onContactsPickerUserAction(@ContactsPickerAction int action, ...);
```

Values of `Integer` type are also supported, which allows using a sentinel
`null` if needed.

[@IntDef annotation]: https://developer.android.com/studio/write/annotations#enum-annotations
[Android lint]: https://chromium.googlesource.com/chromium/src/+/HEAD/build/android/docs/lint.md


## Style / Formatting {#style}

### File Headers
* Use the same format as in the [C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md#File-headers).

### TODOs

* TODO should follow chromium convention. Examples:
  * `TODO(username): Some sentence here.`
  * `TODO(crbug.com/40192027): Even better to use a bug for context.`

### Parameter Comments

Use [parameter comments] when they aid in the readability of a function call.

E.g.:

```java
someMethod(/* enabled= */ true, /* target= */ null, defaultValue);
```

[parameter comments]: https://errorprone.info/bugpattern/ParameterName

### Default Field Initializers

* Fields should not be explicitly initialized to default values (see
  [here](https://groups.google.com/a/chromium.org/d/topic/chromium-dev/ylbLOvLs0bs/discussion)).

### Curly Braces

Conditional braces should be used, but are optional if the conditional and the
statement can be on a single line.

Do:

```java
if (someConditional) return false;
for (int i = 0; i < 10; ++i) callThing(i);
```

or

```java
if (someConditional) {
    return false;
}
```

Do NOT do:

```java
if (someConditional)
    return false;
```

### Import Order

* Static imports go before other imports.
* Each import group must be separated by an empty line.

This is the order of the import groups:

1. android
1. androidx
1. com (except com.google.android.apps.chrome)
1. dalvik
1. junit
1. org
1. com.google.android.apps.chrome
1. org.chromium
1. java
1. javax

## Testing

Googlers, see [go/clank-test-strategy](http://go/clank-test-strategy).

In summary:

* Use real dependencies when feasible and fast. Use Mockito’s `@Mock` most
  of the time, but write fakes for frequently used dependencies.

* Do not use Robolectric Shadows for Chromium code.
  * Shadows make code harder to refactor.
  * Prefer to refactor code to make it more testable.
  * When you really need to use a test double for a static method, add a
    `setFooForTesting() [...]` method to make the test contract explicit.
    * Use [`ResettersForTesting.register()`] from within `ForTesting()`
      methods to ensure that state is reset between tests.

* Use Robolectric when possible (when tests do not require native). Other
  times, use on-device tests with one of the following annotations:
  * [`@Batch(UNIT_TESTS)`] for unit tests
  * [`@Batch(PER_CLASS)`] for integration tests
  * [`@DoNotBatch`] for when each test method requires an app restart

[`ResettersForTesting.register()`]: https://source.chromium.org/search?q=symbol:ResettersForTesting.register
[`@Batch(UNIT_TESTS)`]: https://source.chromium.org/search?q=symbol:Batch.UNIT_TESTS
[`@Batch(PER_CLASS)`]: https://source.chromium.org/search?q=symbol:Batch.PER_CLASS
[`@DoNotBatch`]: https://source.chromium.org/search?q=symbol:DoNotBatch

### Test-only Code

Functions and fields used only for testing should have `ForTesting` as a
suffix so that:

1. The `android-binary-size` trybot can [ensure they are removed] in
   non-test optimized builds (by R8).
2. [`PRESUMBIT.py`] can ensure no calls are made to such methods outside of
   tests, and

`ForTesting` methods that are `@CalledByNative` should use
`@CalledByNativeForTesting` instead.

Symbols that are made public (or package-private) for the sake of tests
should be annotated with [`@VisibleForTesting`]. Android Lint will check
that calls from non-test code respect the "otherwise" visibility.

Symbols with a `ForTesting` suffix **should not** be annotated with
`@VisibleForTesting`. While `otherwise=VisibleForTesting.NONE` exists, it
is redundant given the "ForTesting" suffix and the associated lint check
is redundant given our trybot check. You should, however, use it for
test-only constructors.

[ensure they are removed]: /docs/speed/binary_size/android_binary_size_trybot.md#Added-Symbols-named-ForTest
[`PRESUMBIT.py`]: https://chromium.googlesource.com/chromium/src/+/main/PRESUBMIT.py
[`@VisibleForTesting`]: https://developer.android.com/reference/androidx/annotation/VisibleForTesting

## Location

"Top level directories" are defined as directories with a GN file, such as
[//base](https://chromium.googlesource.com/chromium/src/+/main/base/)
and
[//content](https://chromium.googlesource.com/chromium/src/+/main/content/),
Chromium Java should live in a directory named
`<top level directory>/android/java`, with a package name
`org.chromium.<top level directory>`.  Each top level directory's Java should
build into a distinct JAR that honors the abstraction specified in a native
[checkdeps](https://chromium.googlesource.com/chromium/buildtools/+/main/checkdeps/checkdeps.py)
(e.g. `org.chromium.base` does not import `org.chromium.content`).  The full
path of any java file should contain the complete package name.

For example, top level directory `//base` might contain a file named
`base/android/java/org/chromium/base/Class.java`. This would get compiled into a
`chromium_base.jar` (final JAR name TBD).

`org.chromium.chrome.browser.foo.Class` would live in
`chrome/android/java/org/chromium/chrome/browser/foo/Class.java`.

New `<top level directory>/android` directories should have an `OWNERS` file
much like
[//base/android/OWNERS](https://chromium.googlesource.com/chromium/src/+/main/base/android/OWNERS).

## Tools

`google-java-format` is used to auto-format Java files. Formatting of its code
should be accepted in code reviews.

You can run `git cl format` to apply the automatic formatting.

Chromium also makes use of several [static analysis] tools.

[static analysis]: /build/android/docs/static_analysis.md

## Miscellany

* Use UTF-8 file encodings and LF line endings.
