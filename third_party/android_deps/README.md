Android Deps Repository Generator
---------------------------------

Tool to generate a gradle-specified repository for Android and Java
dependencies.

### Usage

    fetch_all.py [--help]

This script creates a temporary build directory, where it will, for each
of the dependencies specified in `build.gradle`, take care of the following:

  - Download the library
  - Generate a README.chromium file
  - Download the LICENSE
  - Generate a GN target in BUILD.gn
  - Generate .info files for AAR libraries
  - Generate 3pp subdirectories describing the CIPD packages
  - Generate a `deps` entry in DEPS.

It will then compare the build directory with your current workspace, and
print the differences (i.e. new/updated/deleted packages names).

### Adding a new library or updating existing libraries.
Full steps to add a new third party library or update existing libraries:

1. Update `build.gradle` with the new dependency or the new versions.

2. Run `fetch_all.py` to update your current workspace with the changes. This
   will update, among other things, your top-level DEPS file.

3. `git add` all the 3pp related changes and create a CL for review. Revert all
   of the non-3pp files as they will be committed in a followup CL.

4. Land the first CL in step 3 and wait for the corresponding 3pp packager to
   create the new CIPD packages. See [`//docs/cipd_and_3pp.md`][cipd_and_3pp_doc]
   for how it works.

5. Run `fetch_all.py` again. There should not be any 3pp related changes. Create
   a commit.

   If the CL is doing more than upgrading existing packages or adding packages
   from the same source and license (e.g. gms) follow
   [`//docs/adding_to_third_party.md`][docs_link] for the review.

If you are updating any of the gms dependencies, please ensure that the license
file that they use, explained in the [README.chromium][readme_chromium_link] is
up-to-date with the one on android's [website][android_sdk_link], last updated
date is at the bottom.

[cipd_and_3pp_doc]: ../../docs/cipd_and_3pp.md
[owners_link]: http://go/android-deps-owners
[docs_link]: ../../docs/adding_to_third_party.md
[android_sdk_link]: https://developer.android.com/studio/terms
[readme_chromium_link]: ./README.chromium

### Implementation notes:
The script invokes a Gradle plugin to leverage its dependency resolution
features. An alternative way to implement it is to mix gradle to purely fetch
dependencies and their pom.xml files, and use Python to process and generate
the files. This approach was not as successful, as some information about the
dependencies does not seem to be available purely from the POM file, which
resulted in expecting dependencies that gradle considered unnecessary.
