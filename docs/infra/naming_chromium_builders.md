# Naming Chromium Builders

[TOC]

## TLDR -

Name builders as:
`target ("-" build_descriptor)* "-" build_type ("-" purpose_descriptor)*`

Examples: `win64-rel`, `win64-msvc-rel` , `linux-chromeos-rel`,
`ios-blink-rel-fyi`

## Introduction and Requirements

Chromium's continuous integration infrastructure is organized around builders.
Builders provide a specification for what code to check out, what targets to
build, compile arguments to use, and which tests to run. This document provides
a naming convention for builders to improve consistency and clarity.

This document only applies to the chromium/chrome projects and their respective
branch projects (including perf, memory, etc.); this does not include other
projects like Skia, ChromeOS, V8, etc. or the infra projects.

## General notes on naming

* Names should use only lowercase letters. Mixed case, while potentially more
  readable, complicates accurate transcription and requires more rules.
* Names should not have whitespace in them. Spaces make things potentially
  easier to read, but complicate accurate transcription.
* Use a dash `-` to separate parts of a name.
* The more consistency you can have between related things, the better.
  "Linux Builder" and "Linux Tests" vs. "linux_chromium_rel_ng" is bad;
  "ios-simulator" for both is better though by itself it isn't sufficiently
  unique.
* The less redundancy and noise you have, the better. If all of your bots have
  "chromium" in the name (or "_ng", for that matter), you're better off
  eliminating it.
* Names should generally follow the pattern of identifiers separated by
  punctuation. Specifically, we should follow the unicode identifier syntax
  (probably the lowercase ASCII subset, i.e., the `[a-z][a-z0-9_]` subset that
  is a legal identifier in C, Java, C++, Go and Python). We should use the dash
  character to separate identifiers.

### Builders

`target ("-" build_descriptor)* "-" build_type ("-" purpose_descriptor)*`

This can be broken down as:

- **target**: What to build/test (platform, arch, test suite).
  * linux, win, mac, ios, android, etc.
- **build_descriptor**: Modifies how to build or test.
  * Any non obvious build descriptions that you wouldn't assume from the target
    and bucket.
  * This assumes that you will normally be able to either infer about the host
    configuration, e.g. we can assume that "android" builds run on a linux x64
    host. All exceptions can be handled via build_descriptors.
  * An example would be if a builder within the chrome builder group is doing a
    build on Chromium.
  * i.e {bucket}/{builder} `chrome/linux-chromium-rel` - Here we specify
    chromium build, however, if the bucket were chromium, we would just call
    the builder `linux-rel`.
- **build_type**: `rel`, `dbg`, `official`.
  * Release builder (rel) - Builds binaries with default/base level of compiler
    optimizations.
  * Debug (dbg) - Used for builders run in debug mode. This is typically done
    by passing in debug flag parameters, often running slower than release but
    providing more information.
  * Official - Builds binaries with an extra level of compiler optimizations.
    More information can be found [here](https://source.chromium.org/chromium/chromium/src/+/main:build/config/BUILDCONFIG.gn;drc=b026de546dc95db23f84584a395a9d3b31910342;l=126-130).
- **purpose_descriptor**: How to treat the builder (e.g. `fyi`, `rel-ready`).
  * A catch all descriptor that would differentiate a builders purpose from
    its original base name of `{target}` + `{build_descriptor}` +
    `{build_type}`
  * We should only use this if there is a substantial change on the builder
    that does not fall within a target, build_descriptor or build_type.
  * An example of this is making a builder `fyi` (for your information) thus
    denoting the builder is probably not a gardened builder that is used to
    provide data.

### Examples:

**linux-rel:** From a quick glance we can see that descriptors `cft` and
`no-external-ip` are same builders as `linux-rel` but one builds and tests use
Chrome for Testing and another controls the ability for linux-rel builders to
show an external ip address.

* `linux-rel`: Linux builder with a release build type. The omitted descriptor
  indicates no special configuration.
* `linux-rel-cft`: A `linux-rel` builder with the `cft` (Chrome for testing)
  purpose descriptor.
* `linux-rel-no-external-ip`: A `linux-rel` builder that runs in an environment
  with no external IP address. Here we can see that `no-external-ip` changes
  the nature of the builder but does not fall under `build_type`, `target`, or
  `build_descriptor` thus it falls under `purpose_descriptor`.

**linux-chromeos:** This grouping is easy to understand. We start off with a
Linux builder that runs tests for Chrome on ChromeOs. Note, this differs from
real CrOS builds. Information on why it's different [here](https://chromium.googlesource.com/chromium/src/+/main/docs/chromeos_build_instructions.md#Chromium-OS-on-Linux-linux_chromeos).
The use of easy to understand descriptors (`code-coverage`, `annotator`,
`archive`) tells us information that help us distinguish linux-chromeos builders
at a quick glance.

* `linux-chromeos-rel`: A builder for the `linux-chromeos` target with a
  release build.
* `linux-chromeos-dbg`: A builder for the `linux-chromeos` target with a debug
  build.
* `linux-chromeos-annotator-rel`: A `linux-chromeos` release builder with the
  `annotator` descriptor.
* `linux-chromeos-archive-rel`: A `linux-chromeos` release builder that
  additionally archives the build.
* `linux-chromeos-code-coverage`: A `linux-chromeos` builder for code coverage.

**win-rel:** Notice strong base name `win-rel`. It is easy to have a regex find
all builders related to `win-rel` with descriptors being concise and clear.

* `win-rel`: Windows builder with a release build.
* `win-msvc-rel`: A `win-rel` builder that uses the Microsoft Visual C
  compiler.
* `win-msvc-dbg`: A Windows builder that uses the Microsoft Visual C compiler
  and has a debug build type.

### Testers

Testers are builders that are triggered by another builder. The tester will run
tests that were built by the triggering builder.

`builder_name ("-" descriptor)* "-tests"`

**Examples:**
* `android-cronet-arm-rel-kitkat-tests`
* `android-cronet-arm-rel-lollipop-tests`
* `mac-arm64-rel-tests`

If there are multiple testers triggered by a single builder, their names must
be distinct, so all but one will require descriptors. In such a case, it is
recommended that each tester include at least one descriptor unless it is 2
builders and they are differentiated with a boolean descriptor (e.g. `noncq`).
