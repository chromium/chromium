# //net Code Reviews

This page documents responsibilities of //net code reviewers. For the general
guidelines, see [Code Reviews](/docs/code_reviews.md).

## New features and behavior changes

*See*:
[The general flag guarding guidelines](/docs/flag_guarding_guidelines.md).

Changes that introduce new features or non-trivial behavior should be controlled
by a feature flag defined in [net/base/features.h](../base/features.h). This
allows us to disable the feature or revert to the previous behavior if serious
issues arise, without requiring an emergency fix or reverting CLs.

The term "non-trivial behavior changes" is ambiguous, and it is difficult to
define it clearly. Whether to add a flag should be determined on a case-by-case
basis. For example, when changing the interpretation of an HTTP header, it would
be necessary to add a flag.

Code whose behavior is influenced by the feature flag should be tested
both with and without the flag enabled, using
[INSTANTIATE_TEST_SUITE_P](https://google.github.io/googletest/reference/testing.html#INSTANTIATE_TEST_SUITE_P).

## Code coverage requirements

Code in //net is also used in Cronet which runs the code in very different
configurations to Chrome. This creates a risk that a configuration that is
untested in Chrome could cause breakage in an Android app. As one way to
mitigate this risk, //net has higher code coverage requirements than most of
Chrome.

Our aim is to have 100% unit test coverage of non-trivial reachable code paths.
Look for coverage gaps in Gerrit amd request additional tests if appropriate
while respecting the code author's time.

*See also*: [Code Coverage in Chromium](/docs/testing/code_coverage.md)

## Use of appropriate types

As in the rest of Chrome, `std::string` should be used for UTF-8 text and
`std::vector<uint8_t>` should be used for binary data. `net::IOBuffer` can
also be used for binary data, but use the newer
[spanified interfaces](/base/containers/span.h) and not the legacy interfaces
based on char* pointers.

A large amount of legacy code in //net uses `std::string` for binary data.
Try to avoid spreading this usage any further.

## Web exposing features

*See*:
[Launching Features](https://www.chromium.org/blink/launching-features/).

If the changes affect web API behavior or could have potential impacts for
Web developers, make sure to ask the authors to follow the guidelines
outlined on the "Launching Features" page.

[TLS Encrypted ClientHello](https://groups.google.com/a/chromium.org/g/blink-dev/c/CmlXjQeNWDI/m/hx-_4lNBAQAJ)
is an example that needed to follow the
guidelines, even though it didn't involve explicit changes in web exposed APIs.

## Standards

We do not implement standards just because they exist. If a CL implements
a standard that is not implemented in other browsers, make sure there is
an explainer that clearly describes the user benefit.

We implement standards from the WHATWG, W3C and IETF. Where they contradict,
we attempt to align with other browsers.

New HTTP headers should be using
[structured fields](https://datatracker.ietf.org/doc/html/rfc8941), or have a
good reason not to.

Try to verify that the implementation matches the standard.

We deviate from standards for privacy, security and iteroperability reasons.
Be careful of CLs that improve standards compliance at the cost of other
factors.

## Security

//net has access to sensitive user data. Every parser should have a
fuzzer.

Parsers should operate on `std::string_view` or `base::span<uint8_t>`.

## Performance

Most of the code in //net is user-visible performance critical. Look out
for common performance pitfalls

* Unnecessary copying and allocation.
  * Unnecessary string copies are common.
  * Beware of functions taking `const std::string&` parameters. Unless all
    current and future callers already have a string, this will result in
    silent string copies. Usually `std::string_view` is a better choice.
  * Using `std::vector::push_back()` without `reserve()` is a common source of
    unnecessary copies and allocations. Use `base::ToVector()` where
    appropriate.
* Duplicate work.
  * Is the same thing parsed multiple times?
* Algorithmic complexity
  * Could this O(N^2) algorithm be O(N log N)? Could this O(N) algorithm be
    O(1)?

## Dependencies and layering

Because of Cronet's strict size restrictions, the bar for adding new
dependencies to //net is very high.

Do not add a dependency on mojo.

//net is an implementation of the lower levels of
[the Fetch specification](https://fetch.spec.whatwg.org/), very roughly
corresponding to
[HTTP-network-or-cache fetch](https://fetch.spec.whatwg.org/#http-network-or-cache-fetch).
It should not know or care about higher-level concepts like
[windows](https://html.spec.whatwg.org/#window),
[frames](https://html.spec.whatwg.org/#frame) or
[CORS](https://fetch.spec.whatwg.org/#http-cors-protocol). This rule is
frequently bent for pragmatic reasons, but be cautious of CLs that talk about
higher-level Web Platform features.

Another way to answer the question "does it belong in //net?" is to ask
"Would an Android application using Cronet need this?"
