# Sanitizer API

This implements the [Sanitizer API](https://wicg.github.io/sanitizer-api/).

## Status

A basic version of the Sanitizer API - chiefly the `Element.setHTML` method -
is available.

The full Sanitizer API is currently behind a flag:
* --enable-blink-features=SanitizerAPI or
* --enable-experimental-web-platform-features or
* chrome://flags#sanitizer-api

We are actively looking for feedback on the API. If you find problems or have
suggestions for how the API should change, please read the available issues
at https://github.com/WICG/sanitizer-api/issues and raise a new issue if your
suggestion isn't already covered.

As this is a cross-browser effort, suggestions concerning the API should go
to the standardisation group. Issues with Chromium's implementation should
go to https://bugs.chromium.org and use the
[Blink > SecurityFeatures > SanitizerAPI component](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ABlink%3ESecurityFeature%3ESanitizerAPI).

## Staged / Incremental Rollout

An initial version of the Sanitizer API is enabled by default. Additional
features are scheduled to be launched in stages. The API availability
can be controlled via flags:

* `--enable-blink-features=SanitizerAPI`: This includes the sanitization
  methods of the `Sanitizer` object, as specified as of 04/2022.
These APIs are likely to change.

The general `--enable-experimental-web-platform-features` flag implies the full
`--enable-blink-features=SanitizerAPI` feature set.

## Known Issues

The current implementation matches the specification as of 04/2022 and will be
updated as the specification develops. Known omissions relative to the
current spec are:

* Secure context: The current spec draft requires a secure context. This
  might change. Our implementation presently follows the draft.


## Tests

1. For WPT tests, please refer to
`third_party/blink/web_tests/external/wpt/sanitizer-api/` and
`third_party/blink/web_tests/wpt_internal/sanitizer-api/`.
2. For performance tests, please refer to
`third_party/blink/perf_tests/sanitizer-api/`.
3. For fuzzer tests, please refer to
`third_party/blink/renderer/modules/sanitizer_api/sanitizer_api_fuzzer.h`
