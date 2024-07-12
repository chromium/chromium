# Clang and C++ Modules in Chromium

## Overview

Modules in C++ have the potential to significantly speed up building Chromium.
With textual inclusion, many headers are re-parsed over
[35000 times](https://commondatastorage.googleapis.com/chromium-browser-clang/include-analysis.html)
and with modules could be reduced to the order of 10 times.

Clang supports two types of modules:
[Clang header modules](https://clang.llvm.org/docs/Modules.html) and
[C++20 modules](https://clang.llvm.org/docs/StandardCPlusPlusModules.html) (also
called Clang Modules and Standard C++ Modules). We're currently exploring Clang
header modules because:
1. Other projects (e.g. internally at Google) have had success deploying them to
their code bases with large performance wins.
2. They can be experimented with without rewrites to the code base or changes to
the build system.

We're currently experimenting with modules for libc++ and they can be enabled
with the GN arg `use_libcxx_modules`. Using this arg is not currently
recommended, due to the limitations mentioned below. It is only interesting to
people working on the feature.

## Current limitations

### Implicit vs explicit modules

We're using implicit modules, which are created on-the-fly when Clang doesn't
see them in the module cache. This doesn't work with remote execution since the
cached modules aren't known to the build system.

The module cache is set to `<outdir>/gen/libcxx/module_cache`. Since the modules
aren't known to ninja they aren't cleaned with `ninja -t clean` and need to be
manually deleted for a clean build.

We will eventually switch to explicit modules to address these issues, which
will require support in GN and has been partially implemented
([CL1](https://gn-review.googlesource.com/c/gn/+/9601),
[CL2](https://gn-review.googlesource.com/c/gn/+/9602),
[CL3](https://gn-review.googlesource.com/c/gn/+/9680), and
[crbug.com/gn/373](https://crbug.com/gn/373)).

### Duplicate modules

Multiple pcm files are created per module. For correctness, Clang header modules
default to using a hash of command line args in the module path. For compiling
`base`, we have 19 different flag combinations and ~700 pcm files are created
per flag combination for 13483 total pcms.

Some flag combinations produce incompatible modules, like whether RTTI is turned
on or off. For most others, we expect that the resulting modules from slight
flag variations (e.g. setting include preprocessor defines unrelated to libc++)
are compatible with each other and can be reused.

### Performance

With Chrome's Clang plugins turned on, modules perform worse than without
modules even if fully cached ([crbug.com/351909443](https://crbug.com/351909443)).

Building with modules and a cold module cache is much slower than without
modules. This seems unexpected since Clang should still be doing less work.

### Correctness

When the module cache is cold, there are occasional build failures when ninja
parallelism is high enough. I don't see it with 20 jobs but see it with 130 jobs
([crbug.com/351865290](https://crbug.com/351865290)).
