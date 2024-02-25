# The Difference between Google Chrome and Chromium on Linux

Chromium on Linux has two general flavors: You can either get
[Google Chrome](http://www.google.com/chrome?platform=linux) or chromium-browser
(see [Linux Chromium Packages](linux/chromium_packages.md)).
This page tries to describe the differences between the two.

In short, Google Chrome is the Chromium open source project built, packaged, and
distributed by Google. This table lists what Google adds to the Google Chrome
builds **on Linux**.

## Google Chrome

*   Colorful logo
*   [Reports crashes](linux/crash_dumping.md) only if turned on.
    Please include symbolized backtraces in bug reports if you don't have crash
    reporting turned on.
*   User metrics only if turned on
*   Video and Audio codecs (may vary by distribution)
    *   **H.264**, AV1, VP8, and VP9 video codecs.
    *   **AAC**, MP3, Opus, Theora, Vorbis, FLAC, and WAV audio codecs.
*   Code is tested by Chrome developers
*   Sandbox is always on
*   Single deb/rpm package
*   Profile is kept in `~/.config/google-chrome`
*   Cache is kept in `~/.cache/google-chrome`
*   New releases are tested before being sent to users
*   Google API keys are added by Google

## Chromium

*   Blue logo
*   Does not ever [report crashes](linux/crash_dumping.md). Please include
    symbolized backtraces in bug reports.
*   User metrics are never reported.
*   Video and Audio codecs (may vary by distribution)
    *   AV1, VP8, and VP9 video codecs.
    *   MP3, Opus, Theora, Vorbis, FLAC, and WAV audio codecs.
*   Code may be modified by distributions
*   Sandbox depends on the distribution (navigate to about:sandbox to confirm)
*   Packaging depends on the distribution
*   Profile is kept in `~/.config/chromium`
*   Cache is kept in `~/.cache/chromium`
*   New release testing depends on the distribution
    *   Distributions are encouraged to track stable channel releases: see
        https://chromereleases.googleblog.com/,
        https://chromiumdash.appspot.com/releases?platform=Linux
        and https://gsdview.appspot.com/chromium-browser-official/
*   Google API keys depend on the distribution
    *   See https://www.chromium.org/developers/how-tos/api-keys
