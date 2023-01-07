# Supported Projects

The `//net` stack is used on a variety of platforms and in a variety of open
source projects. These different platforms and projects have differing
degrees of support from `//net` OWNERS as well as differing requirements for
designs and technical requirements.

Note that this is a rough high-level overview of the major projects; as more
of `//net` is broken into consumable services/components as part of the Mojo
servicificaiton efforts, it's likely that there will be a larger number of
'variants' of these projects with different constraints and features.

## Google Chrome Browser

The Google Chrome browser, which lives in `//chrome`, is the most important
`//net` consumer and shapes many of the core design decisions. In general,
features that are not intended with or not compatible with the needs of
the Google Chrome browser will have a very high bar for acceptance in `//net`.

The feature matrix

  * **Supported Platforms**: Windows, macOS, Linux, Chromium OS, iOS, Android
  * **Release Frequency**: ~6 weeks between releases
  * **Automatic Updates**: Yes
  * **Command-line Flags**:
    * __Yes__: Windows, macOS, Linux, Chromium OS (Dev image), Android (rooted)
    * __No__: Chromium OS (Release image), iOS, Android (Release)
  * **Field Trials (Finch)**: Yes
  * **Enterprise Policy**: Yes
  * **User Metrics (UMA)**: Yes
  * **Component Updater**: Yes

## Chromium Browser

The Chromium browser refers to the practice of certain Linux distributions to
bundle the open-source components of Chrome Browser in `//chrome`, branded
as Chromium. This version is not distributed by Google, but by individual
Linux distributions (primarily). Other distributions based on building Chromium
for other platforms exist, although do not see as wide of usage.

  * **Supported Platforms**: Windows, macOS, Linux, Chromium OS, iOS, Android
  * **Release Frequency**: Varies by distributor; some Linux distributions
    treat versions of Chromium as "Long Term Stable" and support a single
    version for a longer time than the Chromium projects do, others track
    the Google Chrome release frequency.
  * **Automatic Updates**: Varies by distributor
  * **Command-line Flags**:
    * __Yes__: Windows, macOS, Linux, Chromium OS (dev image), Android (rooted)
    * __No__: Chromium OS (Release image), iOS, Android (Release)
  * **Field Trials (Finch)**: No
  * **Enterprise Policy**: Yes
  * **User Metrics (UMA)**: Varies by distributor
  * **Component Updater**: Varies by distributor

## Android WebView

Distinct from the Chromium browser, the Android WebView is itself based on
the Chromium browser. On official Android devices running Android N or later,
WebView is automatically updated when Google Chrome is updated on the
device. For earlier devices, Android WebView is updated by the System WebView
component.

Android WebView may also be used on non-official Android devices, such as
those based on the Android Open Source Project but do not go through the
Android [Compatability Test Suite](https://source.android.com/compatibility/cts/).
Such releases have limited to no interaction with the Chromium projects, and
so their capabilities cannot be conclusively documented.

For official Android devices, WebView has the following capabilities.

  * **Supported Platforms**: Android
  * **Release Frequency**: ~6 weeks between releases
  * **Automatic Updates**: Varies. Updates are made available on the Android
    App Store, but users must explicitly choose to update. As such, the
    rate of update varies much more than for the Chromium browser.
  * **Command-line Flags**: No for production devices, [yes for userdebug
    devices](https://chromium.googlesource.com/chromium/src/+/HEAD/android_webview/docs/commandline-flags.md)
  * **Field Trials (Finch)**: Yes, [with
    caveats](https://g3doc.corp.google.com/analysis/uma/g3doc/finch/platforms.md?cl=head)
  * **Enterprise Policy**: Yes, with caveats (TODO(rsleevi): document caveats)
  * **User Metrics (UMA)**: Yes, [with caveats](http://go/clank-webview/uma)
  * **Component Updater**: No

## `//content` Embedders

In addition to Chromium, there are a number of other of embedders of
`//content`, such as projects like [Chromium Embedded Framework](https://bitbucket.org/chromiumembedded/cef),
[Electron](http://electron.atom.io/) or Fuchsia's [WebEngine](https://chromium.googlesource.com/chromium/src/+/HEAD/fuchsia_web/webengine/).
While `//net` does not directly support these consumers, it does support the
`//content` embedding API that these projects use. Note that this excludes the
[content_shell](../../content/shell) test framework.

  * **Supported Platforms**: Windows, macOS, Linux, Chromium OS, iOS, Android,
    Fuchsia
  * **Release Frequency**: Varies by consumer; Officially ~6 weeks
  * **Command-line Flags**: Varies by consumer
  * **Field Trials (Finch)**: No
  * **Enterprise Policy**: No
  * **User Metrics (UMA)**: No
  * **Component Updater**: No

## Cronet

[Cronet](../../components/cronet/README.md) is a version of the `//net`
network stack for use in mobile applications on iOS and Android. While
primarily targetting first-party Google applications, Cronet's status as an
open-source project, similar to the Chromium browser, means that it may
find itself embedded in a variety of other projects.

Unlike some of the other `//net` consumers, Cronet does not necessarily
implement "The Web Platform" (as the holistic set of user agent-focused
features), and instead is more akin to an HTTP(s) client networking library.

  * **Supported Platforms**: iOS, Android
  * **Release Frequency**: Varies. While "releases" are made following the
    same frequency as Google Chrome, because it is treated similar to
    a "third-party" library, different products and projects will ship
    different versions of Cronet for differing periods of time.
  * **Command-line Flags**: No
  * **Field Trials (Finch)**: No
  * **Enterprise Policy**: No
  * **User Metrics (UMA)**:
    * __Yes__: First-party (Google) projects, although they will be
      reported in project-specific buckets (e.g. no overarching set of
      metrics for all Cronet consumers).
    * __No__: In general
  * **Component Updater**: No
