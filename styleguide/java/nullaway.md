# Null Checking

Chromium uses [NullAway] to enforce [JSpecify]-style `@Nullable` annotations.
NullAway is a [Error Prone] plugin and [runs as a static analysis step] for
targets without `chromium_code = false`.

[TOC]

[NullAway]: https://github.com/uber/NullAway
[JSpecify]: https://jspecify.dev/docs/user-guide/
[Error Prone]: https://errorprone.info/
[runs as a static analysis step]: /build/android/docs/static_analysis.md#ErrorProne

## NullAway Configuration

[Chromium's NullAway configuration] is as follows:
* [JSpecify mode] is enabled.
   * `@Nullable` is `TYPE_USE`.
   * Non-annotated means non-null (no need for `@NonNull`).
   * Nullness of local variables is inferred.
* Copies of [supported annotations] exist under
  `org.chromium.build.annotations`.
    * These are a part of `//build/android:build_java`, which for convenience,
      is a default dep of all `android_library` and `java_library` targets.
* Null checking is enabled only for classes annotated with `@NullMarked`.
   * For other classes (e.g.: most library & OS APIs), `@Nullable` and
     `@NonNull` are respected, but non-annotated types are permissive (return
     types are non-null and parameters are nullable).
* Java collections and Guava's `Preconditions` are [modeled directly] in
  NullAway.
    * Some additional types are modeled via [`ChromeNullAwayLibraryModel`].
* Android `onCreate()` methods are implicitly marked `@Initializer`.
* `assert foo != null` causes `foo` to no longer be nullable.
* [`assumeNonNull(foo)`] causes `foo` to no longer be nullable without actually
  checking.

[Chromium's NullAway configuration]: https://source.chromium.org/search?q=%22XepOpt:NullAway%22%20f:compile_java%20-f:third_party&sq=&ss=chromium
[JSpecify mode]: https://github.com/uber/NullAway/wiki/JSpecify-Support
[supported annotations]: https://github.com/uber/NullAway/wiki/Supported-Annotations
[`ChromeNullAwayLibraryModel`]: https://source.chromium.org/chromium/chromium/src/+/main:tools/android/errorprone_plugin/src/org/chromium/tools/errorprone/plugin/ChromeNullAwayLibraryModel.java
[modeled directly]: https://github.com/uber/NullAway/blob/HEAD/nullaway/src/main/java/com/uber/nullaway/handlers/LibraryModelsHandler.java
[`assumeNonNull(foo)`]: https://source.chromium.org/chromium/chromium/src/+/main:third_party/openscreen/src/build/android/java/src/org/chromium/build/NullUtil.java

## Nullness Migration

We are actively opting classes into enforcement and hope to be complete by
March 2025. See details in [crbug.com/389129271].

[crbug.com/389129271]: https://crbug.com/389129271

## Nullness Primer

### Type Annotations

```java
// Plain Objects:
private String mNonNullString;
private @Nullable String mNullableString;
private Outer.@Nullable Inner mNullableNestedType;

// Arrays:
private String @Nullable[] mNullableArrayOfNonNullString;
private @Nullable String[] mNonNullArrayOfNullableString;

// Generics:
private List<@Nullable String> mNonNullListOfNullableString;
private @Nullable Callback<@Nullable String> mNullableCallbackOfNullableString;

// Does not compile (annotation must come immediately before type):
@Nullable
private String mInvalidAnnotation;
```

### Method Annotations

NullAway analyzes code on a per-method basis. These annotations tell it how
about pre/post conditions:

```java
// Using this with non-private methods never makes sense.
@RequiresNonNull("mNullableString")
private void usesNullableString() {
    // No warning:
    if (mNullableString.isEmpty()) { ... }
}

@EnsuresNonNull("mNullableString")
private void codeCanCallThisAndThenUseNullableString() {
    // This will warn if mNullableString is @Nullable at any egress.
    assert mNullableString != null;
}

// If this method returns true, then mThing is non-null.
@EnsuresNonNullIf("mThing")
private boolean isThingEnabled() {
    return mThing != null;
}

// Also works with static fields and negated return values.
@EnsuresNonNullIf(value={"sThing1", "sThing2"}, result=false)
private static boolean isDestroyed() {
    return sThing1 == null || sThing2 == null;
}

// If foo is null, this method returns false.
// Most other forms of contracts are not supported.
@Contract("null -> false")
private boolean isParamNonNull(@Nullable String foo) {
    return foo != null;
}

// Returns null only when defaultValue is null
@Contract("_, !null -> !null")
@Nullable String getOrDefault(String key, @Nullable String defaultValue) {
  return defaultValue;
}
```

### "assert", "assumeNonNull()", and "requireNonNull()"

```java
// Always use "import static" for assumeNonNull.
import static org.chromium.build.NullUtil.assumeNonNull;

public String void example() {
    // Prefer its statement form.
    // It won't change git blame, and reads like a precondition.
    assumeNonNull(mNullableThing);

    // It supports nested fields.
    assumeNonNull(mNullableThing.nullableField);

    // Use its expression form when it is more readable to do so.
    someHelper(assumeNonNull(Foo.getInstance()));

    String ret = mNullableThing.getNullableString();
    if (willJustCrashLaterAnyways) {
        // Use "assert" when not locally dereferencing the object.
        assert ret != null;
    } else {
        // Use "requireNonNull()" when returning null might lead to bad things.
        // Asserts are enabled only on Canary and are set as "dump without crashing".
        Objects.requireNonNull(ret);
    }
    return ret;
}

// Use "assert false" + "assumeNonNull(null)" for unreachable code.
public String describe(@MyIntDef int validity) {
    return switch (validity) {
        case MyIntDef.VALID -> "okay";
        case MyIntDef.INVALID -> "not okay";
        default -> {
            assert false;
            yield assumeNonNull(null);
        }
    };
}
```

### Object Construction and Destruction

**Construction:**

* NullAway warns if any non-null fields are still nullable at the end of a
  constructor.
  * When a class uses two-phase initialization (e.g., has an `onCreate()` or
    `initialize()`), you can tell NullAway to not check for null until after
    methods annotated with `@Initializer` are called.
  * `@Initializer` can also be used for `static` methods, which impacts
    warnings for `static` fields.

**Destruction:**

For classes with `destroy()` methods that set fields to `null` that would
otherwise be non-null, you can either:

1) Annotate the fields as `@Nullable` and add `!isDestroyed()` asserts / guards
   where necessary (where `isDestroyed()` is annotated with
   `@EnsuresNonNullIf(value=..., result=false)`), or
2) Annotate the `destroy()` method with `@SuppressWarnings("NullAway")`.

