# Adding third_party Libraries

[TOC]

Using third party code can save time and is consistent with our values - no need
to reinvent the wheel! We put all code that isn't written by Chromium developers
into `//third_party` (even if you end up modifying just a few functions). We do
this to make it easy to track license compliance, security patches, and supply
the right credit and attributions. It also makes it a lot easier for other
projects that embed our code to track what is Chromium licensed and what is
covered by other licenses.

## Put the code in //third_party

By default, all code should be checked into [//third_party](../third_party/),
for the reasons given above. Other locations are only appropriate in a few
situations and need explicit approval; don't assume that because there's some
other directory with third_party in the name it's okay to put new things
there.

## Before you start

To make sure the inclusion of a new third_party project makes sense for the
Chromium project, you should first obtain Chrome Eng Review approval.
Googlers should see go/chrome-eng-review and review existing topics in
g/chrome-eng-review. Please include information about the additional checkout
size, build times, and binary sizes. Please also make sure that the motivation
for your project is clear, e.g., a design doc has been circulated.

## Get the code

There are two common ways to depend on third-party code: you can reference a
Git repo directly (via entries in the DEPS file), or you can check in a
snapshot. The former is preferable if you are actively developing in it or need
access to the history; the latter is better if you don't need the full history
of the repo or don't need to pick up every single change. And, of course, if
the code you need isn't in a Git repo, you have to do the latter.

### Node packages

To include a Node package, add the dependency to the
[Node package.json](../third_party/node/package.json). Make sure to update
the corresponding [`npm_exclude.txt`](../third_party/node/npm_exclude.txt)
and [`npm_include.txt`](../third_party/node/npm_include.txt) to make the code
available during checkout.

### Pulling the code via DEPS

If the code is in a Git repo that you want to mirror, please file an [infra git
ticket](https://bugs.chromium.org/p/chromium/issues/entry?template=Infra-Git)
to get the repo mirrored onto chromium.googlesource.com; we don't allow direct
dependencies on non-Google-hosted repositories, so that we can still build
if an external repository goes down..

Once the mirror is set up, add an entry to [//DEPS](../DEPS) so that gclient
will pull it in. If the code is only needed on some platforms, add a condition
to the deps entry so that developers on other platforms don't pull in things
they don't need.

As for specifying the path where the library is fetched, a path like
`//third_party/<project_name>/src` is highly recommended so that you can put
the file like OWNERS or README.chromium at `//third_party/<project_name>`. If
you have a wrong path in DEPS and want to change the path of the existing
library in DEPS, please ask the infrastructure team before committing the
change.

Lastly, add the new directory to Chromium's `//.gitignore`, so that it won't
show up as untracked files when you run `git status` on the main repository.

### Checking in the code directly

If you are checking in a snapshot, please describe the source in the
README.chromium file, described below.  For security reasons, please retrieve
the code as securely as you can, using HTTPS and GPG signatures if available.
If retrieving a tarball, please do not check the tarball itself into the tree,
but do list the source and the SHA-512 hash (for verification) in the
README.chromium and Change List. The SHA-512 hash can be computed via
`sha512sum` or `openssl dgst -sha512`.  If retrieving from a git
repository, please list the revision that the code was pulled from.

If you are checking the files in directly, you do not need an entry in DEPS
and do not need to modify `//.gitignore`.

### Checking in large files

_Accessible to Googlers only. Non-Googlers can email one of the people in
third_party/OWNERS for help.

See [Moving large files to Google Storage](https://goto.google.com/checking-in-large-files)

## Document the code's context

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
[//third_party/README.chromium.template](../third_party/README.chromium.template)
for a list of fields to include. A presubmit check will check this has the right
format.

README.chromium files contain a field indicating whether the package is
security-critical or not. A package is security-critical if it is compiled
into the product and does any of the following:

* Accepts untrustworthy inputs from the internet
* Parses or interprets complex input formats
* Sends data to internet servers
* Collects new data
* Influences or sets security-related policy (including the user experience)

### Add a LICENSE file and run related checks

You need a LICENSE file. Example:
[//third_party/libjpeg/LICENSE](../third_party/libjpeg/LICENSE).

Run `//tools/licenses.py scan`; this will complain about incomplete or missing
data for third_party checkins. We use `licenses.py credits` to generate the
about:credits page in Google Chrome builds.

If the library will never be shipped as a part of Chrome (e.g. build-time tools,
testing tools), make sure to set "License File" as "NOT_SHIPPED" so that the
license is not included in about:credits page ([more on this below](#credits)).

## Get a review

All third party additions and substantive changes like re-licensing need the
following sign-offs. Some of these are accessible to Googlers only.
Non-Googlers can email one of the people in
[//third_party/OWNERS](../third_party/OWNERS) for help.

* Make sure you have the approval from Chrome Eng Review as mentioned
  [above](#before-you-start).
* Get security@chromium.org approval. Email the list with relevant details and
  a link to the CL. Third party code is a hot spot for security vulnerabilities.
  When adding a new package that could potentially carry security risk, make
  sure to highlight risk to security@chromium.org. You may be asked to add
  a README.security or, in dangerous cases, README.SECURITY.URGENTLY file.
* Add chromium-third-party@google.com as a reviewer on your change. This
  will trigger an automatic round-robin assignment to a reviewer who will check
  licensing matters. These reviewers may not be able to +1 a change so look for
  verbal approval in the comments. (This list does not receive or deliver
  email, so only use it as a reviewer, not for other communication. Internally,
  see cl/221704656 for details about how this is configured.)
* Lastly, if all other steps are complete, get a positive code review from a
  member of [//third_party/OWNERS](../third_party/OWNERS) to land the change.

Please send separate emails to the eng review and security lists.

Subsequent changes don't normally require third-party-owners or security
approval; you can modify the code as much as you want. When you update code, be
mindful of security-related mailing lists for the project and relevant CVE to
update your package.

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
