# Ad Tagging in Chromium

Chromium is able to detect some ads and the resources they load in the
browser. This enables the browser to measure the size, performance, and count of
ads displayed to our users. It also allows the browser to intervene on the
user’s behalf when ads run counter to the user’s interest (e.g., by using
excessive resources or by engaging in [abusive
behavior](https://support.google.com/webtools/answer/7347327).

The ad detection infrastructure is called Ad Tagging. Ad Tagging works by
matching resource requests against a filter list (see how the list is
[generated](https://chromium.googlesource.com/chromium/src.git/+/main/components/subresource_filter/FILTER_LIST_GENERATION.md))
to determine if they’re ad requests.

The current filter list and historical versions can be found [here](https://github.com/chromium/chromium-ads-detection).

Any requests matching the filter are tagged
as ads. Further, requests (and some DOM elements such as iframes) made on behalf
of previously tagged scripts are also tagged as ads by the
[AdTracker](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/frame/ad_tracker.h). An
iframe will be marked as an ad iframe if its url matches the filter list, if
tagged script is involved in the creation of the iframe, or if its parent frame
is an ad iframe. The main frame of a page will never be tagged as an ad. Any
request made within an ad iframe is considered an ad resource request.

## Basic Architecture

### Subresource Filter
The [Subresource
Filter](https://chromium.googlesource.com/chromium/src.git/+/main/components/subresource_filter/README.md)
loads the filter list and matches urls against it. The list is distributed via
the component updater. This same list and component is used for blocking ads on
abusive sites and those that violate the Better Ads Standard.

Each subresource request in the render process is processed by the subresource
filter before the request is sent. If it matches the list, the ResourceRequest
is tagged as an ad.

Each subframe navigation request is processed in the browser process by the
SubresourceFilter and if the final URL in the navigation matches the list then
the render process is told (via the SubresourceFilterAgent) and the iframe is
marked as an ad iframe.

### AdTracker
The
[AdTracker](https://cs.chromium.org/chromium/src/third_party/blink/renderer/core/frame/ad_tracker.h)
keeps track of each script subresource that is considered an ad. Any time that a
new resource is requested or an iframe is created, V8's stack is scanned and if
any of the known ad scripts are in the stack then the resource request or iframe
is considered an ad.

