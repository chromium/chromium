Supported Chromium Products and Platforms
=========================================

Chromium is used to produce many products, which run on many platforms.
One of the biggest challenges in making changes to Chromium is appropriately
considering that wide variety of supported products and platforms and dealing
with platform-specific or product-specific test failures.

To keep this cost manageable the Chromium project is very conservative in adding
new supported platforms and products, with additions requiring approval by Chrome
ATLs (chrome-atls-discuss@google.com). This document is the canonical document
describing which products are officially supported on which platforms.

**Note**: This document applies to Chromium. Some of Chromium's subcomponents like
ANGLE or V8 currently support more platforms.

Definitions of Terms
--------------------

* **Platform**: for this document means a build configuration: A combination
of OS, ISA like 64-bit Intel or 32-bit Arm, compiler, debug / release,
sanitizers, and so on. In short, a combination of flags you might stick into
args.gn.
* **Product**: An executable that can be built on one or more platforms and
ships to users.
* **Embedder**: An embedder is a code module that glues together ("embeds")
  Chromium's lower-level layers (e.g., content layer + selected //components) to
  produce a product. Embedders can live either upstream (i.e., in the Chromium
  repository) or downstream.
* **Officially supported product**: A product is officially supported if
engineers working in the Chromium codebase are accountable for regressions
in that product.
* **Officially supported platform**: A platform is officially supported if there
are bots on [Chromium's waterfall](https://ci.chromium.org/p/chromium/g/main/console) and commit queue that build and run tests on that
platform. Commit queue coverage means that no patches that land will break these
platforms. Note that official platform support can and does vary by product,
as detailed below.
* **Community supported platform**: Some platforms that are not officially
supported for a given product can still be community supported. That means that
despite no bots making sure that the platform does not break, the community sends
patches to unbreak things, and we accept these patches unless they introduce
undue cost in the judgment of the local owners in question.
* **Unsupported platform**: Actively blocked from working upstream (e.g., may
  #error out in build).
* **Downstream product**: Chromium is used by several other projects as a "soft-fork"
(downstream branch) for which engineers making upstream changes are not
accountable for preventing regressions, but instead should ideally be helpful in
providing guidance to
project owners in a good open-source citizen best-effort fashion. Such
downstream products are of course free to support platforms that are
unsupported in upstream Chromium at the cost of maintaining the fork.
* **SKU (Stock keeping unit)**: A build configuration of a product that ships to
users. We try to limit the number of SKUs. For example, on each OS, we generally try to
have a single binary for each product per ISA (instruction set architecture, e.g. x86
or ARM), with optional fast paths for optional specialized instructions dispatched to
at runtime.
* **CQ (commit queue) bot**: A bot that runs some build/test configuration on Chromium
CLs pre-submit and blocks submit if it turns red.
* **CI (continuous integration) bot**: A bot that runs some build/test configuration
  on Chromium CLs post-submit and closes the tree if it turns red.
* **FYI bot**: A bot that runs some build/test configuration on Chromium's
  waterfall at some cadence but does not block commit or close the tree on
  turning red.
* **Internal bot**: A bot that runs some team's set of tests somewhere
  internal to that team. (Note for Googlers: see details about Google-specific
  internal bots on the CQ
  [here](https://g3doc.corp.google.com/company/teams/chrome/ops/engprod/browser_infra/cq/overview.md?cl=head#internal-builders-on-the-cq)).

Engineer Responsibility
-----------------------

### When making changes

* The engineer making a change is accountable for reasonably proactively ensuring
that change doesn't negatively impact officially supported platforms and products
using their code
* This is obviously always a cost/risk judgement call and the perfect balance
certainly does not mean elimination of all risk. But engineers should expect that
when they cause an issue of this type, they are the ones on the hook for
remediation (eg. immediate revert / merge), post-mortem and potentially post-
mortem action items and so should feel the incentive to use tools like experimentation
appropriately. In particular:
  * Detected breakage of CQ/CI bots has a "revert first, ask later" policy.
    Engineers should expect that any of their CLs causing such breakage may
    freely be reverted without prior discussion, and their first priority in
    resolving such breakage should be turning the bots green again ASAP.
  * Detected breakage of FYI bots should *not* result in reverts a priori. The
    team gardening that FYI bot may reach out to the engineer authoring the CL
    to understand more about the impact of the CL, but as a general rule, the
    challenge of keeping the FYI bot green is on the team gardening the FYI
    bot.
  * Detected breakage of internal bots should *not* result in reverts a priori.
    The team gardening that internal bot may reach out to the engineer
    authoring the CL to together determine the best path forward. That best
    path forward may involve reverting the upstream CL, but that should be
    done only (a) with the consensus of the engineer who authored the CL and
    (b) with the commitment of the downstream team to do whatever necessary to
    allow the engineer in question to advance their upstream work. Note: There
    may be exceptions for engineers on the downstream team in question, for
    whom there can be tighter policies as determined by the team itself (for
    a concrete example, see the "Tast notes section" in [this document](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/testing/testing_in_chromium.md)).

### When introducing new features

In general, engineers adding a new feature are accountable for:
* Deciding which of the supported platforms their new feature will support
* Ensuring their feature works well on all such platforms, and is correctly
disabled (and so causes no regressions) on all other official Chromium platforms
* Adhering to Chromium's [flag guarding
  guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/flag_guarding_guidelines.md)
* Informing platform owners appropriately in case they want to partner on
additional support earlier rather than later
  * Some feature areas have additional processes to support this and promote
consistency: eg. for new web platform APIs we ask engineers to explain their
platform support decision and API owners may object when a decision lacks
suitable justification (eg. lacking WebView support for no good reason)

Supported Compilers
-------------------

The only officially supported C++ compiler is Chromium's hermetic clang.
This is upstream clang built at a specific upstream revision that's
periodically updated. (See "Cronet" below for a small asterisk.)
See docs/clang.md for more details.

The only officially supported Rust compiler is Chromium's hermetic rustc.
This is upstream rustc built at a fixed revision, built against the same
LLVM that Chromium's clang is built against (to make cross-language LTO work
well). It enables rustc nightly features.

On Linux, GCC is community-supported (per the definition in the [definitions of
terms](#definitions-of-terms)).

MSVC is not supported, and `base/compiler_specific.h` `#error`s out on it.

Supported Chromium Browser Platforms by OS
------------------------------------------

The Chromium browser (and its official counterpart Google Chrome) are produced
via the //chrome embedder (except for on iOS, where the embedder is
//ios/chrome). This section details officially supported Chromium browser
platforms.

Note: Googlers can see additional internal information
[here](https://g3doc.corp.google.com/company/teams/chrome/platform_support.md?cl=head).

### Android

Chrome on Android is supported on x86, x64, ARM, and ARM64. The current minimum
OS version supported is listed [here](https://support.google.com/chrome/a/answer/7100626).

The `is_desktop_android` GN arg configures a build of Chrome for Android that
is customized for a desktop form factor.

### ChromeOS

Chrome follows the [ChromeOS auto-update
policy](https://support.google.com/chrome/a/answer/6220366).

### Linux

[This page](https://support.google.com/chrome/a/answer/7100626) details
minimum operating system and hardware requirements.

### macOS

[This page](https://support.google.com/chrome/a/answer/7100626) details
minimum operating system requirements.

### iOS

Chrome on iOS is supported on ARM64. The minimum supported operating system
version changes roughly once a year. As of February 2026, the minimum OS
version supported is iOS 17.0.

### Windows

[This page](https://support.google.com/chrome/a/answer/7100626) details
minimum operating system and hardware requirements.

Other Upstream Embedders
------------------------

Besides the embedder for the Chromium browser, there are several other
upstream embedders in Chromium, which have their own CQ and CI bots that
Chromium engineers are responsible for keeping green. This section details
these embedders.

### Android WebView

Android WebView is an Android system component for displaying web content.
The embedder code for Android WebView lives in //android_webview. Details are
[here](https://chromium.googlesource.com/chromium/src/+/main/android_webview/README.md).

### Chromecast

The embedder code for Chromecast lives in //chromecast. This code is used to
build the *Cast Web Runtime*, also known as the "Cast Browser", which is used to
display [web-based Google Cast apps](https://developers.google.com/cast/docs/web_receiver).

The supported platforms are defined in the gn files in
//chromecast/build/args/product. Each file corresponds to a single supported
platform for the Cast Web Runtime.

Details are [here](https://chromium.googlesource.com/chromium/src/chromecast/README.md).

### Cronet

Cronet is an Android library that packages Chromium's network stack. The
embedder code for Cronet lives in //components/cronet. Details are [here](https://chromium.googlesource.com/chromium/src/+/main/components/cronet/README.md).

### Fuchsia WebEngine

Fuchsia WebEngine is an application running on the Fuchsia operating system for
displaying web content and running cast applications (sometimes being referred
as a Cast Receiver). The embedder code for Fuchsia WebEngine lives in
//fuchsia_web. Details, including information on platform support, are
[here](https://chromium.googlesource.com/chromium/src/+/main/fuchsia_web/README.md).

### Headless

Headless Chromium allows running Chromium in a headless/server environment.
The embedder code for Headless lives in //headless. Details are
[here](https://chromium.googlesource.com/chromium/src/+/main/headless/README.md).

### iOS WebView

iOS WebView is an Objective-C framework that renders web content with
`[CWVWebView]`. The
embedder code for iOS WebView lives in //ios/web_view. Details are
[here](https://chromium.googlesource.com/chromium/src/+/main/ios/web_view/README.md).

