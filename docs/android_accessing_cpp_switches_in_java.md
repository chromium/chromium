# Accessing C++ Switches In Java

[TOC]

## Introduction

Accessing C++ switches in Java is implemented via a Python script which analyzes
the C++ switches file and generates the corresponding Java class, based on a
template file. The template file must be specified in the GN target.

## Usage

1. Create a template file (ex. `FooSwitches.java.tmpl`). Change "Copyright
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
     * Contains command line switches that are specific to the foo project.
     */
    public final class FooSwitches {{

    {NATIVE_STRINGS}

        // Prevents instantiation.
        private FooSwitches() {{}}
    }}
   ```

2. Add a new build target and add it to the `srcjar_deps` of an
   `android_library` target:

    ```gn
    if (is_android) {
      import("//build/config/android/rules.gni")
    }

    if (is_android) {
      java_cpp_strings("java_switches_srcjar") {
        # External code should depend on ":foo_java" instead.
        visibility = [ ":*" ]
        sources = [
          "//base/android/foo_switches.cc",
        ]
        template = "//base/android/java_templates/FooSwitches.java.tmpl"
      }

      # If there's already an android_library target, you can add
      # java_switches_srcjar to that target's srcjar_deps. Otherwise, the best
      # practice is to create a new android_library just for this target.
      android_library("foo_java") {
        srcjar_deps = [ ":java_switches_srcjar" ]
      }
    }
    ```

3. The generated file `out/Default/gen/.../org/chromium/foo/FooSwitches.java`
   would contain:

    ```java
    // Copyright $YEAR The Chromium Authors
    // Use of this source code is governed by a BSD-style license that can be
    // found in the LICENSE file.

    package org.chromium.foo;

    /**
     * Contains command line switches that are specific to the foo project.
     */
    public final class FooSwitches {

        // ...snip...

        // This following string constants were inserted by
        //     java_cpp_strings.py
        // From
        //     ../../base/android/foo_switches.cc
        // Into
        //     ../../base/android/java_templates/FooSwitches.java.tmpl

        // Documentation for the C++ switch is copied here.
        public static final String SOME_SWITCH = "some-switch";

        // ...snip...

        // Prevents instantiation.
        private FooSwitches() {}
    }
    ```

## See also
* [Accessing C++ Enums In Java](android_accessing_cpp_enums_in_java.md)
* [Accessing C++ Features In Java](android_accessing_cpp_features_in_java.md)

## Code
* [Generator
code](https://cs.chromium.org/chromium/src/build/android/gyp/java_cpp_strings.py?dr=C&sq=package:chromium)
and
[Tests](https://cs.chromium.org/chromium/src/build/android/gyp/java_cpp_strings_tests.py?dr=C&sq=package:chromium)
* [GN
template](https://cs.chromium.org/chromium/src/build/config/android/rules.gni?sq=package:chromium&dr=C)
