Android Deps Repository Generator
---------------------------------

Tool to generate a gradle-specified repository for Android and Java
dependencies.

### Usage

    fetch_all.py [--help]

This script creates a temporary build directory, where it will, for each of the
dependencies specified in `build.gradle`, take care of the following:

  - Download the library
  - Generate a README.chromium file
  - Download the LICENSE
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate 3pp subdirectories describing the CIPD packages
  - Generate a `deps` entry in DEPS.

It will then compare the build directory with your current workspace, and print
the differences (i.e. new/updated/deleted packages names).

### Adding a new library or updating existing libraries.
Full steps to add a new third party library or update existing libraries:

1. Update `build.gradle` with the new dependency or the new versions.

2. Run `fetch_all.py --local` to update your current workspace with the changes.
   This will update, among other things, your top-level DEPS file. If this is a
   new library, you can skip directly to step 5 since the next step is not going
   to work for you.

3. Run `gclient sync` to make sure that cipd has access to the versions you are
   trying to roll. This might fail with a cipd error failing to resolve a tag.

4. If the previous step works, upload your cl and you are done, if not continue
   with the steps.

5. Add a `overrideLatest` property override to your package in
   `ChromiumDepGraph.groovy` in the `PROPERTY_OVERRIDES` map, set it to `true`.

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

If the CL is doing more than upgrading existing packages or adding packages
from the same source and license (e.g. gms) follow
[`//docs/adding_to_third_party.md`][docs_link] for the review.

If you are updating any of the gms dependencies, please ensure that the license
file that they use, explained in the [README.chromium][readme_chromium_link] is
up-to-date with the one on android's [website][android_sdk_link], last updated
date is at the bottom.

[3pp_bot]: https://ci.chromium.org/p/chromium/builders/ci/3pp-linux-amd64-packager
[cipd_and_3pp_doc]: ../../docs/cipd_and_3pp.md
[owners_link]: http://go/android-deps-owners
[docs_link]: ../../docs/adding_to_third_party.md
[android_sdk_link]: https://developer.android.com/studio/terms
[readme_chromium_link]: ./README.chromium

### Implementation notes:
The script invokes a Gradle plugin to leverage its dependency resolution
features. An alternative way to implement it is to mix gradle to purely fetch
dependencies and their pom.xml files, and use Python to process and generate the
files. This approach was not as successful, as some information about the
dependencies does not seem to be available purely from the POM file, which
resulted in expecting dependencies that gradle considered unnecessary. This is
especially true nowadays that pom.xml files for many dependencies are no longer
maintained by the package authors.

#### Groovy Style Guide
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