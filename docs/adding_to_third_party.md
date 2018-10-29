# Adding third_party Libraries

[TOC]

Using third party code can save time and is consistent with our values - no need
to reinvent the wheel! We put all code that isn't written by Chromium developers
into src/third_party (even if you end up modifying just a few functions). We do
this to make it easy to track license compliance, security patches, and supply
the right credit and attributions. It also makes it a lot easier for other
projects that embed our code to track what is Chromium licensed and what is
covered by other licenses.

## Get the Code

When you find code you want to use, get it. This often means downloading: from
Sourceforge, from the hosting feature of Google Code, or from somewhere else.
Sometimes it can mean negotiating a license with another company and receiving
the code another way. Please describe the source in the README.chromium file,
described below.  For security reasons, please retrieve the code as securely as
you can, using HTTPS and GPG signatures if available.  If retrieving a tarball,
please do not check the tarball itself into the tree, but do list the source and
the SHA-512 hash (for verification) in the README.chromium and Change List. The
SHA-512 hash can be computed via the `shasum (sha512sum)` or `openssl`
utilities.  If retrieving from a git repository, please list the SHA-512 hash.

## Put the Code in (the right) third_party

By default, all code should be checked into
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/).
It is OK to have third_party subdirectories that are not top-level (e.g.
src/net/third_party), but don't add more third_party directories than necessary.

## Document the Code's Context

### Add OWNERS

Your OWNERS file must include 2 Chromium developer accounts. This will ensure
accountability for maintenance of the code over time. While there isn't always
an ideal or obvious set of people that should go in OWNERS, this is critical for
first-line triage of any issues that crop up in the code.

As an OWNER, you're expected to:

* Remove the dependency when/if it is no longer needed
* Update the dependency when a security or stability bug is fixed upstream
* Help ensure the Chrome feature that uses the dependency continues to use the
  dependency in the best way, as the feature and the dependency change over
  time.

### Add a README.chromium

You need a README.chromium file with information about the project from which
you're re-using code. See
[README.chromium.template](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/README.chromium.template)
for a list of fields to include. A presubmit check will check this has the right
format.

### Add a LICENSE file and run related checks

You need a LICENSE file. Example:
[third_party/libjpeg/LICENSE](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/libjpeg/LICENSE?revision=42288&view=markup).

Run the following scripts:

* `src/tools/licenses.py scan` - This will complain about incomplete or missing
  data for third_party checkins. We use 'licenses.py credits' to generate the
  about:credits page in Google Chrome builds.

If the library will never be shipped as a part of Chrome (e.g. build-time tools,
testing tools), make sure to set "License File" as "NOT_SHIPPED" so that the
license is not included in about:credits page ([more on this below](#credits)).

### Modify DEPS

If the code is applicable and will be compiled on all supported Chromium
platforms (Windows, Mac, Linux, Chrome OS, iOS, Android), check it in to
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/).

If the code is only applicable to certain platforms, check it in to
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/)
and add an entry to the
[DEPS](http://src.chromium.org/viewvc/chrome/trunk/src/DEPS) file that causes
the code to be checked out from src/third_party into src/third_party by gclient.

_Explanation: Checking it into src/third_party causes all developers to need to
check out your code. This wastes disk space cause syncing to take longer for
developers that don't need your code. When all platforms really do need the
code, checking it in to src/third_party allows some slight improvements over
DEPS._

As for specifying the path where the library is fetched, a path like
`src/third_party/<project_name>/src` is highly recommended so that you can put
the file like OWNERS or README.chromium at `third_party/<project_name>`. If you
have a wrong path in DEPS and want to change the path of the existing library in
DEPS, please ask the infrastructure team before committing the change.

### Checking in large files

_Accessible to Googlers only. Non-Googlers can email one of the people in
third_party/OWNERS for help._

See [Moving large files to Google Storage](https://goto.google.com/checking-in-large-files)

## Setting up ignore

If your code is synced via DEPS, you should add the new directory to Chromium's
`.gitignore`. This is necessary because Chromium's main git repository already
contains
[src/third_party](http://src.chromium.org/viewvc/chrome/trunk/src/third_party/)
and the project synced via DEPS is nested inside of it and its files regarded as
untracked. That is, anyone running `git status` from `src/` would see a clutter.
Your project's files are tracked by your repository, not Chromium's, so make
sure the directory is listed in Chromium's `.gitignore`.

## Get a Review

All third party additions and substantive changes like re-licensing need the
following sign-offs. Some of these are accessible to Googlers only. Non-Googlers
can email one of the people in third_party/OWNERS for help.

* Get Chrome Eng Review approval. Googlers should see
  go/chrome-eng-review. Please include information about the additional
  checkout size, build times, and binary sizes. Please also make sure that the
  motivation for your project is clear, e.g., a design doc has been circulated.
* Get security@chromium.org approval. Email the list with relevant details and
  a link to the CL. Third party code is a hot spot for security vulnerabilities.
  When adding a new package that could potentially carry security risk, make
  sure to highlight risk to security@chromium.org. You may be asked to add
  a README.security or, in dangerous cases, README.SECURITY.URGENTLY file.
* Add chromium-third-party@google.com as a reviewer on your change. This
  will trigger an automatic round-robin assignment of the review to an
  appropriate reviewer. This list does not receive or deliver email, so only
  use it as a reviewer, not for other communication.

Please send separate emails to the eng review and security lists.

Subsequent changes don't require third-party-owners approval; you can modify the
code as much as you want. When you update code, be mindful of security-related
mailing lists for the project and relevant CVE to update your package.

## Ask the infrastructure team to add a git mirror for the dependency

Before committing the DEPS, you need to ask the infra team to create a git
mirror for your dependency. [Create a
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
for infra and ask the infra team.

If you are using a git repo from googlesource.com then you must ensure that the
repository is configured to give the build bots unlimited quota, or else the
builders will fail to checkout with an "Over Quota" error. [Create a
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
for infra and ask the infra team what needs to be done. Note that you'll need
unlimited quota for at least two role accounts. See the quota status of
`boringssl` as an example.

## How we ensure that the right credits are displayed {#credits}

As we said at the beginning, it is important that Chrome displays the
right credit and attributions for all of the third_party code we use.

To view this in chrome, you can open chrome://credits.

That page displays a resource embedded in the browser as part of the
[//components/resources/components_resources.grd](../components/resources/components_resource.grd)
GRIT file; the actual HTML text is generated in the
[//components/resources:about_credits](../components/resources/BUILD.gn)
build target using a template from the output of the
[//tools/licenses.py](../tools/licenses.py) script. Assuming you've followed
the rules above to ensure that you have the proper LICENSE file and it passes
the checks, it'll be included automatically.
