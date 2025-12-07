This directory contains test that rely in internal testing APIs as well as
legacy tests, which should be migrated to WPT over time (or simply removed
if we have the equivalent coverage in WPT).  When fixing a test flake in
this directory subtree, consider migrating to WPT if here only for legacy
reasons.

Please avoid adding new tests to this directory or any of its sub-folders. New
tests should be authored in [WPT](https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/web_tests/external/wpt/)
with few exceptions. The following guidelines may be useful for choosing where
to place a test:

* scroll-driven animation: external/wpt/scroll-animations
* CSS transition: external/wpt/css/css-transitions
* CSS animation: external/wpt/css/css-animations
* web animations API: external/wpt/web-animations
* animation worklet: external/wpt/animation-worklet
* animated paint worklet: external/wpt/css/css-paint-api
* animated transform: external/wpt/css/css-transforms/animation
* background animation: external/wpt/css/css-backgrounds/animations
* clip-path animation: external/wpt/css/css-masking/clip-path/animations

Exceptions to authoring in WPT are when an internal testing API is needed or
testing Chromium specific behavior (e.g. compositor thread).  Before adding
a new internal API or adding a test based on an existing testing API, check
if a similar problem has been solved in a WPT test. For example,
the functionality in the internal animation pause API is fully replaceable with
web-animation API calls that are supported on all browsers.

Please refer to README.md files in these WPT directories for more help on
writing effective tests.

In some cases, multiple folders may seem appropriate (e.g. a scroll-driven
transform), though even in these cases, one category may be more central to the
test (common theme across sub-tests). If the sub-tests feel too disjoint to fall
into a single category, then consider breaking into multiple tests.

