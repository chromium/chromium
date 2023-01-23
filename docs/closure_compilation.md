# Closure Compilation

**Important: Closure Compilation is only supported on ChromeOS Ash. On all
other platforms, Closure Compiler is deprecated; TypeScript should be used
for type checking.** See [bug](https://www.crbug.com/1316438)

## What is type safety?

[Statically-typed languages](https://en.wikipedia.org/wiki/Type_system#Static_type_checking)
like C++ and Java have the notion of variable types.

This is typically baked into how you declare variables:

```c++
const int32 kUniversalAnswer = 42;  // type = 32-bit integer
```

or as [templates](https://en.wikipedia.org/wiki/Template_metaprogramming) for
containers or generics:

```c++
std::vector<int64> fibonacci_numbers;  // a vector of 64-bit integers
```

When differently-typed variables interact with each other, the compiler can warn
you if there's no sane default action to take.

Typing can also be manually annotated via mechanisms like `dynamic_cast` and
`static_cast` or older C-style casts (i.e. `(Type)`).

Using statically-typed languages provides _some_ level of protection against
accidentally using variables in the wrong context.

JavaScript is dynamically-typed and doesn't offer this safety by default. This
makes writing JavaScript more error prone, and various type errors have resulted
in real bugs seen by many users.

## Chrome's solution to typechecking JavaScript

Enter [Closure Compiler](https://developers.google.com/closure/compiler/), a
tool for analyzing JavaScript and checking for syntax errors, variable
references, and other common JavaScript pitfalls.

To get the fullest type safety possible, it's often required to annotate your
JavaScript explicitly with [Closure-flavored @jsdoc
tags](https://developers.google.com/closure/compiler/docs/js-for-compiler)

```js
/**
 * @param {string} version A software version number (i.e. "50.0.2661.94").
 * @return {!Array<number>} Numbers corresponding to |version| (i.e. [50, 0, 2661, 94]).
 */
function versionSplit(version) {
  return version.split('.').map(Number);
}
```

See also:
[the design doc](https://docs.google.com/a/chromium.org/document/d/1Ee9ggmp6U-lM-w9WmxN5cSLkK9B5YAq14939Woo-JY0/edit).

## Typechecking Your Javascript

Given an example file structure of:

  + lib/does_the_hard_stuff.js
  + ui/makes_things_pretty.js

`lib/does_the_hard_stuff.js`:

```javascript
var wit = 100;

// ... later on, sneakily ...

wit += ' IQ';  // '100 IQ'
```

`ui/makes_things_pretty.js`:

```javascript
/** @type {number} */ var mensa = wit + 50;

alert(mensa);  // '100 IQ50' instead of 150
```

Closure compiler can notify us if we're using `string`s and `number`s in
dangerous ways.

To do this, we can create:

  + ui/BUILD.gn

With these contents:

```
# Copyright 2018 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import("//third_party/closure_compiler/compile_js.gni")

js_type_check("closure_compile") {
  deps = [
    ":make_things_pretty",
  ]
}

js_library("make_things_pretty") {
  deps = [
    "../lib:does_the_hard_stuff",
  ]

  externs_list = [
    "$externs_path/extern_name_goes_here.js"
  ]
}
```

## Running Closure compiler locally

You can locally test that your code compiles on Linux or Mac.  This requires
[Java](http://www.oracle.com/technetwork/java/javase/downloads/index.html) and a
[Chrome checkout](https://www.chromium.org/developers/how-tos/get-the-code) (i.e.
python, depot_tools). Note: on Ubuntu, you can probably just run `sudo apt-get
install openjdk-7-jre`.

First, add the following to your GN args:
```
enable_js_type_check = true
```
Then you should be able to run:

```shell
ninja -C out/Default webui_closure_compile
```

and should see output like this:

```shell
ninja: Entering directory `out/Default/'
[0/1] ACTION Compiling ui/makes_things_pretty.js
```

To compile only a specific folder, add an argument after the script name:

```shell
ninja -C out/Default ui:closure_compile
```

In our example code, this error should appear:

```
(ERROR) Error in: ui/makes_things_pretty.js
## /my/home/chromium/src/ui/makes_things_pretty.js:1: ERROR - initializing variable
## found   : string
## required: number
## /** @type {number} */ var mensa = wit + 50;
##                                   ^
```

Hooray! We can catch type errors in JavaScript!

## Preferred BUILD.gn structure
* Make all individual JS file targets a js\_library.
* The top level target should be called “closure\_compile”.
* If you have subfolders that need compiling, make “closure\_compile” a group(),
  and any files in the current directory a js\_type\_check() called “<directory>\_resources”.
* Otherwise, just make “closure\_compile” a js\_type\_check with all your js\_library targets as deps
* Leave all closure targets below other kinds of targets becaure they’re less ‘important’

See also:
[Closure Compilation with GN](https://docs.google.com/a/chromium.org/document/d/1Ee9ggmp6U-lM-w9WmxN5cSLkK9B5YAq14939Woo-JY0/edit).

## Trying your change

Closure compilation runs in the compile step of Linux, Android and ChromeOS builds.

From the command line, you try your change with:

```shell
git cl try -b linux-rel
```

## Integrating with the continuous build

To compile your code on every commit, add your file to the
`'webui_closure_compile'` target in `src/BUILD.gn`:

```
  group("webui_closure_compile") {
    data_deps = [
      # Other projects
      "my/project:closure_compile",
    ]
  }
```

## Externs

[Externs files](https://github.com/google/closure-compiler/wiki/FAQ#how-do-i-write-an-externs-file)
define APIs external to your JavaScript. They provide the compiler with the type
information needed to check usage of these APIs in your JavaScript, much like
forward declarations do in C++.

Third-party libraries like Polymer often provide externs. Chrome must also
provide externs for its extension APIs. Whenever an extension API's `idl` or
`json` schema is updated in Chrome, the corresponding externs file must be
regenerated:

```shell
./tools/json_schema_compiler/compiler.py -g externs \
  extensions/common/api/your_api_here.idl \
  > third_party/closure_compiler/externs/your_api_here.js
```
