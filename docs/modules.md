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
with the GN arg `use_libcxx_modules. Using this arg is not currently
recommended, due to the limitations mentioned below.
It is only interesting to people working on the feature.

## Current limitations

### Performance

With Chrome's Clang plugins turned on, modules perform worse than without
modules ([crbug.com/351909443](https://crbug.com/351909443)).

### Configurations

Clang modules don't play nice with code with RTTI / exceptions depending on
code without, and vice versa. Work is ongoing to fix this, but for now, it
remains a problem ([crbug.com/403415459](https://crbug.com/403415459)).

### Correctness

The configurations issue above can cause unexpected build failures.
