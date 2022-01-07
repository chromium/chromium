# Mojo Rust Public Bindings

This crate contains the public Mojo bindings for Rust.

## Build Instructions within Mojo Source Tree

In the Mojo source tree, this crate gets compiled directly with examples, so for
instructions on how to use those examples, please see the //examples directory
at the root of the repository.

Additionally, the Rust tests run as part of the overarching test suite, invoked
by running

```bash
mojo/tools/mojob.py test
```

from the root directory.

## Standalone Build Instructions

1.  Build Mojo Dependencies (from source root)

```bash
mojo/tools/mojob.py gn
mojo/tools/mojob.py build
```

2.  Build with Cargo (from this directory)

First, set the Mojo output directory in order for Cargo to be able to find the
Mojo dependencies:

```bash
export MOJO_OUT_DIR=/path/to/out/Debug
```

Then build normally using Cargo:

```bash
cargo build [--release]
```

Additional non-standard environment variables that Cargo will respond to:
*   MOJO_RUST_NO_EMBED - Specifies to not bundle the Mojo embedder or link
    against libstdc++, however tests will not work.

3.  Test

Note: if you set MOJO_RUST_NO_EMBED, running tests will fail.

```bash
cargo test [--release]
```

