# Reader Mode on Desktop Platforms
Reader Mode is an accessibility feature which offers a simplified version of the
original page that focuses on the “core” text, stripping out extraneous images,
UI, scripts, and other elements. It is launched on Android as “Simplified View”.

Reader Mode is based on the DOM distiller project which provides functionality
for simplifying a webpage. This document focuses on how the
[DOM distiller](https://chromium.googlesource.com/chromium/dom-distiller)
project is integrated into Chrome on Desktop.

## Overview

Desktop Reader Mode is hidden behind a
[base::Feature](https://source.chromium.org/chromium/chromium/src/+/main:components/dom_distiller/core/dom_distiller_features.cc)
flag, ‘enable-reader-mode’. To run Chrome with Reader Mode, set the “Enable
Reader Mode” flag to “Enabled” in chrome://flags or start Chrome with
--enable-feature=”ReaderMode”.

There's also a flag that instead exposes a Setting; with this variant, users
need to first enable the Setting once and then they can use Reader Mode
on any supported page. Enabling this variant requires a long command-line
argument:

```
--enable-features="ReaderMode<FakeStudy" --force-fieldtrials=FakeStudy/FakeGroup --force-fieldtrial-params="FakeStudy.FakeGroup:discoverability/offer-in-settings"
```

### Code Locations

Most of Reader Mode code is in components/dom_distiller (see the
[DOM distiller project](https://chromium.googlesource.com/chromium/dom-distiller)).
It is tied into Chrome via hooks in chrome/browser/dom_distiller (Desktop) and
chrome/browser/android/dom_distiller (Android).

### Tests

Most Reader Mode tests are in components_unittests or components_browsertests.
Tests for integration with Chrome Desktop are in browser_tests, including tests
of ReaderModeIconView and dom_distiller/tab_utils.h.

### Bugs

Reader Mode bugs should be filed under
[UI>Browser>ReaderMode](https://bugs.chromium.org/p/chromium/issues/list?q=component:UI%3EBrowser%3EReaderMode)
in crbug.com.

## How Reader Mode works in Desktop Chrome

### Deciding whether to offer Reader Mode

Reader Mode classifies all pages visited by the user as “distillable” or
“not distillable”. A page is distillable, roughly speaking, when it has a
http or https scheme, contains an article and when DOM Distiller is likely to
accurately extract its core content. For example, many news articles are
distillable because they mostly consist of a single column of core text. In
contrast, the Wikipedia main page is not distillable because it contains several
unrelated text areas that are of roughly equal importance.

The [DistillabilityAgent](https://cs.chromium.org/chromium/src/components/dom_distiller/content/renderer/distillability_agent.h),
located in the renderer process, examines the page contents whenever the
compositor makes a meaningful change to the layout, which happens 1 to 3 times
as the page loads. It then uses one of several different heuristics to determine
whether the page is distillable or not. The browser receives the result obtained
by the DistillabilityAgent for a given web contents via the
[DistillabilityService](https://cs.chromium.org/chromium/src/components/dom_distiller/content/common/mojom/distillability_service.mojom),
which is wrapped by a helper class, the
[DistillabilityDriver](https://cs.chromium.org/chromium/src/components/dom_distiller/content/browser/distillability_driver.h).
The DistillabilityDriver packages this information as a
[DistillabilityResult](https://cs.chromium.org/chromium/src/components/dom_distiller/content/browser/distillable_page_utils.h),
forwards it to all registered observers, and caches it.

### Toggling Reader Mode
Users can toggle reader mode using an omnibox icon or an option, Toggle Reader
Mode, in the “customize and control Chrome” menu, both of which execute
BrowserCommands [ToggleDistilledView()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/browser_commands.cc;bpv=1;bpt=1;l=1364?q=browser_commands%20dom_distiller&ss=chromium%2Fchromium%2Fsrc).

[ReaderModeIconView](https://cs.chromium.org/chromium/src/chrome/browser/ui/views/reader_mode/reader_mode_icon_view.h)
is a DistillabilityObserver and sets its visibility based on the latest result
for the currently active web contents. Reader Mode on desktop only considers
whether the page is a distilled page, or, if not, the field
DistillabilityResult::is_distillable when deciding whether to display the icon;
the other fields are ignored. The icon’s visibility is updated when
 * the user navigates to a new page within the same tab,
 * the user switches tabs, and
 * when a new distillability result is available.

### Representing Reader Mode in the tab strip and omnibox
Reader Mode should be considered a way of viewing a page, rather than a separate
page to users. For this reason, we display most of the original page’s
information:

 * The omnibox displays the original page URL, minus the scheme.
    * The actual URL of the Reader Mode page is still “chrome-distiller://”
    which is still returned from WebContents::GetLastCommittedURL() and
    WebContents::GetVisibleURL().
    * A special case in LocationBarModel::GetFormattedURL converts from
    “chrome-distiller://” URLs to the original URL minus the scheme
      * However, if a user types in an invalid chrome-distiller:// URL,
      it will be displayed in the omnibox, because it doesn’t correspond to any
      article.
    * Users cannot copy the hidden “chrome-distiller://” URLs, instead
    OmniboxEditModel::AdjustTextForCopy converts Reader Mode URLs to their
    original URLs, if the chrome-distiller:// url encodes a valid original URL.
 * The security badge shows a Reader Mode-specific icon plus the phrase
 “Reader Mode”
    * SecurityState’s GetSecurityLevel returns SecurityLevel::NONE for
    “chrome-distiller://” scheme pages. Distilled pages should not contain forms,
    payment handlers, or other JS from the original URL, so they won’t be
    affected by downgraded security level.
 * The tab strip displays the original page’s favicon.
    * ChromeFaviconClient is used to check if a URL is a Reader Mode page and
    if so gets the original page’s URL to use for fetching the favicon.
 * Bookmarking the distilled page actually bookmarks the original article
    * BookmarkUtils GetURLToBookmark and GetURLAndTitleToBookmark both check for
    whether a page is a distilled page before extracting the original page
    title/URL from the distilled URL.

The page title is the original page title with “- Reader Mode” added to the end.
For example, if the original page’s title is “An Interesting Article - A
Website”, the tab strip will display “An Interesting Article - A Website -
Reader Mode” when the user activates Reader Mode.

#### Representing page security

On desktop Reader Mode there are strict security checks in place to ensure that
SecurityLevel::NONE is appropriate. This includes:
 * Only pages with SecurityLevel::SECURE are considered distillable (checked in
 DistillabilityDriver::OnDistillability)
 * Subresources with mixed content are not loaded on Reader Mode pages (enforced
 in DomDistillerViewerSource::StartDataRequest)
 * Subresources with certificate errors are not loaded on Reader Mode pages
 (checked in ChromeContentBrowserClient::ShouldDenyRequestOnCertificateError)
On Android, we do not show a custom security indicator or have a specific
SecurityLevel, so it is not necessary to do such strict checking.

### Extracting core page content
Reader Mode uses Chrome’s built-in DOM Distiller to generate the simplified
version of the page. See the
[DOM Distiller project](https://github.com/chromium/dom-distiller) on GitHub and
this (Google-internal) [presentation](https://docs.google.com/presentation/d/1etC7ghAU89ec-UeJQ90q4KbHJHH6owfl7OactTcJvCc/edit#slide=id.p)
for more information on how DOM Distiller works and how to debug it. From
Chrome’s perspective, distilling the page to extract core page content can be
considered a black box.

In short, content is extracted from the fully rendered article via Javascript
(specifically from the compiled Javascript file built from DOM Distiller,
[domdistiller.js](https://source.chromium.org/chromium/chromium/src/+/main:third_party/dom_distiller_js/dist/js/domdistiller.js))
into a DistilledPageProto.

The DOM Distiller retrieves the currently loaded document’s DOM and converts it
to a simplified HTML string using the following process:
 1. Remove hidden elements from the set of elements to examine.
 2. Identify important elements by passing them through a series of filters,
 marking elements as “content” or “not content” based on factors such as number
 of words in the element, location on the page relative to other elements, and
 image size.
 3. Create the output HTML by concatenating the string representation of
 elements marked as “content”.

The distilled page is served on a locally stored file with a URI composed of
the following:
 * Scheme: chrome-distiller://
 * Host: a random string plus a hash of the entire original URL
 * Query: parameters giving distillation start time and the original page URL
 and title

### Displaying Reader Mode Pages

A [DistilledPageProto](https://source.chromium.org/chromium/chromium/src/+/main:components/dom_distiller/core/proto/distilled_page.proto)
is created for each page distilled, and is used to generate the HTML of the
distilled page.

Pages are loaded by
[DomDistillerViewerSource](https://source.chromium.org/chromium/chromium/src/+/main:components/dom_distiller/content/browser/dom_distiller_viewer_source.h),
which serves the HTML and resources for viewing pages. After the DOM is
initially loaded, the contents are populated via Javascript.
