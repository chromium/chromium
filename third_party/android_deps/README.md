# Android Deps

[TOC]

Chromium's way to pull prebuilt .jar / .aar files from Maven.

There are 2 roots for libraries:

1. `//third_party/androidx`
   * Contains all androidx libraries listed in `//third_party/androidx/build.gradle.template`
   * Pulls from daily snapshots hosted on https://androidx.dev
   * Libraries are combined into a single CIPD instance by [android-androidx-packager]
   * Auto-rolled by [androidx-chromium]

2. `//third_party/android_deps`
   * Contains all other libraries, listed in `//third_party/android_deps/build.gradle.template`
   * Also contains the scripts and gradle plugin used by both roots
   * All libraries are combined into a single CIPD instance by [android-androidx-packager] (out of convenience).
   * Auto-rolled by [android-deps-chromium]

This system supports deps between roots, but since they roll separately,
such deps can require manually rolling multiple roots atomically, and sometimes
explicitly adding dependent libraries to `build.gradle{.template}` files.

[androidx-chromium]: https://autoroll.skia.org/r/androidx-chromium
[android-deps-chromium]: https://autoroll.skia.org/r/android-deps-chromium
[android-androidx-packager]: https://ci.chromium.org/ui/p/chromium/builders/ci/android-androidx-packager

## Adding a new Library

See first: [`//docs/adding_to_third_party.md`].

For AndroidX libries, see [`//third_party/androidx/README.md`]

[`//docs/adding_to_third_party.md`]: /docs/adding_to_third_party.md
[`//third_party/androidx/README.md`]: /third_party/androidx/README.md

### Adding an Autorolled Library

1. Add the gradle entry for the desired target to `//third_party/android_deps/build.gradle.template`
2. Do a trial run (downloads files locally):
   ```
   third_party/android_deps/fetch_all.py --local
   ```
3. Assuming it works fine, upload & submit your change to `build.gradle.template`
4. Wait for the [android-androidx-packager] and [android-deps-chromium] to run (or [trigger the packager manually] to expedite)

[trigger the packager manually]: https://luci-scheduler.appspot.com/jobs/chromium/android-androidx-packager

## Common Issues

### Missing Metadata

E.g. missing license, HTML in license, missing URL or description:

* Add an entry to [`PROPERTY_OVERRIDES`]

[`PROPERTY_OVERRIDES`]: /third_party/android_deps/buildSrc/src/main/groovy/ChromiumDepGraph.groovy

### BUILD.gn Needs Customization

* For AndroidX, add an entry to `//third_party/androidx/customizations.gni`
* For others, add an entry to [`addSpecialTreatment()`], or for hand-written
  targets, edit `//third_party/android_deps/overrides.gni` (`BUILD.gn` is
  overwritten by the roller)

[`addSpecialTreatment()`]: /third_party/android_deps/buildSrc/src/main/groovy/BuildConfigGenerator.groovy

## Implementation Notes

The script invokes a Gradle plugin to leverage its dependency resolution
features. An alternative way to implement it is to mix gradle to purely fetch
dependencies and their pom.xml files, and use Python to process and generate the
files. This approach was not as successful, as some information about the
dependencies does not seem to be available purely from the POM file, which
resulted in expecting dependencies that gradle considered unnecessary. This is
especially true nowadays that pom.xml files for many dependencies are no longer
maintained by the package authors.

### Groovy Style Guide

The groovy code in `//third_party/android_deps/buildSrc/src/main/groovy` is best
edited using Android Studio (ASwB works too). This code can be auto-formatted by
using Android Studio's code formatting actions.

The easiest way to find these actions is using `Ctrl+Shift+A` and then typing
the name of the action to be performed (e.g. `reformat`). Another easy way is
setting up `Settings>Tools>Actions on Save>Reformat code` and
`..>Optimize imports`.

The current code is formatted using a specific code style, you can import
`//third_party/android_deps/Chromium_Groovy.xml` via
`Settings>Editor>Code Style>[settings gear]>Import Scheme...`.
