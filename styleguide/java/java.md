# Chromium Java style guide

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

## Java 10 Language Features

### Type deduction using `var`

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

## Java 8 Language Features
[D8] is used to rewrite some Java 7 & 8 language constructs in a way that is
compatible with Java 6 (and thus all Android versions). Use of [these features]
is encouraged.

[D8]: https://developer.android.com/studio/command-line/d8
[these features]: https://developer.android.com/studio/write/java8-support

## Java Library APIs

Android provides the ability to bundle copies of `java.` APIs alongside
application code, known as [Java Library Desugaring]. However, since this
bundling comes with a performance cost, Chrome does not use it. Treat `java.`
APIs the same as you would `android.` ones and guard them with
`Build.VERSION.SDK_INT` checks [when necessary]. The one exception is if the
method is [directly backported by D8] (these are okay to use, since they are
lightweight). Android Lint will fail if you try to use an API without a
corresponding `Build.VERSION.SDK_INT` guard or `@RequiresApi` annotation.

[Java Library Desugaring]: https://developer.android.com/studio/write/java8-support-table
[when necessary]: https://developer.android.com/reference/packages
[directly backported by D8]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/r8/backported_methods.txt

## Other Language Features & APIs

### Exceptions
We discourage overly broad catches via `Throwable`, `Exception`, or
`RuntimeException`, except when dealing with `RemoteException` or similar
system APIs.
 * There have been many cases of crashes caused by `IllegalStateException` /
   `IllegalArgumentException` / `SecurityException` being thrown where only
   `RemoteException` was being caught. In these cases, use
   `catch (RemoteException | RuntimeException e)`.
 * For all broad catch expressions, add a comment to explain why.

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

### Asserts
The Chromium build system strips asserts in release builds (via ProGuard) and
enables them in debug builds (or when `dcheck_always_on=true`) (via a [build
step](https://codereview.chromium.org/2517203002)). You should use asserts in
the [same
scenarios](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md#CHECK_DCHECK_and-NOTREACHED)
where C++ DCHECK()s make sense. For multi-statement asserts, use
`org.chromium.build.BuildConfig.ENABLE_ASSERTS` to guard your code (similar to
`#if DCHECK_IS_ON()` in C++).

Example assert:

```java
assert someCallWithoutSideEffects() : "assert description";
```

Example use of `BuildConfig.ENABLE_ASSERTS`:

```java
import org.chromium.build.BuildConfig;

...

if (BuildConfig.ENABLE_ASSERTS) {
  // Any code here will be stripped in Release by ProGuard.
  ...
}
```

### Finalizers
In line with [Google's Java style guide](https://google.github.io/styleguide/javaguide.html#s6.4-finalizers),
never override `Object.finalize()`.

Custom finalizers:
* are called on a background thread, and at an unpredicatble point in time,
* swallow all exceptions (asserts won't work),
* causes additional garbage collector jank.

Classes that need destructor logic should provide an explicit `destroy()`
method. Use [LifetimeAssert](https://chromium.googlesource.com/chromium/src/+/main/base/android/java/src/org/chromium/base/LifetimeAssert.java)
to ensure in debug builds and tests that `destroy()` is called.

### AndroidX Annotations
* Use them! They are [documented here](https://developer.android.com/studio/write/annotations).
  * They generally improve readability.
  * Some make lint more useful.
* `javax.annotation.Nullable` vs `androidx.annotation.Nullable`
  * Always prefer `androidx.annotation.Nullable`.
  * It uses `@Retention(SOURCE)` rather than `@Retention(RUNTIME)`.

### IntDef Instead of Enum

Java enums generate far more bytecode than integer constants. When integers are
sufficient, prefer using an [@IntDef annotation], which will have usage checked
by [Android lint].

Values can be declared outside or inside the `@interface`. We recommend the
latter, with constants nested within it as follows:

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

## Tools

### Automatically formatting edited files
A checkout should give you clang-format to automatically format Java code.
It is suggested that Clang's formatting of code should be accepted in code
reviews.

You can run `git cl format` to apply the automatic formatting.

### IDE Setup
For automatically using the correct style, follow the guide to set up your
favorite IDE:

* [Android Studio](https://chromium.googlesource.com/chromium/src/+/main/docs/android_studio.md)
* [Eclipse](https://chromium.googlesource.com/chromium/src/+/main/docs/eclipse.md)

### Checkstyle
Checkstyle is automatically run by the build bots, and to ensure you do not have
any surprises, you can also set up checkstyle locally using [this
guide](https://sites.google.com/a/chromium.org/dev/developers/checkstyle).

### Lint
Lint is run as part of the build. For more information, see
[here](https://chromium.googlesource.com/chromium/src/+/main/build/android/docs/lint.md).

## Style / Formatting

### File Headers
* Use the same format as in the [C++ style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md#File-headers).

### TODOs
* TODO should follow chromium convention. Examples:
  * `TODO(username): Some sentence here.`
  * `TODO(crbug.com/123456): Even better to use a bug for context.`

### Code formatting
* Fields should not be explicitly initialized to default values (see
  [here](https://groups.google.com/a/chromium.org/d/topic/chromium-dev/ylbLOvLs0bs/discussion)).

### Curly braces
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

## Test-only Code
Functions used only for testing should be restricted to test-only usages
with the testing suffixes supported [PRESUMBIT.py](https://chromium.googlesource.com/chromium/src/+/main/PRESUBMIT.py).
`ForTesting` is the conventional suffix although similar patterns, such as
`ForTest`, are also accepted. These suffixes are checked at presubmit time
to ensure the functions are called only by test files.

It's generally bad practice to directly call test-only methods from
non-test-only code. However, occasionally it has to be done, and if so, you
should guard the check with an `if (BuildConfig.IS_FOR_TEST)` so that our Java
optimizer can still remove the call in non-test builds.

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

## Miscellany
* Use UTF-8 file encodings and LF line endings.
