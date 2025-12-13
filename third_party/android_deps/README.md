# Android Deps

[TOC]

Chromium's way to pull prebuilt .jar / .aar files from Maven.

There are 3 roots for libraries:

1. `//third_party/androidx`
   * Contains all androidx libraries listed in `//third_party/androidx/build.gradle.template`
   * Pulls from daily snapshots hosted on https://androidx.dev
   * Libraries are combined into a single CIPD instance by [android-androidx-packager]
   * Auto-rolled by [androidx-chromium]

2. `//third_party/android_deps/autorolled`
   * Contains deps reachable from `//third_party/android_deps/autorolled/build.gradle.template`
   * All libraries are combined into a single CIPD instance by [android-androidx-packager] (out of convenience).
   * Auto-rolled by [android-deps-chromium]

3. `//third_party/android_deps`
   * This was the original root, and thus contains scripts used by the other two
   * Contains deps reachable from `//third_party/android_deps/build.gradle`
   * Each library is packaged into its own CIPD package
   * Not auto-rolled

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

### Adding an Autorolled Library (Preferred)

1. Add the gradle entry for the desired target to `//third_party/android_deps/autorolled/build.gradle.template`
2. Do a trial run (downloads files locally):
   ```
   third_party/android_deps/autorolled/fetch_all_autorolled.py --local
   ```
3. Assuming it works fine, upload & submit your change to `build.gradle.template`
4. Wait for the [android-androidx-packager] and [android-deps-chromium] to run (or [trigger the packager manually] to expedite)

[trigger the packager manually]: https://luci-scheduler.appspot.com/jobs/chromium/android-androidx-packager

### Adding a Non-Autorolled Library

1) Add the gradle entry for the desired target to `//third_party/android_deps/build.gradle`
2) Do a trial run (downloads files locally):
   ```
   third_party/android_deps/fetch_all.py --local
   ```
3) Assuming it works fine, upload & submit you changes to `build.gradle` and everything in `libs/`.
   * Revert your local changes to `DEPS` and `BUILD.gn`
4) Wait for the [3pp-linux-amd64-packager] to run (or [trigger it manually] to expedite)
5) Run `fetch_all.py --local` again, and this time commit all the changes.

[3pp-linux-amd64-packager]: https://ci.chromium.org/ui/p/chromium/builders/ci/3pp-linux-amd64-packager
[trigger it manually]: https://luci-scheduler.appspot.com/jobs/chromium/3pp-linux-amd64-packager

### Adding or Updating a Non-Autorolled Library

1. Update `build.gradle` with the new dependency or the new versions.

2. Run `fetch_all.py --local` to update your current workspace with the changes.
   This will update, among other things, your top-level `DEPS` file. If this is a
   new library, you can skip directly to step 5 since the next step is not going
   to work for you.

3. Run `gclient sync` to make sure that cipd has access to the versions you are
   trying to roll. This might fail with a cipd error failing to resolve a tag.

4. If the previous step works, upload your cl and you are done, if not continue
   with the steps.

5. Add a `overrideLatest` property override to your package in
   `ChromiumDepGraph.groovy` in the [`PROPERTY_OVERRIDES`] map, set it to `true`.

6. Run `fetch_all.py --local` again.

7. `git add` all the 3pp related changes and create a CL for review. Keep the
   `3pp/`, `.gradle`, `OWNERS`, `.groovy` changes in the CL and revert the other
   files. The other files should be committed in a follow up CL. Example git
   commands:
   * `git add third_party/android_deps{*.gradle,*.groovy,*3pp*,*OWNERS,*README.md}`
   * `git commit -m commit_message`
   * `git restore third_party/android_deps DEPS`
   * `git clean -id`

8. Land the first CL in the previous step and wait for the corresponding 3pp
   packager to create the new CIPD packages. The 3pp packager runs every 6
   hours.  You can see the latest runs [here][3pp_bot]. See
   [`//docs/cipd_and_3pp.md`][cipd_and_3pp_doc] for how it works. Anyone on the
   Clank Commons team and any trooper can trigger the bot on demand for you.

9. If your follow up CL takes more than a day please revert the original CL.
   Once the bot uploads to cipd there is no need to keep the modified 3pp files.
   The bot runs 4 times a day. When you are ready to land the follow up CL, you
   can land everything together since the cipd packages have already been
   uploaded.

10. Remove your `overrideLatest` property override entry in
    `ChromiumDepGraph.groovy` so that the 3pp bot goes back to downloading and
    storing the latest versions of your package so that it is available when you
    next try to roll.

11. Run `fetch_all.py --local` again. Create a CL with the changes and land it.

[3pp_bot]: https://ci.chromium.org/p/chromium/builders/ci/3pp-linux-amd64-packager
[cipd_and_3pp_doc]: ../../docs/cipd_and_3pp.md
[`PROPERTY_OVERRIDES`]: /third_party/android_deps/buildSrc/src/main/groovy/ChromiumDepGraph.groovy

## Common Issues

### Missing Metadata

E.g. missing license, HTML in license, missing URL or description:

* Add an entry to [`PROPERTY_OVERRIDES`]

### BUILD.gn Needs Customization

* For AndroidX, add an entry to `//third_party/androidx/customizations.gni`
* For others, add an entry to [`addSpecialTreatment()`]

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
