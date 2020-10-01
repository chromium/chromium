# Accessing C++ Enums In Java

[TOC]

## Introduction

Accessing C++ enums in Java is implemented via a Python script which analyzes
the C++ enum and spits out the corresponding Java class. The enum needs to be
annotated in a particular way. By default, the generated class name will be the
same as the name of the enum. If all the names of the enum values are prefixed
with the MACRO\_CASED\_ name of the enum those prefixes will be stripped from
the Java version.

## Features
* Customize the package name of the generated class using the
`GENERATED_JAVA_ENUM_PACKAGE` directive (required)
* Customize the class name using the `GENERATED_JAVA_CLASS_NAME_OVERRIDE`
directive (optional)
* Strip enum entry prefixes to make the generated classes less verbose using
the `GENERATED_JAVA_PREFIX_TO_STRIP` directive (optional)
* Supports
[`@IntDef`](https://developer.android.com/reference/android/support/annotation/IntDef.html)
* Copies comments that directly precede enum entries into the generated Java
class

## Usage

1. Add directives to your C++ enum

    ```cpp
    // GENERATED_JAVA_ENUM_PACKAGE: org.chromium.chrome
    // GENERATED_JAVA_CLASS_NAME_OVERRIDE: FooBar
    // GENERATED_JAVA_PREFIX_TO_STRIP: BAR_
    enum SomeEnum {
      BAR_A,
      BAR_B,
      BAR_C = BAR_B,
    };
    ```

2. Add a new build target

    ```gn
    import("//build/config/android/rules.gni")

    java_cpp_enum("foo_generated_enum") {
      sources = [
        "base/android/native_foo_header.h",
      ]
    }
    ```

3. Add the new target to the desired android_library targets srcjar_deps:

    ```gn
    android_library("base_java") {
      srcjar_deps = [
        ":foo_generated_enum",
      ]
    }
    ```

4. The generated file `org/chromium/chrome/FooBar.java` would contain:

    ```java
    package org.chromium.chrome;

    import android.support.annotation.IntDef;

    import java.lang.annotation.Retention;
    import java.lang.annotation.RetentionPolicy;

    @IntDef({
        FooBar.A, FooBar.B, FooBar.C
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface FooBar {
      int A = 0;
      int B = 1;
      int C = 1;
    }
    ```

## Formatting Notes

* Handling long package names:

    ```cpp
    // GENERATED_JAVA_ENUM_PACKAGE: (
    //   org.chromium.chrome.this.package.is.too.long.to.fit.on.a.single.line)
    ```

* Enum entries
    * Single line enums should look like this:

        ```cpp
        // GENERATED_JAVA_ENUM_PACKAGE: org.foo
        enum NotificationActionType { BUTTON, TEXT };
        ```

    * Multi-line enums should have one enum entry per line, like this:

        ```cpp
        // GENERATED_JAVA_ENUM_PACKAGE: org.foo
        enum NotificationActionType {
          BUTTON,
          TEXT
        };
        ```

    * Multi-line enum entries are allowed but should be formatted like this:

        ```cpp
        // GENERATED_JAVA_ENUM_PACKAGE: org.foo
        enum NotificationActionType {
          LongKeyNumberOne,
          LongKeyNumberTwo,
          ...
          LongKeyNumberThree =
              LongKeyNumberOne | LongKeyNumberTwo | ...
        };
        ```

* Preserving comments

    ```cpp
    // GENERATED_JAVA_ENUM_PACKAGE: org.chromium
    enum CommentEnum {
      // This comment will be preserved.
      ONE,
      TWO, // This comment will NOT be preserved.
      THREE
    }
    ```

    ```java
    ...
    public @interface CommentEnum {
      ...
      /**
       * This comment will be preserved.
       */
      int ONE = 0;
      int TWO = 1;
      int THREE = 2;
    }
    ```

## See also
* [Accessing C++ Switches In Java](android_accessing_cpp_switches_in_java.md)
* [Accessing C++ Features In Java](android_accessing_cpp_features_in_java.md)

## Code
* [Generator
code](https://cs.chromium.org/chromium/src/build/android/gyp/java_cpp_enum.py?dr=C&sq=package:chromium)
and
[Tests](https://cs.chromium.org/chromium/src/build/android/gyp/java_cpp_enum_tests.py?dr=C&q=java_cpp_enum_tests&sq=package:chromium&l=1)
* [GN
template](https://cs.chromium.org/chromium/src/build/config/android/rules.gni?q=java_cpp_enum.py&sq=package:chromium&dr=C&l=458)
