Skia is a complete 2D graphic library for drawing Text, Geometries, and Images.

The Skia library can be found in `//third_party/skia`, and full documentation
is available at https://skia.org/

This directory includes low-level chromium utilities for interacting with Skia:

* Build rules for the Skia library
* Configuration of the library (`config/SkUserConfig.h`)
* Serialization of Skia types (`public/mojom`)
* Implementations of Skia interfaces for platform behavior, such as fonts and
  memory allocation, as well as other miscellaneous utilities (`ext`).

Note that Skia is used directly in many parts of the chromium codebase.
This directory is only concerned with code layered on Skia that will be reused
frequently, across multiple chromium components.
