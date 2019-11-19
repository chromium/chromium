# Google Chrome branded builds

By default, chromium will build with the open source chromium assets and
branding (`is_chrome_branded = false` in [GN
 args](https://www.chromium.org/developers/gn-build-configuration), see
also [Chrome vs. Chromium](chromium_browser_vs_google_chrome.md)).

The main reason for this is that the Google Chrome logo and related assets is
a trademark which we don't want to release under Chromium's open source
license.

Therefore, if you want to add a trademarked resource, check it into the
internal repository, and pick a resource based on the branding
(`is_chrome_branded` in GN, `#if BUILDFLAG(GOOGLE_CHROME_BRANDING)` in cpp). If
possible, check an open source version into Chromium, so the feature
continues to work as expected in the open source build.

E.g. [`//components/resources/default_100_percent/chromium`](../components/resources/default_100_percent/chromium) vs [`//components/resources/default_100_percent/google_chrome`](https://chrome-internal.googlesource.com/chrome/components/default_100_percent/google_chrome/).

For strings, it’s ok to check them into the open source repository, but make sure that you refer to the correct product, i.e., check in a version of the string that says “Google Chrome” and a version that says “Chromium”.

E.g. [`//chrome/app/chromium_strings.grd`](../chrome/app/chromium_strings.grd) vs [`//chrome/app/google_chrome_strings.grd`](../chrome/app/google_chrome_strings.grd).
