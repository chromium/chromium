# Accessing C++ Switches In Java

[TOC]

## Introduction

Accessing C++ switches in Java is implemented via a Python script which analyzes
the C++ switches file and spits out the corresponding Java class. The generated
class name will be based upon the switch file name, and the path must be
specified in a comment within the switch file itself.

## Usage

1. Create a template file (ex. `FooSwitches.java.tmpl`)
   ```java
    // Copyright $YEAR The Chromium Authors. All rights reserved.
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

2. Add a new build target

    ```gn
    import("//build/config/android/rules.gni")

    java_cpp_strings("java_switches") {
      sources = [
        "//base/android/foo_switches.cc",
      ]
      template = "//base/android/java_templates/FooSwitches.java.tmpl"
    }
    ```

3. Add the new target to the desired `android_library` targets `srcjar_deps`:

    ```gn
    android_library("base_java") {
      srcjar_deps = [
        ":java_switches",
      ]
    }
    ```

4. The generated file `out/Default/gen/.../org/chromium/foo/FooSwitches.java`
   would contain:

    ```java
    // Copyright $YEAR The Chromium Authors. All rights reserved.
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

## Code
* [Generator
code](https://cs.chromium.org/chromium/src/build/android/gyp/java_cpp_strings.py?dr=C&sq=package:chromium)
and
[Tests](https://cs.chromium.org/chromium/src/build/android/gyp/java_cpp_strings_tests.py?dr=C&sq=package:chromium)
* [GN
template](https://cs.chromium.org/chromium/src/build/config/android/rules.gni?sq=package:chromium&dr=C)
