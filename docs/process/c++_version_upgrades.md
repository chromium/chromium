# C++ Version Upgrades

This describes both "why" and "how" around updating Chrome's C++ version (e.g.
"C++17") to something newer.

## Goals

* Minimize toolchain support concerns in Chromium.

   *Example:* A new feature is known to have edge-case bugs, which developers
     must be told to avoid.
* Minimize toolchain support concerns in subprojects.

   *Example:* Code in //base would benefit from some new feature, but is also
     built (in Cronet) by a toolchain that doesn't support that feature.
* Avoid library incompatibilities.

   *Example:* A new version of protobuf requires a new C++ version, which
     Chromium doesn't yet use.
* Benefit from language/library improvements, subject to cost/benefit tests.

   *Example:* Features like C++11's `std::move` or C++20's Concepts affect
     large amounts of code and can be requirements for other features.
* Maintain Chromium's history of being a modern, well-maintained project where
  engineers feel they have access to the tools they need.

   *Example:* Abseil supports C++ versions for
     [approximately 10 years](https://opensource.google/documentation/policies/cplusplus-support#support_criteria_4),
     but being limited to ten-year-old standards is too limiting in Chromium.

## Non-Goals

* Use new C++ features as quickly as possible.
   * *Example:* C++23 "deducing this" could be used to simplify some
     [CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern)
     instances in Chromium, but on its own likely doesn't provide sufficient
     maintenance benefit to prioritize a rapid version update.
* Precisely match Google's internal C++ version and timelines.
   * *Example:* Google went directly from C++11 to C++17, while Chromium spent
     some time on C++14.

## Current Strategy

Chrome [ATLs](mailto:chrome-atls-discuss@google.com) are responsible for
ensuring "important but not urgent" changes like this are prioritized and
staffed appropriately. Their goal will be to upgrade Chrome's C++ version about
3 years after the date of the standard (e.g. for "C++35" we'd aim for ~2038).

This amount of lag time is approximately what happened for C++11-20 and is
usually a good balance of tradeoffs. Earlier is possible for compelling
features, but reducing the delay under two years is usually intractable for
toolchain reasons. A bit later might be OK, but since a version update requires
several months, it's best not to wait until there's an urgent need.

## Playbook

Version updates generally require one engineer, and take between a few weeks and
a few months of full-time work (depending on what's changed in the standard),
along with several months of coordination, rollout, and testing that doesn't
require significant engineering time.

1. Reach out to ATLs to validate proposed timeframes and check for any new
   external dependencies not listed here.
1. Reach out to other affected projects to alert them and verify they're
   prepared to upgrade. The two main projects to coordinate with are:
   1. The [libchrome](https://goto.google.com/libchrome) team in CrOS, since
      once Chromium begins using new-version features, it will force libchrome
      to be built with the new version or else break rolls. In the C++20
      timeframe, [lokerik@](mailto:lokerik@google.com) was a good initial
      contact.
   1. The [Cronet](https://goto.google.com/cronet) team on the Android side,
      since they build some Chrome code using a different toolchain and library
      version. There is a
      [Chromium waterfall bot](https://ci.chromium.org/ui/p/chromium/builders/ci/android-cronet-mainline-clang-arm64-rel)
      validating this configuration, so breakage should be visible. In the C++20
      timeframe, [sporeba@](mailto:sporeba@google.com) was a good initial
      contact.
1. Locally edit
   [//build/config/compiler/BUILD.gn](https://chromium.googlesource.com/chromium/src/+/main/build/config/compiler/BUILD.gn);
   update flags that reference the current C++ version to refer to the next
   version.
1. Build locally, fixing compile failures.
   * Fixes in Chromium must compile in both the current and new C++ version.
   * Likely, there will also be failures in various subprojects; these must be
     fixed by landing changes upstream and rolling in new versions. There is no
     single way to do this since subprojects are hosted in a variety of places
     and pulled in via multiple different methods, but generally it requires
     testing the fix by locally modifying the in-Chromium copy, then doing a
     standalone checkout of the subproject elsewhere using the upstream
     contribution instructions in order to actually send the patch.
   * For Google-owned subprojects, even when there is a GitHub repo that claims
     to accept external contributions, it is usually a mirror of something
     internal, and it is faster and more likely successful to contribute changes
     to that internal source of truth and then have them mirrored to GitHub.
1. Once a full local build succeeds, repeat the process on other platforms. This
   is easiest from a Linux host since Linux can cross-build most other
   platforms.
   * Note that ChromeOS builds using a different compiler version than the rest
     of Chrome, rolled on a much less aggressive schedule, and thus may have
     bugs or support gaps that don't exist for other platforms. If there are
     problems, reach out to the
     [CrOS toolchain team](https://goto.google.com/crostc); in the C++20
     timeframe, [gbiv@](mailto:gbiv@google.com) was a good initial contact.
1. Once things compile, do try runs and investigate any failures. While it may
   seem like simply changing the C++ version should not cause runtime failures,
   it's possible, and said failures are often subtle. For example, during the
   C++20 update, fixes to make more types aggregates also changed the semantics
   of one type such that it became eligible for moves in more situations,
   exposing a latent use-after-move bug that had previously "worked" because the
   attempted `std::move` had still resulted in a copy.
1. Add PRESUBMITs to prevent usage of all new language/library features, so none
   is added during the rollout period below. (Partial lists of new features are
   available from the individual language version pages linked atop
   https://en.cppreference.com/w/cpp.)
1. Reach out to
   [Chrome release managers](https://goto.google.com/chrome-release-management)
   to alert them of plans to begin rolling out a new version, in case it leads
   to bugs.
1. Reach out to the
   [Chrome infrastructure team](https://goto.google.com/chrome-brapp-engprod)
   about potential compile time and binary size impact; in the C++20 timeframe,
   [estaab@](mailto:estaab@chromium.org) was a good initial contact. Roll out
   the new version one platform at a time under their supervision, to ensure it
   behaves as expected. (This will require temporarily adding more conditional
   logic to the .gn files.) You can crudely estimate build time impact in
   advance by timing local builds.
1. Determine a launch date and validate with stakeholders. Send a message to
   [chromium-dev@](http://groups.google.com/a/chromium.org/g/chromium-dev)
   announcing the date and giving brief instructions on how to share concerns.
1. Make a list of all new language and library features and bucket as "allowed"
   (clearly safe for Chromium use), "TBD" (needs more investigation/discussion),
   and "banned" (clearly inappropriate for Chromium). Send this proposal to
   [cxx@](http://groups.google.com/a/chromium.org/g/cxx) and discuss until there
   is consensus on how to proceed. Write a CL that updates
   [//styleguide/c++/c++-features.md](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++-features.md)
   and the PRESUBMIT checks to match the proposal (and mark the new version
   "initially allowed"), get it reviewed, but do not submit it.
1. Write a smoketest CL that uses some new feature in somewhere low-level enough
   to affect all of Chromium and any subprojects (e.g. something core in
   //base). Land it and ensure it can successfully roll into all dependencies.
1. On launch day, land the styleguide/presubmit update CL, then announce
   availability of the new version to chromium-dev@.
1. In the months following the release, work with cxx@ to follow up on TBD
   features, proposing allowing/banning them as support improves and/or we do
   further necessary research.
