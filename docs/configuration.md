# Configuration: Prefs, Settings, Features, Switches & Flags

This document outlines all the runtime configuration surfaces of Chromium,
and discusses appropriate uses and standard patterns for each of them. Some of
these are intended for use by users, some by developers, and some by system
administrators.

[TOC]

## Prefs

Example: prefs::kAllowDinosaurEasterEgg aka "allow_dinosaur_easter_egg"

Prefs are implemented by registering them with a central pref service, usually
via [Profile::RegisterProfilePrefs][profile-register]. They store typed values,
which persist across restarts and may be synced between browser instances via
the Sync service. There are several pref stores, which are documented in detail
in the [prefs docs][prefs]. They can be directly configured via enterprise
policy.

Prefs:

* *Are not* directly surfaced to the user
* *Are not* localized into the user's language
* *Are* configurable via enterprise policy if a policy exists for the pref
  (there is no catch-all policy that allows setting arbitrary prefs)
* *Are not* reported via UMA when in use
* *Are not* included in chrome://version
* *Are* automatically persistent across restarts (usually)

## Features

Example: base::kDCheckIsFatalFeature

These are implemented via creating a [base::Feature][base-feature] anywhere.
These can be enabled via server-side experimentation or via the command-line
using "--enable-features".  Which features are in use is tracked by UMA metrics,
and is visible in chrome://version as the "Variations" field. Do note that in
release builds, only a series of hashes show up in chrome://version rather than
the string names of the variations, but these hashes can be turned back into
string names if needed. This is done by consulting [the testing
config][fieldtrial-config] for Chromium builds, or a Google-internal tool for
Chrome builds.

*Features are the best way to add runtime conditional behavior.*

Features:

* *Are not* directly surfaced to the user
* *Are not* localized into the user's language
* *Are not* configurable via enterprise policy
* *Are* reported via UMA/crash when in use
* *Are* included in chrome://version
* *Are not* automatically persistent across restarts

## Switches

Example: switches::kIncognito aka "--incognito"

These are implemented by testing anywhere in the codebase for the presence or
value of a switch in [base::CommandLine::ForCurrentProcess][base-commandline].
There is no centralized registry of switches and they can be used for
essentially any purpose.

Switches:

* *Are not* directly surfaced to the user
* *Are not* localized into the user's language
* *Are not* configurable via enterprise policy (except on Chrome OS, via FeatureFlagsProto)
* *Are not* reported via UMA when in use
* *Are* included in chrome://version
* *Are not* automatically persistent across restarts

In general, switches are inferior to use of base::Feature, which has the same
capabilities and low engineering overhead but ties into UMA reporting. New code
should use base::Feature instead of switches. An exception to this is when the
configuration value is a string, since features can't take an arbitrary string
value.

## Flags

Example: chrome://flags/#ignore-gpu-blocklist

These are implemented by adding an entry in [about_flags.cc][about-flags]
describing the flag, as well as metadata in [flag-metadata][flag-metadata].
Flags have a name and description, and show up in chrome://flags. Flags also
have an expiration milestone, after which they will be hidden from that UI and
disabled, then later removed. Flags are backed by either a feature or a set of
switches, which they enable at browser startup depending on the value of the
flag.

Flags should usually be temporary, to allow for pre-launch testing of a feature.
Permanent flags (those with expiration -1) should only be used when either:

* The flag is regularly used for support/debugging purposes, by asking users to
  flip it to eliminate a possible problem (such as ignore-gpu-blocklist)
* The flag is used for ongoing QA/test purposes in environments where command-line
  switches can't be used (e.g. on mobile)

"Users might need to turn the feature on/off" is not a sufficient justification
for a permanent flag. If at all possible, we should design features such that
users don't want or need to turn them off, but if we need to retain that choice,
we should promote it to a full setting (see below) with translations and
support.  "Developers/QA might need to turn the feature on/off", on the other
hand, is justification for a permanent flag.

Flags:

* *Are* directly surfaced to the user
* *Are not* localized into the user's language
* *Are not* configurable via enterprise policy
* *Are* reported via UMA when in use (via Launch.FlagsAtStartup)
* *Are not* included in chrome://version
* *Are* automatically persistent across restarts

## Settings

Example: "Show home button"

Settings are implemented in WebUI, and show up in chrome://settings or one of
its subpages. They generally are bound to a pref which stores the value of that
setting. These are comparatively expensive to add, since they require
localization and some amount of UX involvement to figure out how to fit them
into chrome://settings, plus documentation and support material. Many settings
are implemented via prefs, but not all prefs correspond to settings; some are
used for tracking internal browser state across restarts.

Settings:

* *Are* directly surfaced to the user
* *Are* localized into the user's language
* *Are not* configurable via enterprise policy (but their backing prefs may be)
* *Are not* reported via UMA when in use
* *Are not* included in chrome://version
* *Are* automatically persistent across restarts (via their backing prefs)

You should add a setting if end-users might want to change this behavior. A
decent litmus test for whether something should be a flag or a setting is: "will
someone who can't read or write code want to change this?"

## Summary Table
|                                              | Prefs       | Features       | Switches | Flags                               | Settings                          |
| :-                                           | :-          | :-             | :--:     | :--:                                | :-                                |
| Directly surfaced to the user                | ❌          | ❌            | ❌       | ✅                                  | ✅                                |
| Localized into the user's language           | ❌          | ❌            | ❌       | ❌                                  | ✅                                |
| Configurable via enterprise policy           | ✅ if a policy<br>maps to the pref | ❌ | ❌ except on ChromeOS | ❌         | ❌ but their backing prefs may be |
| Reported when in use                         | ❌          | via UMA/crash |  ❌      | via UMA<br> `Launch.FlagsAtStartup` | ❌                                |
| Included in chrome://version                 | ❌          | ✅            | ✅       | ❌                                  | ❌                                |
| Automatically persistent<br> across restarts | ✅ usually  | ❌            | ❌       | ✅                                  | ✅ via backing prefs              |

## Related Documents

* [Chromium Feature API & Finch (Googler-only)](http://go/finch-feature-api)
* [Adding a new feature flag in chrome://flags](how_to_add_your_feature_flag.md)
* [Runtime Enabled Features](../third_party/blink/renderer/platform/RuntimeEnabledFeatures.md)
* [Initialization of Blink runtime features in content layer](initialize_blink_features.md)
* [Integrating a feature with the origin trials framework](origin_trials_integration.md)

[base-commandline]: https://cs.chromium.org/chromium/src/base/command_line.h?type=cs&l=98
[base-feature]: https://cs.chromium.org/chromium/src/base/feature_list.h?sq=package:chromium&g=0&l=53
[about-flags]: https://cs.chromium.org/chromium/src/chrome/browser/about_flags.cc
[fieldtrial-config]: https://cs.chromium.org/chromium/src/testing/variations/fieldtrial_testing_config.json
[flag-metadata]: https://cs.chromium.org/chromium/src/chrome/browser/flag-metadata.json
[prefs]: https://www.chromium.org/developers/design-documents/preferences
[profile-register]: https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/profiles/profile.h;l=189;drc=b0378e4b67a5dbdb15acf0341ccd51acda81c8e0
