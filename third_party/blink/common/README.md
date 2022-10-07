# Blink 'common' directory

This directory contains the common Web Platform stuff that needs to be shared
by renderer-side and browser-side code.

Things that live in `third_party/blink` can directly depend on this directory,
while the code outside the Blink directory (e.g. `//content` and `//chrome`)
can only depend on the common stuff via the public headers exposed in
`blink/public/common`.

Anything in this directory should **NOT** depend on the non-common stuff
in the Blink directory. See `DEPS` and `BUILD.gn` files for more details.

Code in this directory would normally use `blink` namespace.

Unlike other directories in Blink, code in this directory should:

* Use Chromium's common types (e.g. //base ones) rather than Blink's ones
  (e.g. WTF types)

* Follow [Chromium's common coding style guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
