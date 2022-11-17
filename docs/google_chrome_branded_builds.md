# Google Chrome branded builds

> Note: to do any of this, an `src-internal` checkout is needed. For more info,
> see http://go/chrome-linux-build#optional-add-src-internal-to-the-checkout

By default, chromium will build with the open source chromium assets and
branding (`is_chrome_branded = false` in
[GN args](https://www.chromium.org/developers/gn-build-configuration), see also
[Chrome vs. Chromium](chromium_browser_vs_google_chrome.md)).

The main reason for this is that the Google Chrome logo and related assets is
a trademark which we don't want to release under Chromium's open source
license.

Therefore, if you want to add a trademarked resource, check it into an internal
repository (see section below), and pick a resource based on the branding
(`is_chrome_branded` in GN, `#if BUILDFLAG(GOOGLE_CHROME_BRANDING)` in cpp, `<if
expr="_google_chrome">` in grit preprocessing). If possible, check an open
source version into Chromium, so the feature continues to work as expected in
the open source build.

E.g.
[`//components/resources/default_100_percent/chromium`](../components/resources/default_100_percent/chromium)
vs
[`//components/resources/default_100_percent/google_chrome`](https://chrome-internal.googlesource.com/chrome/components/default_100_percent/google_chrome/).

For strings, it’s ok to check them into the open source repository, but make
sure that you refer to the correct product, i.e., check in a version of the
string that says “Google Chrome” and a version that says “Chromium”.

E.g. [`//chrome/app/chromium_strings.grd`](../chrome/app/chromium_strings.grd)
vs
[`//chrome/app/google_chrome_strings.grd`](../chrome/app/google_chrome_strings.grd).

## Internal asset repositories

Assets live in various locations based on where they are used: native vs WebUI,
chrome vs component layer, etc. You can read more about this here:
[chromium.org | High DPI Resources](https://www.chromium.org/developers/design-documents/high-dpi-resources/)

To check in product-specific assets, in general:
- Add `//chrome` ones under
  `//chrome/app/theme[/optional_scale_factor_indicator]/[product_name]`. E.g.
  [`//chrome/app/theme/default_100_percent/chromium/product_logo_32.png`](../chrome/app/theme/default_100_percent/chromium/product_logo_32.png)

  > Note: WebUI-specific resources should go under
  > `//chrome/browser/resources/[/optional_scale_factor_indicator]/[product_name]`.
  > We don't have an internal repo associated yet, so please
  > [create one](http://go/git-admin-cheatsheet#creating-a-repo) if the use case
  > comes up.

- Add `//components` ones under
  `//components/resources[/optional_scale_factor_indicator]/[product_name]`.
  E.g.
  [`//components/resources/default_200_percent/chromium/product_logo.png`](../components/resources/default_200_percent/chromium/product_logo.png)

Each `google_chrome` version of a product directory points to a separate
internal git repo. Some examples:
- https://chrome-internal.googlesource.com/chrome/theme/google_chrome/
- https://chrome-internal.googlesource.com/chrome/theme/default_100_percent/google_chrome/
- https://chrome-internal.googlesource.com/chrome/components/default_200_percent/google_chrome/

To add resources there, `cd` to this repo, add your new assets and `git cl
upload` to start an internal code review. Once it lands, you will need to create
a roll CL for the `//../src-internal/DEPS`, using
[roll-dep](https://chromium.googlesource.com/chromium/tools/depot_tools/+/main/README.md#:~:text=cl.md.-,roll%2Ddep,-%3A%20A%20gclient%20dependency).

Once that CL lands, an auto-roller bot will update the main repo's src-internal
hash reference in `//DEPS` ([example autoroll CL](https://crrev.com/c/4024955))
and your new internal resources will be available on the bots. The chromium-side
CL making use of it can then be uploaded.
