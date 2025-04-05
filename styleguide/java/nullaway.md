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
* Android's `onCreate()` (and similar) methods are implicitly marked `@Initializer`.

[Chromium's NullAway configuration]: https://source.chromium.org/search?q=%22XepOpt:NullAway%22%20f:compile_java%20-f:third_party&sq=&ss=chromium
[JSpecify mode]: https://github.com/uber/NullAway/wiki/JSpecify-Support
[supported annotations]: https://github.com/uber/NullAway/wiki/Supported-Annotations
[`ChromeNullAwayLibraryModel`]: https://source.chromium.org/chromium/chromium/src/+/main:tools/android/errorprone_plugin/src/org/chromium/tools/errorprone/plugin/ChromeNullAwayLibraryModel.java
[modeled directly]: https://github.com/uber/NullAway/blob/HEAD/nullaway/src/main/java/com/uber/nullaway/handlers/LibraryModelsHandler.java

## Nullness Migration

We are actively opting classes into enforcement. Track progress via [crbug.com/389129271].

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

### Field Annotations

```java
// Starts as null, but may not be assigned a nullable value.
private @MonotonicNonNull String mSomeValue;

public void doThing(String value) {
    // Emits a warning since mSomeValue is nullable:
    helper(mSomeValue);

    mSomeValue = value;
    // No warning about mSomeValue being nullable, even though it's used in a lambda.
    PostTask.postTask(TaskTraits.USER_BLOCKING, () -> helper(mSomeValue));
}
```

### "assert", "assumeNonNull()", "assertNonNull()", and "requireNonNull()"

```java
// Always use "import static" for assumeNonNull / assertNonNull.
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.build.NullUtil.assertNonNull;

public String void example() {
    // Prefer statements over expressions to keep preconditions separate from usage.
    assumeNonNull(mNullableThing);
    assert mOtherThing != null;

    // It supports nested fields and getters.
    assumeNonNull(someObj.nullableField);
    assumeNonNull(someObj.getNullableThing());

    // Use its expression form when it is more readable to do so.
    someHelper(assumeNonNull(Foo.maybeCreate(true)));

    // Use assertNonNull when you need an assert as an expression.
    mNonNullField = assertNonNull(dict.get("key"));

    String ret = obj.getNullableString();
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

// Use "assertNonNull(null)" for unreachable code.
public String describe(@MyIntDef int validity) {
    return switch (validity) {
        case MyIntDef.VALID -> "okay";
        case MyIntDef.INVALID -> "not okay";
        default -> assertNonNull(null);
    };
}
```

### Object Construction and Destruction

**Construction:**

* NullAway warns if any non-null fields are still nullable at the end of a
  constructor.
  * When a class uses two-phase initialization (e.g., has an `onCreate()` or
    `initialize()`), you can tell NullAway to pretend all such methods have
    been called before performing validation.
  * `@Initializer` can also be used for `static` methods, which impacts
    warnings for `static` fields.
  * That `@Initializer` methods are actually called is not checked.

*** note
**Note:** When multiple setters are always called after constructing an object,
prefer to create an single `initialize()` method that sets them instead.
***

**Destruction:**

For classes with `destroy()` methods that set fields to `null` that would
otherwise be non-null, you can either:

1) Annotate the fields as `@Nullable` and add `!isDestroyed()` asserts / guards
   where necessary (where `isDestroyed()` is annotated with
   `@EnsuresNonNullIf(value=..., result=false)`), or
2) Annotate the `destroy()` method with `@SuppressWarnings("NullAway")`.

**View Binders:**

It might seem appropriate to mark `onBindViewHolder()` with `@Initializer`,
but these are not really "methods that are called immediately after the
constructor". Instead, consider adding an `assertBound()` method.

Example:

```java
@EnsuresNonNull({"mField1", "mField2", ...})
private void assertBound() {
    assert mField1 != null;
    assert mField2 != null;
    ...
}
```

### JNI

* Nullness is not checked for `@CalledByNative` methods ([crbug/389192501]).
* Nullness **is checked** via `assert` statements for Java->Native methods
  (when `@NullMarked` exists).

[crbug/389192501]: https://crbug.com/389192501

### Struct-like Classes

NullAway has no special handling for classes with public fields and will emit
a warning for any non-primitive non-`@Nullable` fields not initialized by a
constructor.

Fix this by:

* Creating a constructor that sets these fields (Android Studio has a
  `Generate->Constructor` function that will do this).
* If this makes the call-site less readable, add `/* paramName= */` comments
  for the parameters.
* As a bonus, the constructor may also allow you to mark fields as `final`.

### Effectively Non-Null Return Types

Some methods are technically `@Nullable`, but effectively `@NonNull`. That is,
they are marked as having `@NonNull` return types despite sometimes returning
`null`. Examples:
   * [`Activity.findViewById()`]
   * `Context.getSystemService()`
   * `PreferenceManager.findPreference()` (this one via [`ChromeNullAwayLibraryModel`])

Enforcing null checks for these would be detrimental to readability.

For Chromium-authored code that falls into this bucket, prefer to add
companion "Checked" methods over mis-annotating nullability.

Example:

```java
// When you're not sure if the tab exists:
public @Nullable Tab getTabById(String tabId) {
    ...
}

// When you know the tab exists:
public Tab getTabByIdChecked(String tabId) {
    return assertNonNull(getTabById(key));
}
```

[`Activity.findViewById()`]: https://cs.android.com/android/platform/superproject/main/+/main:frameworks/base/core/java/android/app/Activity.java?q=symbol%3A%5Cbandroid.app.Activity.findViewById%5Cb%20case%3Ayes

## NullAway Shortcomings

Does not work: `boolean isNull = thing == null; if (!isNull) { ... }`
* Feature request: https://github.com/uber/NullAway/issues/98

It does not infer nullness of inferred generics.
* Feature request: https://github.com/uber/NullAway/issues/1075

Validation of (but not use of) `@Contract` is buggy.
* Bug: https://github.com/uber/NullAway/issues/1104

## FAQ

**Q: Why not use Checker Framework?**

* Chromium already uses Error Prone, so NullAway was easy to integrate.

**Q: How do `@NullUnmarked` and `@SuppressWarnings("NullAway")` differ?**

* Both suppress warnings on a method.
* `@SuppressWarnings` leaves the method signature `@NullMarked`.
* `@NullUnmarked` causes parameters and return types to have unknown
  nullability, and thus also suppress nullness warnings that may exist at a
  method's call sites.

**Q: Can I use JSpecify Annotations?**

* Yes. For code that will be mirrored and built in other environments, it is
  best to use JSpecify annotations. You'll probably want to set:

```gn
deps += [ "//third_party/android_deps:org_jspecify_jspecify_java" ]

# Prevent automatic dep on build_java.
chromium_code = false

# Do not let chromium_code = false disable Error Prone.
enable_errorprone = true
```
