# Accessing C++ Features In Java

[TOC]

# Checking if a Feature is enabled

In C++, add your `base::Feature` to an existing `base::android::FeatureMap` in the appropriate layer/component. Then, you can check the
enabled state like so:

```java
// FooFeatureMap can check FooFeatures.MY_FEATURE as long as foo_feature_map.cc
// adds `kMyFeature` to its `base::android::FeatureMap`.
if (FooFeatureMap.getInstance().isEnabled(FooFeatures.MY_FEATURE)) {
    // ...
}
```

If the components or layer does not have a FeatureMap, create a new one:

1. In C++, create a new `foo_feature_map.cc` (ex.
[`content_feature_map`](/content/browser/android/content_feature_map.cc)) with:
    * `kFeaturesExposedToJava` array with a pointer to your `base::Feature`.
    * `GetFeatureMap` with a static `base::android::FeatureMap` initialized
      with `kFeaturesExposedToJava`.
    * `JNI_FooFeatureList_GetNativeMap` simply calling `GetFeatureMap`.
2. In Java, create a `FooFeatureMap.java` class extending `FeatureMap.java`
   (ex. [`ContentFeatureMap`](/content/public/android/java/src/org/chromium/content/browser/ContentFeatureMap.java)) with:
    * A `getInstance()` that returns the singleton instance.
    * A single `long getNativeMap()` as @NativeMethods.
    * An `@Override` for the abstract `getNativeMap()` simply calling
      `FooFeatureMapJni.get().getNativeMap()`.
3. Still in Java, `FooFeatures.java` with the String constants with the feature
   names needs to be generated or created.
    * Auto-generate it by writing a `FooFeatures.java.tmpl`. [See instructions
      below]((#generating-foo-feature-list-java)).
    * If `FooFeatures` cannot be auto-generated, keep the list of String
      constants with the feature names in a `FooFeatures` or `FooFeatureList`
      separate from the pure boilerplate `FooFeatureMap`.

# Auto-generating FooFeatureList.java {#generating-foo-feature-list-java}

Accessing C++ `base::Features` in Java is implemented via a Python script which
analyzes the `*_features.cc` file and generates the corresponding Java class,
based on a template file. The template file must be specified in the GN target.
This outputs Java String constants which represent the name of the
`base::Feature`.

## Usage

1. Create a template file (ex. `FooFeatures.java.tmpl`). Change "Copyright
   2020" to be whatever the year is at the time of writing (as you would for any
   other file).
   ```java
    // Copyright 2020 The Chromium Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    package org.chromium.foo;

    // Be sure to escape any curly braces in your template by doubling as
    // follows.
    /**
     * Contains features that are specific to the foo project.
     */
    public final class FooFeatures {{

    {NATIVE_FEATURES}

        // Prevents instantiation.
        private FooFeatures() {{}}
    }}
   ```

2. Add a new build target and add it to the `srcjar_deps` of an
   `android_library` target:

    ```gn
    if (is_android) {
      import("//build/config/android/rules.gni")
    }

    if (is_android) {
      java_cpp_features("java_features_srcjar") {
        # External code should depend on ":foo_java" instead.
        visibility = [ ":*" ]
        sources = [
          "//base/android/foo_features.cc",
        ]
        template = "//base/android/java_templates/FooFeatures.java.tmpl"
      }

      # If there's already an android_library target, you can add
      # java_features_srcjar to that target's srcjar_deps. Otherwise, the best
      # practice is to create a new android_library just for this target.
      android_library("foo_java") {
        srcjar_deps = [ ":java_features_srcjar" ]
      }
    }
    ```

3. If you need to expose your flag in WebView, and you created a new
   `android_library` in the previous step, then add a `deps` entry to
   `common_java` in `//android_webview/BUILD.gn`.

   If you don't need to expose a flag in WebView, then skip this and go to the
   next step.

   ```gn
   android_library("common_java") {
     ...

     deps = [
       ...
       "//path/to:foo_java",
       ...
     ]
   }
   ```

4. The generated file `out/Default/gen/.../org/chromium/foo/FooFeatures.java`
   would contain:

    ```java
    // Copyright $YEAR The Chromium Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    package org.chromium.foo;

    // Be sure to escape any curly braces in your template by doubling as
    // follows.
    /**
     * Contains features that are specific to the foo project.
     */
    public final class FooFeatures {

        // This following string constants were inserted by
        //     java_cpp_features.py
        // From
        //     ../../base/android/foo_features.cc
        // Into
        //     ../../base/android/java_templates/FooFeatures.java.tmpl

        // Documentation for the C++ Feature is copied here.
        public static final String SOME_FEATURE = "SomeFeature";

        // ...snip...

        // Prevents instantiation.
        private FooFeatures() {}
    }
    ```

### Troubleshooting

The script only supports limited syntaxes for declaring C++ base::Features. You
may see an error like the following during compilation:

```
...
org/chromium/foo/FooFeatures.java:41: error: duplicate declaration of field: MY_FEATURE
    public static final String MY_FEATURE = "MyFeature";
```

This can happen if you've re-declared a feature for mutually-exclusive build
configs (ex. the feature is enabled-by-default for one config, but
disabled-by-default for another). Example:

```c++
#if defined(...)
BASE_FEATURE(kMyFeature, "MyFeature", base::FEATURE_ENABLED_BY_DEFAULT);
#else
BASE_FEATURE(kMyFeature, "MyFeature", base::FEATURE_DISABLED_BY_DEFAULT);
#endif
```

The `java_cpp_features` rule doesn't know how to evaluate C++ preprocessor
directives, so it generates two identical Java fields (which is what the
compilation error is complaining about). Fortunately, the workaround is fairly
simple. Rewrite the definition to only use directives around the enabled state:

```c++
BASE_FEATURE(kMyFeature,
             "MyFeature",
#if defined(...)
             base::FEATURE_ENABLED_BY_DEFAULT
#else
             base::FEATURE_DISABLED_BY_DEFAULT
#endif
};

```


## See also
* [Accessing C++ Enums In Java](android_accessing_cpp_enums_in_java.md)
* [Accessing C++ Switches In Java](android_accessing_cpp_switches_in_java.md)

## Code
* [Generator code](/build/android/gyp/java_cpp_features.py) and
  [Tests](/build/android/gyp/java_cpp_features_tests.py)
* [GN template](/build/config/android/rules.gni)
