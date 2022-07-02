This directory contains the configurations of which binaries get archived on
Chromium builders, and controls how and where they're stored. This applies
mostly to the `*-archive-*` builders on this
[console](https://ci.chromium.org/p/chromium/g/chromium/console).

Each JSON file here corresponds to the archive configuration for a single
builder. See the
[properties.proto](https://source.chromium.org/chromium/chromium/tools/build/+/main:recipes/recipe_modules/archive/properties.proto;drc=cca630e6c409dcdcc18567b94fcdc782b337e0ab;l=270)
definition of the archive recipe module for the schema of these files.
