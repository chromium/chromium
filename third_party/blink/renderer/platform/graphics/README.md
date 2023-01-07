<!---
  The live version of this document can be viewed at:
  https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/platform/graphics/README.md
-->

# Platform graphics code

This directory contains graphics support code with minimal external dependencies
(e.g., no references to [`core`](../../core)). The main subdirectories are:

- [`compositing`](compositing/README.md) -- Contains the implementation
  of the "blink compositing algorithm".
- [`darkmode`](darkmode/README.md) -- Dark mode neural network classifier.
- `filters` -- Filter effects.
- `gpu` -- GPU-accelerated support code.
- [`paint`](paint/README.md) -- Contains the implementation of display lists and
  display list-based painting.
