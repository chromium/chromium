# The Mojom Parser

The Mojom format is an interface definition language (IDL) for describing
interprocess communication (IPC) messages and data types for use with the
low-level cross-platform
[Mojo IPC library](https://chromium.googlesource.com/chromium/src/+/main/mojo/public/c/system/README.md).

This directory consists of a `mojom` Python module, its tests, and supporting
command-line tools. The Python module implements the parser used by the
command-line tools and exposes an API to help external bindings generators emit
useful code from the parser's outputs.

TODO(crbug.com/40122045): Fill out this documentation once the library
and tools have stabilized.
