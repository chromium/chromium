# Accessing C++ Features In Java

[TOC]

## Introduction

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

3. Add a `deps` entry to `"common_java"` in `"//android_webview/BUILD.gn"` if
   creating a new `android_library` in the previous step:

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

This can happen if you've re-declared a feature for mutually-exclsuive build
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

## Checking if a Feature is enabled

The standard pattern is to create a `FooFeatureList.java` class with an
`isEnabled()` method (ex.
[`ContentFeatureList`](/content/public/android/java/src/org/chromium/content_public/browser/ContentFeatureList.java)).
This should call into C++ (ex.
[`content_feature_list`](/content/browser/android/content_feature_list.cc)),
where a subset of features are exposed via the `kFeaturesExposedToJava` array.
You can either add your `base::Feature` to an existing `feature_list` or create
a new `FeatureList` class if no existing one is suitable. Then you can check the
enabled state like so:

```java
// It's OK if ContentFeatureList checks FooFeatures.*, so long as
// content_feature_list.cc exposes `kMyFeature`.
if (ContentFeatureList.isEnabled(FooFeatures.MY_FEATURE)) {
    // ...
}
```

At the moment, `base::Features` must be explicitly exposed to Java this way, in
whichever layer needs to access their state. See https://crbug.com/1060097.

## See also
* [Accessing C++ Enums In Java](android_accessing_cpp_enums_in_java.md)
* [Accessing C++ Switches In Java](android_accessing_cpp_switches_in_java.md)

## Code
* [Generator code](/build/android/gyp/java_cpp_features.py) and
  [Tests](/build/android/gyp/java_cpp_features_tests.py)
* [GN template](/build/config/android/rules.gni)