### JNI

* Nullness is not checked for `@CalledByNative` methods ([crbug/389192501]).
* Nullness **is checked** via `assert` statements for Java->Native methods
  (when `@NullMarked` exists).

[crbug/389192501]: https://crbug.com/389192501

## NullAway Shortcomings

Does not work: `boolean isNull = thing == null; if (!isNull) { ... }`
* Feature request: https://github.com/uber/NullAway/issues/98

It does not infer nullness of inferred generics.
* Feature request: https://github.com/uber/NullAway/issues/1075

Validation of (but not use of) `@Contract` is buggy.
* Bug: https://github.com/uber/NullAway/issues/1104

## FAQ

**Q: Why not use Checker Framework?**

A: Chromium already uses Error Prone, so NullAway was easy to integrate.

**Q: How do `@NullUnmarked` and `@SuppressWarnings("ErrorProne")` differ?**

A: NullAway treats these two the same. In Chromium, `@SuppressWarnings` is used
when a warning is unavoidable (appeasing NullAway would make the code worse),
and `@NullUnmarked` is used when a method has not yet been updated to support
`@Nullable` annotations (it's a remnant of our [automated adding of
annotations]).

[automated adding of annotations]: https://docs.google.com/document/d/1KNKs7jI8uoBLfBq4HCuGTYPLuTOMgUEuKrzyCUFjEk8/edit?tab=t.0#heading=h.1y1pwesy8vhq

**Q: Can I use JSpecify Annotations?**

A: Yes. For code that will be mirrored and built in other environments, it is
best to use JSpecify annotations. You'll probably want to set:

```gn
deps += [ "//third_party/android_deps:org_jspecify_jspecify_java" ]

# Prevent automatic dep on build_java.
chromium_code = false

# Do not let chromium_code = false disable Error Prone.
enable_errorprone = true
```
