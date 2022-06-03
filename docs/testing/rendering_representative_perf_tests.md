# Representative Performance Tests for Rendering Benchmark

`rendering_representative_perf_tests` run a subset of stories from rendering
benchmark on CQ, to prevent performance regressions. For each platform there is
a `story_tag` which describes the representative stories used in this test.
These stories will be tested using the [`run_benchmark`](../../tools/perf/run_benchmark) script, and then the recorded values for the target metric (currently `frame_times`) will be
compared with the historical upper limit described in [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`][rep_perf_json].

These tests are currently running on CQ on:
* mac-rel
* win10_chromium_x64_rel_ng

[TOC]

## Investigating Representative perf test failures

The representative perf tests runs a set of stories and measures the `frame_times` metric during the interaction to determine performance.
The common reasons of failure are (1) the measured value is higher than the expected value and (2) crashes while running the tests.

In the case of difference between values, a message would be logged in the output of the test explaining so.
Example:`animometer_webgl_attrib_arrays higher average frame_times(21.095) compared to upper limit (17.062)` ([Link to the task](https://chromium-swarm.appspot.com/task?d=true&id=4a92ea7a6b40d010))
This means that the animometer_webgl_attrib_arrays story has the average frame_times of 21 ms and the recorded upper limit for the story (in the tested platform) is 17 ms.

In these cases the failed story will be ran three more times to make sure that this has not been a flake, and the new result (average of three runs) will be reported in the same format.
For deeper investigation of such cases you can find the traces of the runs in the isolated outputs of the test. In the isolated outputs directory look at `output.json` for the initial run and at `re_run_failures/output.json` for the three traces recorded from re-runs.
If the failure is as a result of an expected regression, please follow the instructions in the next section for the "Updating Expectations".

In the `output.json` file, you can find the name of the story and under the trace.html field of the story a gs:// link to the trace ([Example](https://isolateserver.appspot.com/browse?namespace=default-gzip&digest=f8961d773cdf0bf121525f29024c0e6c19d42e61&as=output.json)).
To download the trace run: `gsutil cp gs://link_from_output.json trace_name.html`

Also if tests fail with no specific messages in the output, it will be useful to check the {benchmark}/benchmark_log.txt file in the isolated outputs directory for more detailed log of the failure.

### Running representative perf tests locally

You can run the representative perf tests locally for more investigation, but it is important to note that the values may differ with the values reported on the bots as these tests can have different values for different hardware.

```
./testing/scripts/run_rendering_benchmark_with_gated_performance.py ./tools/perf/run_benchmark \
--benchmark rendering.desktop --isolated-script-test-output /tmp/temp_dir/ \
--isolated-script-test-perf-output /tmp/temp_dir
```
*rendering.mobile for running mobile representatives*


For investigation of crashes (or when the comparison of the values is not the focus) it might be easier to directly run the benchmark for the representative story tags such as:
*   `representative_win_desktop (benchmark: rendering.desktop)`
*   `representative_mac_desktop (benchmark: rendering.desktop)`
*   `representative_mobile (benchmark: rendering.mobile)`

```
./tools/perf/run_benchmark rendering.desktop --story-tag-filter representative_win_desktop
```

## Updating Expectations

There might be multiple reasons to skip a story in representative perf tests such as:
*   Tests results are flaky and the story needs to be skipped
*   Adding a new change with expected regression, so we need to skip the test, so that the change would pass on CQ), and adjust the upper limit later.
*   We want to add a new story to the representative set and the appropriate upper limit is not known yet

In these cases the story should not cause a failure but it needs to record the values for later adjustments.
As a result the preferred method to skip a story of the representative perf test is to mark the specific story as experimental in [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`][rep_perf_json] along with a bug referring to the cause of test suppression (flakiness, change with expected regression or experimenting with new representatives). This way the test will be run but the values will not be considered for failing the test.

To do so find the story under the affected platform in [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`][rep_perf_json] and add the "experimental" tag to it.

```
"platform": {
    "story_name": {
        "ci_095": 0.377,
        "avg": 31.486,
        "experimental": true,
        "_comment": "crbug.com/bug_id"
    },
}
```
*[Example Cl](https://chromium-review.googlesource.com/c/chromium/src/+/2208294)*


## Maintaining Representative Performance Tests

### Clustering the Benchmark and Choosing Representatives

The clustering of the benchmark is based on the historical values recorded for
`frame_times`. For steps on clustering the benchmark check [Clustering benchmark stories](../../tools/perf/experimental/story_clustering/README.md).

Currently there are three sets of representatives described by story tags below:
*   `representative_mac_desktop`
*   `representative_mobile`
*   `representative_win_desktop`

Adding more stories to representatives or removing stories from the set is
managed by adding and removing story tags above to stories in [rendering benchmark](../../tools/perf/page_sets/rendering).

### Updating the Upper Limits

The upper limits for averages and confidence interval (CI) ranges of
`frame_times` described in [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`][rep_perf_json] are used to passing or failing a test. These values are the 95 percentile of the past 30 runs of the test on each platform (for both average and CI).

This helps with catching sudden regressions which results in a value higher
than the upper limits. But in case of gradual regressions, the upper limits
may not be useful in not updated frequently. Updating these upper limits also
helps with adopting to improvements.

Updating these values can be done by running [`src/tools/perf/experimental/representative_perf_test_limit_adjuster/adjust_upper_limits.py`](../../tools/perf/experimental/representative_perf_test_limit_adjuster/adjust_upper_limits.py) and committing the changes.
The script will create a new JSON file using the values of recent runs in place
of [`src/testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json`][rep_perf_json].

[rep_perf_json]: ../../testing/scripts/representative_perf_test_data/representatives_frame_times_upper_limit.json