# Sanitizer API

This implements the [Sanitizer API](https://wicg.github.io/sanitizer-api/).

## Status

The Sanitizer API is currently behind a flag:
* --enable-blink-features=SanitizerAPI or
* --enable-experimental-web-platform-features or
* chrome://flags#sanitizer-api

In Mozilla Firefox, the Sanitizer API can be enabled in about:config by setting
dom.security.sanitizer.enabled flag to true.

We are actively looking for feedback on the API. If you find problems or have
suggestions for how the API should change, please read the available issues
at https://github.com/WICG/sanitizer-api/issues, and raise a new issue if your
suggestion isn't already covered.

As this is a cross-browser effort, suggestions concerning the API should go
to the standardisation group. Issues with Chromium's implementation should
go to https://bugs.chromium.org and use the
[Blink > SecurityFeatures > SanitizerAPI component](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3ESecurityFeature%3ESanitizerAPI)

## Known Issues

The current implementation matches the specification as of 09/2021 and will be
updated as the specification develops. Known omissions relative to the
current spec are:

* [MathML and SVG are not currently supported](https://github.com/WICG/sanitizer-api/issues/103)

* [Element.setHTML signature now has an options dictionary.](https://github.com/WICG/sanitizer-api/issues/130) (M97) The previous method signature is supported but deprecated, and will be removed before enabling the Sanitizer by default.

## Tests

1. For WPT tests, please refer to
`third_party/blink/web_tests/external/wpt/sanitizer-api/` and
`third_party/blink/web_tests/wpt_internal/sanitizer-api/`.
2. For performance tests, please refer to
`third_party/blink/perf_tests/sanitizer-api/`.
3. For fuzzer tests, please refer to
`third_party/blink/renderer/modules/sanitizer_api/sanitizer_api_fuzzer.h`
