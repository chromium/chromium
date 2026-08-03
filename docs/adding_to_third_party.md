# Adding third_party Libraries

[TOC]

Using third party code can save time and is consistent with our values - no need
to reinvent the wheel! We put all code that isn't written by Chromium developers
into `//third_party`. We do this to make it easy to track license compliance,
security patches, and supply the right credit and attributions. It also makes it
a lot easier for other projects that embed our code to track what is Chromium
licensed and what is covered by other licenses.

# Put the code in //third_party

By default, all third party code should be checked into
[//third_party](../third_party/), for the reasons given above.

There is one primary exception to this, which is that if a third_party
dependency has its own dependencies *and* it can be built on its own (without
Chromium), you can check its dependencies into its third_party. For example,
Dawn is a project that is developed independently of Chromium, and
it has a dependency on GLFW (which Chromium does not have). Dawn
can check that dependency into its `//third_party/glfw3/src`, and in a Chromium
checkout, that will show up at `//third_party/dawn/third_party/glfw3/src`.
That is okay, but it'd be better if we could add GLFW to a Chromium
checkout (in chromium/src's `third_party/glfw`) and configure Dawn
to use that location when it is being built as part of Chromium.

However, if that dependency is also needed by Chromium or another
of Chromium's dependencies, then it must be checked out into Chromium's
//third_party (i.e., now you have to use `//third_party/glfw`). This
prevents us from possibly needing to use two different versions of a
dependency.

Apart from that, other locations are only appropriate in a few
situations and need explicit approval; don't assume that because there's some
other directory with third_party in the name it's okay to put new things
there.

Regardless of where you add a third party dependency, you should use the
[recommended directory structure](#standard-dep-structure).

# Before you start

NOTE: You can skip this step when you are only moving existing directories in
Chromium to //third_party/.

To make sure the inclusion of a new //third_party project makes sense for the
Chromium project, you should first obtain approval from Chrome's Technical
Steering Council and Chrome Security. Please include the following topics in an
email to chromium-third-party-libraries at chromium.org (this will trigger a
review by both parties - you're good to go once you have both signoffs on the
email thread):

1.  Link to the upstream repository.

2.  Motivation:

    What does the library do, and how does that functionality support Chromium's
    goals? Are there alternatives you considered, either dependencies we already
    include, or other libraries we could have chosen? Linking to a design doc
    (and ensuring it's shared appropriately!) would be ideal.

3.  Security:

    Is the library written in a
    [memory-safe language](security/rule-of-2.md#unsafe-implementation-languages)?
    Does it process untrusted input? What's the integration strategy (e.g. will
    it run in a utility process, as part of the browser process, etc)? Will the
    library be built from source, or distributed as a pre-built binary? In the
    latter case, please explain why it can't be built from source, as you'll need
    explicit approval.

4.  Maintenance:

    You will be responsible for [owning the dependency](#add-owners), keeping it
    up to date with security and stability fixes. Third party dependencies must
    be [kept up to date](#fresh-deps), so you'll need a plan to autoroll it. If
    it cannot be autorolled, you will need an exception, as discussed in
    "[Update Mechanism](#update-mechanism)" and
    "[Autoroll Exceptions](#autoroll-exceptions)" below.

    Likewise, how well-maintained is the upstream on all the platforms Chromium
    supports? What is the expected maintenance burden on the Chromium project
    over time?

5.  Weight:

    What is the expected build time and binary size impact? If you have
    an active CL, the [compile-size](speed/binary_size/compile_size_builder.md)
    and [android-binary-size](speed/binary_size/android_binary_size_trybot.md)
    bots are good data points to link to as good proxies for speed and size
    implications. If the expected APK increase is greater
    than 16kb, consider analyzing the library via tools like
    [Chrome Supersize](../tools/binary_size/libsupersize/README.md) to trim
    unnecessary overhead.

    What's the impact on checkout size? If it's significant (e.g. >20MB),
    consider filtering the files to be checked in.

    Are there performance or memory implications to be aware of, especially on
    Android?

## Rust

Rust is allowed for third party libraries. Unlike C++ libraries, Rust third
party libraries are [regularly rolled to updated versions by a
rotation](https://chromium.googlesource.com/chromium/src/tools/+/HEAD/crates/create_update_cl.md)
and can be audited for unsafety.
The process for importing a new Rust third-party dependency is documented at
[`//third_party/rust/README-importing-new-crates.md`](../third_party/rust/README-importing-new-crates.md)
(Googlers can also find additional considerations at go/chrome-rust).

Email rust-dev@chromium.org with any questions about the Rust toolchain.

Note that new Rust crates do not require security approval - the Rust
third_party approval is sufficient. Likewise, minor version updates for crates
are regularly handled by a rotation; you'll only be responsible for major
version updates (and minor version updates which result in `gnrt` or Chromium
CQ failures).

## A note on size constraints

The size of Chromium derived executables can impact overall performance of those binaries as they
need to run on a wide range of devices including those with extremely limited RAM. Additionally, we
have experience from Windows of the binary size impacting successful patch rate of updates as well
as constraints from the Android Ecosystem where APKs included in the system image have hard
limits on their size due to allocation size of the system partition. For more details and
guidelines on size increases see
[//docs/speed/binary_size/binary_size_explainer.md](speed/binary_size/binary_size_explainer.md) and Googlers can
additionally check [go/chrome-binary-size](https://goto.google.com/chrome-binary-size)

## Binaries, obfuscated or minified code

The addition of third-party dependencies that contain binaries, obfuscated
code, or minified code is strongly discouraged. Code review is an important
part of reducing risk to Chromium and a reviewer asked to approve a change
that contains any of these has no way to determine the legitimacy of what
they are approving. Minification for performance optimization is
[usually not necessary](speed/binary_size/optimization_advice.md), and the
trade-off in terms of understandability and security is rarely worth
it.

Where your dependency will form part of a release binary where size is a concern,
there are existing tools which handle [compression for distribution](speed/binary_size/optimization_advice.md).

You should not check in any pre-built binaries where there is an alternate,
supported solution for getting them. If you need to compile from source,
consider using [CIPD](cipd_and_3pp.md) instead.

This is accessible to Googlers only. Non-Googlers can email one of the people
in third_party/OWNERS for help.

See [Chrome Code Policy](https://goto.google.com/chrome-code-policy)

# Keeping dependencies up to date (fresh) {#fresh-deps}

It is Chromium policy that third party dependencies be kept up to date, or
"fresh". Fresh means:

* There is no newer minor or patch version that has been available for 3+ months.
* There is no newer major version that has been available for 1 year or more.
* The upstream project has had a release within the last 2 years.

The easiest way to ensure your dependency meets this criteria is to onboard it
to an autoroller. Refer to
[managing-third-party](https://chromium.googlesource.com/chromium/src/+/main/docs/managing-third-party/)
for options and user guides. While the autoroller will detect new versions as
they become available and attempt to roll them into Chromium, OWNERS are
responsible for resolving any issues which arise as a result of the update and
landing the update as soon as feasible. If you cannot autoroll your dependency,
you will need an [autoroll exception](#autoroll-exception).

# Get the code

There are two common ways to depend on third-party code: you can reference a
Git repo directly (via entries in the DEPS file) or you can check in a
snapshot (vendor the code). The former is preferable in most cases:

1. If you are actively developing in the upstream repo, then having the DEPS
   file include the upstream (that's been mirrored to GoB, see [here](/docs/dependencies.md#adding-to-GoB))
   can be a way to include those changes into Chromium at a particular revision.
   The DEPS file will be updated to a new revision when you are ready to "roll"
   the new version into Chromium. This also avoids duplicate copies of the code
   showing up in multiple repos leading to contributor confusion.
1. This interacts favorably with our upstream tracking automation. We
   automatically consume the upstream Git hashes and match them against a
   database of known upstreams to tracking drift between Chromium and upstream
   sources.
1. This makes adding deps that don't need local changes easier. E.g. some of
   our automation automatically converts non-GN build rules into GN build rules
   without any additional CLs.

Checking in a snapshot is useful if this is effectively taking on maintenance
of an unmaintained project (e.g. an ancient library that we're going to GN-ify
that hasn't been updated in years). And, of course, if the code you need isn't
in a Git repo, then you have to snapshot/vendor.

## Node packages

To include a Node package, add the dependency to the
[Node package.json](../third_party/node/package.json). Make sure to update
the corresponding [`npm_exclude.txt`](../third_party/node/npm_exclude.txt)
and [`npm_include.txt`](../third_party/node/npm_include.txt) to make the code
available during checkout.

## Pulling the code via DEPS

See [here](/docs/dependencies.md#adding-dependencies).

## Checking in the code directly

If you are checking in a snapshot, you should follow the [standard directory structure](#standard-dep-structure).
For security reasons, please retrieve the code as securely as you can, using
HTTPS and GPG signatures if available.
If retrieving a tarball, please do not check the tarball itself into the tree,
but do list the source and the SHA-512 hash (for verification) in the
README.chromium and Change List. The SHA-512 hash can be computed via
`sha512sum` or `openssl dgst -sha512`.  If retrieving from a git
repository, please list the upstream URL and revision that the code was pulled
from.

If you are checking the files in directly, you do not need an entry in DEPS
and do not need to modify `//third_party/.gitignore`.

## Checking in large files

This is accessible to Googlers only. Non-Googlers can email one of the people
in third_party/OWNERS for help.

See [Moving large files to Google Storage](https://goto.google.com/checking-in-large-files)

# Standard directory structure for dependencies {#standard-dep-structure}

Regardless of how you import a dependency, you should use the following
directory structure. This folder layout enforces separation between first and
third party code, making it easier to manage updates and dependency hygiene
long term.

Any first party code or files you need for dependency management or
interoperability should be added to the top level dependency directory, and the
dependency source imported into the child src directory.

**Recommended directory structure:**
```
❯ //third_party/<dependency-name>
├── BUILD.gn
├── README.chromium
├── OWNERS
├── src <-- import third party code here
│   ├── LICENSE
│   ├── a.h
│   └── b.cc
```

**What constitutes a dependency:**

* A dependency should be sourced from a single upstream location. Putting code
  from multiple upstream sources in a single `//third_party` directory makes it
  difficult to reason about the origin of files and perform automated updates.
* If your dependency has its own vendored dependencies, it's not necessary to
  split these into additional directories.

**Formatting:**

Do not reformat or apply Chromium-style formatting to any code within the
dependency `src` directory. Maintaining the original formatting is essential
for generating clean diffs against upstream versions. This simplifies
reviewing upstream changes, applying security patches, and performing updates.

If you experience issues with submitting a CL due to Chromium formatting
requirements which need to be disabled, or you need to format first party code
in your top level dependency folder, you can add a language appropriate
formatting config (e.g [.clang-format-ignore](https://clang.llvm.org/docs/ClangFormat.html#clang-format-ignore))
to your top level dependency directory. Ensure it does not format the third
party code.


# Document the code's context

## Add OWNERS

Your OWNERS file must either list the email addresses of two Chromium
committers on the first two lines or include a `file:` directive to an OWNERS
file within the `third_party` directory that itself conforms to this criterion.
This will ensure accountability for maintenance of the code over time. While
there isn't always an ideal or obvious set of people that should go in OWNERS,
this is critical for first-line triage of any issues that crop up in the code.

As an OWNER, you're expected to:

* Remove the dependency when/if it is no longer needed
* Update the dependency when a security or stability bug is fixed upstream
* Help ensure the Chrome feature that uses the dependency continues to use the
  dependency in the best way, as the feature and the dependency change over
  time.

# README.chromium

You need a README.chromium file with information about the project from which
you're re-using code. See
[//third_party/README.chromium.template](../third_party/README.chromium.template)
for a list of fields to include. A presubmit check will check this has the right
format.

Make sure to document all security considerations, concerns, and risks in the
`Description:` field of the README.chromium. Third party code is a hot spot
for security vulnerabilities. Help people make informed decisions about
relying on this package by highlighting security considerations.

## Security Critical {#security-critical}
README.chromium files contain a field indicating whether the package is
security-critical or not. A package is security-critical if it is compiled
into the product and does any of the following:

* Accepts untrustworthy inputs from the internet
* Parses or interprets complex input formats
* Sends data to internet servers
* Collects new data
* Influences or sets security-related policy (including the user experience)

## Update Mechanism {#update-mechanism}

The `Update Mechanism:` field specifies how this dependency is kept up-to-date.
You will use one of the following values below. The bug link should be an
approved autoroll exception ([example](https://crbug.com/422921734)).

* `Autoroll`
* `Manual (https://crbug.com/<BUG_ID>)`
* `Static (https://crbug.com/<BUG_ID>)`
* `Static.HardFork (https://crbug.com/<BUG_ID>)`

|||---||| 3,3,6

### Autoroll

Updated automatically by a service.

Please refer to
[managing-third-party](https://chromium.googlesource.com/chromium/src/+/main/docs/managing-third-party/)
for options and user guide.

*** note
### Manual

**An exception is required.**

Updated manually by OWNERS.

Owners may use `roll-dep`, or custom bespoke methods.
***

*** note
### Static / Static.HardFork

**An exception is required.**

*The dependency will be treated as if it's written by Chromium Authors.*

* **Static**: Origin is not a version controlled repository or a package
manager, or the dependency is intentionally never updated, or the upstream Git
repo is archived.
* **Static.HardFork**: Origin is a Git repo or package manager, but our version
has diverged from the upstream, and is no longer updatable.
***
|||---|||

> To maximise vulnerability scanning coverage, OWNERS should provide a
> revision/version/cpe which represents the closest point of divergence
> from the upstream. This may return some false positives but ensures
> coverage is optimal.

### Autoroll Exceptions {#autoroll-exceptions}

You can request your dependency to be exempted from autorolling. You MUST
demonstrate a strong technical need for doing so.

For example, the following justifications ARE NOT sufficient for granting an
exemption:

* The dependency is test-only
* The dependency isn't shipped

The exemption is evaluated on a case-by-case basis by Chrome ATLs and Chrome's
third-party supply chain security team.

To request an exemption, file a bug in
[Chromium > ThirdParty > Autoroll Exceptions](https://issues.chromium.org/issues/new?component=1801247&template=2135097)
and explain your reasons.


### Transitive Dependencies
If a child dependency is rolled with a parent, it should specify an
Update Mechanism that is at _most_ as frequently/automated-ly
updated as the parent.

* If the parent is `Autorolled`, then it could be `Autorolled`, `Manual` or `Static/Static.HardFork`.
* If the parent is `Manual`, then the child can only be `Manual` or `Static/Static.HardFork`.
* If the parent is `Static/Static.HardFork` then it must be `Static/Static.HardFork`.

## CPE Prefix
One of the fields is CPEPrefix. This is used by Chromium and Google systems to
spot known upstream security vulnerabilities, and ensure we merge the fixes
into our third-party copy. These systems are not foolproof, so as the OWNER,
it's up to you to keep an eye out rather than solely relying on these
automated systems. But, adding CPEs decreases the chances of us missing
vulnerabilities, so they should always be added if possible.

The CPE is a common format shared across the industry; you can look up the CPE
for your package [here](https://nvd.nist.gov/products/cpe/search).
* Use CPE format 2.3 (preferred) or CPE format 2.2 (supported).
* If the CPE uses the 2.3 URI binding or 2.2 format (i.e. starts with "cpe:/"),
and no version is explicitly specified within the `CPEPrefix`, the `Version`
in the `README.chromium` file will be appended to the `CPEPrefix`, if available.
  * Note: if the `Version` field is set to a git hash value, version matching
  for vulnerabilities will fail.

When searching for a CPE, you may find that there is not yet a CPE for the
specific upstream version you're using. This is normal, as CPEs are typically
allocated only when a vulnerability is found. You should follow the version
number convention such that, when that does occur in future, we'll be notified.
If no CPE is available, please specify "unknown".

If you're using a patched or modified version which is halfway between two
public versions, please "round downwards" to the lower of the public versions
(it's better for us to be notified of false-positive vulnerabilities than
false-negatives).


## Shipped
Your README.chromium should also specify whether your third party dependency
will be shipped as part of a final binary. The "Shipped" field replaces the now
deprecated special value of "NOT_SHIPPED" which was previously allowed in the
"License File" field. This use is no longer supported and all third party
dependencies must include a valid license regardless of whether it is shipped
or not.

## License
#### Add a LICENSE file and run related checks

You need a LICENSE file. Example:
[//third_party/libpng/LICENSE](../third_party/libpng/LICENSE). Dependencies
should not be added without a license file and license type, even if they are
not shipped in a final product. Existing dependencies without a license file or
license type are currently being cleaned up as part of the metadata uplift
effort. If you are an OWNER of a dependency missing license fields, there will
soon be a bug filed to fix it.

Run `//tools/licenses/licenses.py scan`; this will complain about incomplete or missing
data for third_party checkins. We use `licenses.py credits` to generate the
about:credits page in Google Chrome builds.

If the library will never be shipped as a part of Chrome (e.g. build-time tools,
testing tools), make sure to set the "Shipped" field to "no" so that the license
is not included in about:credits page ([more on this below](#credits)).

README.chromium files contain a 'License Android Compatible' field indicating whether
the package's license is compatible with Android. This is optional if the package is
not shipped, or uses a standard license.

When a dependency allows a choice of license, OWNERS should choose the least
restrictive license that meets Chromium's needs and document only the chosen
license(s) in the README.chromium file.

Multiple licenses apply when there are dependencies bundled together, or
different parts have different restrictions, these are inherently 'and'. This is
very different to a project allowing multiple license options.

The `License:` field in README.chromium must use a _comma-separated list_ of licenses
that are actively in use. Complex license expressions are not allowed or
supported.

Use SPDX license identifiers (https://spdx.org/licenses/) when possible e.g.
['Apache-2.0'](https://spdx.org/licenses/Apache-2.0.html). You can find the full
allowlist in
[depot_tools/+/main:metadata/fields/custom/license_allowlist.py](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:metadata/fields/custom/license_allowlist.py).

If the dependency uses a license that is not in the
[license classification](#license-classifications)(see below) to determine what actions are
required.

## License Classifications

Licenses used in our codebase fall into several categories. Notice-level and
less restrictive licenses are allowed in all projects.

| Classification | Description | Examples | Can I use it? |
| :--- | :--- | :--- | :--- |
| **Public Domain / Permissive** | Allow you to do almost anything, may require attribution. | [CC0-1.0](https://spdx.org/licenses/CC0-1.0.html), [Unlicense](https://spdx.org/licenses/Unlicense.html) | Yes |
| **Notice** | Require copyright notice and attribution. | [Apache-2.0](https://spdx.org/licenses/Apache-2.0.html), [BSD-3-Clause](https://spdx.org/licenses/BSD-3-Clause) | Yes |
| **Reciprocal** | Require sharing modifications under the same terms. | [MPL-1.1](https://spdx.org/licenses/MPL-1.1.html), [APSL-2.0](https://spdx.org/licenses/APSL-2.0.html) | Yes (allowed for open-source projects like Chromium) |
| **Restricted** | Stricter requirements, case-by-case approval required. | [LGPL-2.1](https://spdx.org/licenses/LGPL-2.1.html), [GPL-2.0](https://spdx.org/licenses/GPL-2.0.html) | Requires [approval](#file-a-restricted-license-request) |

Make sure you understand the license terms before checking in a dependency, and
when making any local modifications or forks.

### Restricted Licenses

Restricted licenses (like GPL) have stricter requirements and require
case-by-case approval unless they have been added to the
[allowlist](https://source.chromium.org/chromium/chromium/tools/depot_tools/+/main:metadata/fields/custom/license_allowlist.py)
(e.g. LGPL).

Common cases that are allowed (but still require a request):
*   **[GPL-2.0](https://spdx.org/licenses/GPL-2.0.html)** licenses are allowed for all non-shipped dependencies.

#### File a Restricted License Request

*   **Googlers**: File a bug using [this bug template](http://b/issues/new?component=1450496&template=2332739).
    If approved, you will be given a
    [`restrictive_license_approval.textproto`](https://chromium.googlesource.com/chromium/tools/depot_tools/+/main/metadata/restrictive_license_approval.proto)
    file to put next to your `README.chromium`.
*   **External Contributors**: File a bug using [this bug template](https://g-issues.chromium.org/issues/new?component=1456923&template=2336896).
    A member of the `//third_party/OWNERS` team will review it and handle this process on your
    behalf.

## Multiple packages
Adding multiple packages in a single third party directory is not recommended,
because it does not follow the best practices for
[third party dependency structure](#standard-dep-structure)
and complicates vulnerability scanning.

Each dependency should have its own third party directory with a few very
limited exceptions:
* A package manager is used to manage dependencies in the directory via a lockfile.
* Your third party dependency has its own vendored transitive dependencies

If your dependency is covered by one of the above exceptions and the information
for multiple packages must be placed in a single README.chromium, use the below
line to separate the data for each package:
```
-------------------- DEPENDENCY DIVIDER --------------------
```

# Vulnerability Cover {#vulnerability-cover}

All dependencies _must_ provide sufficient metadata to enable vulnerability scanning:
* `URL` and (`Version` or `Revision`) match upstream identifiers (git, package manager, etc);
or
* `CPEPrefix` and `Version`.

There are limited exceptions for dependencies matching:
* `Update Mechanism: Static[.HardFork]`
* `URL: Google Internal`
* `URL: This is the canonical public repository`

# Get a review

All third party additions and substantive changes like re-licensing need the
following sign-offs. Some of these are accessible to Googlers only.
Non-Googlers can email one of the people in
[//third_party/OWNERS](../third_party/OWNERS) for help.

* Make sure you have the approval from Chrome TSC and Chrome Security as
mentioned [above](#before-you-start).
* Add chromium-third-party@google.com as a reviewer on your change. This
  will trigger automatic assignment to a reviewer who will check license use,
  metadata compliance and verify long term dependency maintenance plans.
  Not all these reviewers may be able to +1 a change so look for verbal approval
  in the comments. (This list does not receive or deliver email, so only use it
  as a reviewer, not for other communication. Internally, see
  [crbug.com/441381904](https://crbug.com/441381904) for additional context on
  how this is configured.). If you have questions about the third-party
  process, ask one of the [//third_party/OWNERS](../third_party/OWNERS) instead.
* Lastly, if all other steps are complete, get a positive code review from a
  member of [//third_party/OWNERS](../third_party/OWNERS) to land the change.

Subsequent changes don't normally require third-party-owners or security
approval; you can modify the code as much as you want. When you update code, be
mindful of security-related mailing lists for the project and relevant CVE to
update your package.

# How we ensure that the right credits are displayed {#credits}

As we said at the beginning, it is important that Chrome displays the
right credit and attributions for all of the third_party code we use.

To view this in chrome, you can open chrome://credits.

That page displays a resource embedded in the browser as part of the
[//components/resources/components_resources.grd](../components/resources/components_resources.grd)
GRIT file; the actual HTML text is generated in the
[//components/resources:about_credits](../components/resources/BUILD.gn)
build target using a template from the output of the
[//tools/licenses/licenses.py](../tools/licenses/licenses.py) script. Assuming
you‘ve followed the rules above to ensure that you have the proper path to the
LICENSE file and set the Shipped value, if it passes the checks, it’ll be
included automatically.
