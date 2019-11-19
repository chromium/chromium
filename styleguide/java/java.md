# Chromium Java style guide

_For other languages, please see the [Chromium style
guides](https://chromium.googlesource.com/chromium/src/+/master/styleguide/styleguide.md)._

Chromium follows the [Android Open Source style
guide](http://source.android.com/source/code-style.html) unless an exception
is listed below.

You can propose changes to this style guide by sending an email to
`java@chromium.org`. Ideally, the list will arrive at some consensus and you can
request review for a change to this file. If there's no consensus,
[`//styleguide/java/OWNERS`](https://chromium.googlesource.com/chromium/src/+/master/styleguide/java/OWNERS)
get to decide.

[TOC]

## Java 8 Language Features
[Desugar](https://github.com/bazelbuild/bazel/blob/master/src/tools/android/java/com/google/devtools/build/android/desugar/Desugar.java)
is used to rewrite some Java 7 & 8 language constructs in a way that is
compatible with Java 6 (and thus all Android versions). Use of
[these features](https://developer.android.com/studio/write/java8-support)
is encouraged, but there are some gotchas:

### Default Interface Methods
 * Desugar makes default interface methods work by copy & pasting the default
   implementations into all implementing classes.
 * This technique is fine for infrequently-used interfaces, but should be
   avoided (e.g. via a base class) if it noticeably increases method count.

### Lambdas and Method References
 * These are syntactic sugar for creating anonymous inner classes.
   * Furthermore, stateless lambdas 
     [become singletons](https://stackoverflow.com/questions/27524445/does-a-lambda-expression-create-an-object-on-the-heap-every-time-its-executed)
     and so do not result in new instances when used in loops.
 * Use them only where the cost of an extra class & method definition is
   justified.

### try-with-resources
 * Some library classes do not implement Closeable on older platform APIs.
   Runtime exceptions are thrown if you use them with a try-with-resources.
   Do not use the following classes in a try-with-resources:
   * java.util.zip.ZipFile (implemented in API 19)
   * java.net.Socket (implemented in API 19)

## Other Language Features & APIs

### Exceptions
* As with the Android style guide, we discourage overly broad catches via
`Exception` / `Throwable` / `RuntimeException`.
  * If you need to have a broad catch expression, use a comment to explain why.
* Catching multiple exceptions in one line is fine.

It is OK to do:
```java
try {
  somethingThatThrowsIOException(filePath);
  somethingThatThrowsParseException(filePath);
} catch (IOException | ParseException e) {
  Log.w(TAG, "Failed to read: %s", filePath, e);
}
```

* Avoid adding messages to exceptions that do not aid in debugging.

For example:
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
scenarios](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md#CHECK_DCHECK_and-NOTREACHED)
where C++ DCHECK()s make sense. For multi-statement asserts, use
`org.chromium.base.BuildConfig.DCHECK_IS_ON` to guard your code.

Example assert:

```java
assert someCallWithoutSideEffects() : "assert description";
```

Example use of `DCHECK_IS_ON`:

```java
if (org.chromium.base.BuildConfig.DCHECK_IS_ON) {
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
method.

### Other Android Support Library Annotations
* Use them! They are [documented here](https://developer.android.com/studio/write/annotations).
  * They generally improve readability.
  * Some make lint more useful.
* `javax.annotation.Nullable` vs `android.support.annotation.Nullable`
  * Always prefer `android.support.annotation.Nullable`.
  * It uses `@Retention(SOURCE)` rather than `@Retention(RUNTIME)`.

## Tools

### Automatically formatting edited files
A checkout should give you clang-format to automatically format Java code.
It is suggested that Clang's formatting of code should be accepted in code
reviews.

You can run `git cl format` to apply the automatic formatting.

### IDE Setup
For automatically using the correct style, follow the guide to set up your
favorite IDE:

* [Android Studio](https://chromium.googlesource.com/chromium/src/+/master/docs/android_studio.md)
* [Eclipse](https://chromium.googlesource.com/chromium/src/+/master/docs/eclipse.md)

### Checkstyle
Checkstyle is automatically run by the build bots, and to ensure you do not have
any surprises, you can also set up checkstyle locally using [this
guide](https://sites.google.com/a/chromium.org/dev/developers/checkstyle).

### Lint
Lint is run as part of the build. For more information, see
[here](https://chromium.googlesource.com/chromium/src/+/master/build/android/docs/lint.md).

## Style / Formatting

### File Headers
* Use the same format as in the [C++ style guide](https://chromium.googlesource.com/chromium/src/+/master/styleguide/c++/c++.md#File-headers).

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

## Location
"Top level directories" are defined as directories with a GN file, such as
[//base](https://chromium.googlesource.com/chromium/src/+/master/base/)
and
[//content](https://chromium.googlesource.com/chromium/src/+/master/content/),
Chromium Java should live in a directory named
`<top level directory>/android/java`, with a package name
`org.chromium.<top level directory>`.  Each top level directory's Java should
build into a distinct JAR that honors the abstraction specified in a native
[checkdeps](https://chromium.googlesource.com/chromium/buildtools/+/master/checkdeps/checkdeps.py)
(e.g. `org.chromium.base` does not import `org.chromium.content`).  The full
path of any java file should contain the complete package name.

For example, top level directory `//base` might contain a file named
`base/android/java/org/chromium/base/Class.java`. This would get compiled into a
`chromium_base.jar` (final JAR name TBD).

`org.chromium.chrome.browser.foo.Class` would live in
`chrome/android/java/org/chromium/chrome/browser/foo/Class.java`.

New `<top level directory>/android` directories should have an `OWNERS` file
much like
[//base/android/OWNERS](https://chromium.googlesource.com/chromium/src/+/master/base/android/OWNERS).

## Miscellany
* Use UTF-8 file encodings and LF line endings.
