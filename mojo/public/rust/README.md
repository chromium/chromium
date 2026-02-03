This directory contains the code that generates mojom bindings for rust code.
It is still under active development, and not yet in a usable state.

For more information, see [Creating Rust Bindings for Mojo](https://docs.google.com/document/d/18TvtLIfvxQ_beZrQ5dhxaFwlw4FBZa1VX10aUoAyY9s/edit?usp=sharing)

FOR_RELEASE: Fill this out more!

# Layers
FOR_RELEASE: Actually explain what this means
Unlike C++, we've got three layers on top of the C bindings:
1. (mostly) Safe, idiomatic, but still low-level FFI bindings (system/ffi)
2. Ergonomic, mid-level bindings (system)
3. High-level, often Mojom-specific code (bindings)

C++ just has (2) and (3), building (2) directly on the C code.