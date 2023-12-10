# About release ready filters

This directory is for test suites running on
[Chrome Release Ready builders](https://ci.chromium.org/p/chrome/g/chrome.release-ready/console),
where the test owners need to block release at test case level. It means that
all test cases must run and pass.

Because they are release critical, any changes against these filters must be
reviewed by release team, who understand the impact for Chrome release
quality. Without according change here, disabling or deleting the test from
source will fail the suite.

## Manual

1. Add a filter file in this folder with all test cases explicitly listed.
1. Append `--enforce-exact-positive-filter` to the test arg.

Config example:

```python
'foo_tests': {
    'args': [
        '--test-launcher-filter-file=../../testing/buildbot/filters/release_ready/YOUR FILTER',
        '--enforce-exact-positive-filter',
        ],
}
```

We will add rel-ready [trybots](https://crbug.com/1479548) to help developers
verify filter changes in 23Q4.
