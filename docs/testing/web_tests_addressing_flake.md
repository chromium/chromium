# Addressing Flaky Web Tests

## Understanding builder results

Often (e.g. by Flake Portal), you will be pointed to a particular build in which
your test has flaked. You will need the name of the specific build step that has
flaked; usually for Web Tests this is `blink_web_tests` but there are variations
(e.g. `not_site_per_process_blink_web_tests`).

On the builder page, find the appropriate step:

![web_tests_blink_web_tests_step]

While you can examine the individual shard logs to find your test output, it is
easier to view the consolidated information, so scroll down to the **archive
results for blink\_web\_tests** step and click the `layout_test_results` link:

![web_tests_archive_blink_web_tests_step]

This will open a new tab with the results viewer. By default your test should be
shown, but if it isn't then you can click the 'All' button in the 'Query' row,
then enter the test filename in the textbox beside 'Filters':

![web_tests_results_viewer_query_filter]

There are a few ways that a Web Test can flake, and what the result means may
depend on the [test type](writing_web_tests.md#Test-Types):

1. `FAIL` - the test failed. For reference or pixel tests, this means it did not
   match the reference image. For JavaScript tests, the test either failed an
   assertion *or* did not match the [baseline](web_test_expectations.md)
   `-expected.txt` file checked in for it.
   * For image tests, this status is reported as `IMAGE` (as in an image diff).
   * For Javascript tests, this status is reported as `TEXT` (as in a text
     diff).
1. `TIMEOUT` - the test timed out before producing a result. This may happen if
   the test is slow and normally runs close to the timeout limit, but is usually
   caused by waiting on an event that never happens. These unfortunately [do not
   produce any logs](https://crbug.com/487051).
1. `CRASH` - the browser crashed while executing the test. There should be logs
   associated with the crash available.
1. `PASS` - this can happen! Web Tests can be marked as [expected to
   fail](web_test_expectations.md), and if they then pass then that is an
   unexpected result, aka a potential flake.

Clicking on the test row anywhere *except* the test name (which is a link to the
test itself) will expand the entry to show information about the failure result,
including actual/expected results and browser logs if they exist.

In the following example, our flaky test has a `FAIL` result which is a flake
compared to its (default) expected `PASS` result. The test results (`TEXT` - as
explained above this is equivalent to `FAIL`), output, and browser log links are
highlighted.

![web_tests_results_viewer_flaky_test]

## Reproducing Web Test flakes

>TODO: add documentation on reproducing Web Test flakes

## Debugging flaky Web Tests

>TODO: add documentation on debugging flaky Web Tests

[web_tests_blink_web_tests_step]: images/web_tests_blink_web_tests_step.png
[web_tests_archive_blink_web_tests_step]: images/web_tests_archive_blink_web_tests_step.png
[web_tests_results_viewer_query_filter]: images/web_tests_results_viewer_query_filter.png
[web_tests_results_viewer_flaky_test]: images/web_tests_results_viewer_flaky_test.png
