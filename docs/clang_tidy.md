# Clang Tidy

[TOC]

## Danger, Will Robinson!

Support for `clang-tidy` in Chromium is very experimental, and is somewhat
painful to use. We are exploring making it easier and integrating with existing
tools, but aren't there yet. If you don't want to wait and enjoy tinkering,
forge ahead. Otherwise, feel free to turn back now.

## Introduction

[clang-tidy](http://clang.llvm.org/extra/clang-tidy/) is a clang-based C++
“linter” tool. Its purpose is to provide an extensible framework for diagnosing
and fixing typical programming errors, like style violations, interface misuse,
or bugs that can be deduced via static analysis.

## Setting Up

### Automatic Setup

The script [clang_tidy_tool.py](../tools/clang/scripts/clang_tidy_tool.py) will
automatically fetch, build, and invoke `clang-tidy`. To do this manually, follow
the steps in the next section.

### Manual Setup

In addition to a full Chromium checkout, you need the clang-tidy binary. We
recommend checking llvm's clang source and building the clang-tidy binary
directly. Instructions for getting started with clang are available from
[llvm](http://clang.llvm.org/get_started.html). You'll need to get llvm,
clang, and the extra clang tools (you won't need Compiler-RT or libcxx).
If you don't have it, you'll also need to install CMake as a part of this
process.

Instead of building with `"Unix Makefiles"`, generate build files for Ninja with
```
cmake -GNinja \
    -DLLVM_ENABLE_PROJECTS=clang;clang-tools-extra \
    -DCMAKE_BUILD_TYPE=Release \
    ../llvm
```

Then, instead of using `make`, use ninja to build the clang-tidy binary with
```
ninja clang-tidy
```

This binary will be at (build)/bin/clang-tidy.

If you intend to use the `fix` feature of clang-tidy, you'll also need to build
the `clang-apply-replacements` binary.
```
ninja clang-apply-replacements
```

## Running clang-tidy

Running clang-tidy is (hopefully) simple.
1.  Build chrome normally.
```
ninja -C out/Release chrome
```
2.  Generate the compilation database
```
tools/clang/scripts/generate_compdb.py -p out/Release > out/Release/compile_commands.json
```
3.  Enter the build directory.
```
cd out/Release
```
4.  Run clang-tidy.
```
<PATH_TO_LLVM_SRC>/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py \
    -p . \# Set the root project directory, where compile_commands.json is.
    # Set the clang-tidy binary path, if it's not in your $PATH.
    -clang-tidy-binary <PATH_TO_LLVM_BUILD>/bin/clang-tidy \
    # Set the clang-apply-replacements binary path, if it's not in your $PATH
    # and you are using the `fix` behavior of clang-tidy.
    -clang-apply-replacements-binary \
        <PATH_TO_LLVM_BUILD>/bin/clang-apply-replacements \
    # The checks to employ in the build. Use `-*` to omit default checks.
    -checks=<CHECKS> \
    -header-filter=<FILTER> \# Optional, limit results to only certain files.
    -fix \# Optional, used if you want to have clang-tidy auto-fix errors.
    chrome/browser # The path to the files you want to check.

Copy-Paste Friendly (though you'll still need to stub in the variables):
<PATH_TO_LLVM_SRC>/clang-tools-extra/clang-tidy/tool/run-clang-tidy.py \
    -p . \
    -clang-tidy-binary <PATH_TO_LLVM_BUILD>/bin/clang-tidy \
    -clang-apply-replacements-binary \
        <PATH_TO_LLVM_BUILD>/bin/clang-apply-replacements \
    -checks=<CHECKS> \
    -header-filter=<FILTER> \
    -fix \
    chrome/browser
```

\*It's not clear which, if any, `gn` flags may cause issues for
`clang-tidy`. I've had no problems building a component release build,
both with and without goma. if you run into issues, let us know!

## Questions

Questions? Reach out to rdevlin.cronin@chromium.org or thakis@chromium.org.
Discoveries? Update the doc!
