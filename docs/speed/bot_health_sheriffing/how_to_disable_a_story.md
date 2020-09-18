# How to disable a failing test/story on the perf waterfall

To disable a failing test/story, the first step is to figure
out if the failing thing is gtest or Telemetry, then you can
follow the below directions to disable the failing test/story.

# How to tell if the thing that is failing is Gtest or Telemetry

When a Telemetry benchmark story fails, the format of the output is
`<benchmark name>/<story name>`

When a gtest C++ executable fails, the format of the output is
`<executable name>`

Also, usually gtests executables are named something that ends
with `tests` such as `components_perftests` or `browser_performance_tests`
As of January 2020, no benchmarks are named in a similar fashion.

# How to disable a failing Telemetry benchmark or story

## Modify [`tools/perf/expectations.config`](https://cs.chromium.org/chromium/src/tools/perf/expectations.config?q=expectations.config&sq=package:chromium&dr)

Start a fresh branch in an up-to-date Chromium checkout. If you're unsure of how to do this, [see these instructions](https://www.chromium.org/developers/how-tos/get-the-code).


In your editor, open up [`tools/perf/expectations.config`](https://cs.chromium.org/chromium/src/tools/perf/expectations.config?q=expectations.config&sq=package:chromium&dr).

You'll see that the file is divided into sections sorted alphabetically by benchmark name. Find the section for the benchmark in question. (If it doesn't exist, add it in the correct alphabetical location.)

Each line in this file looks like:

    crbug.com/12345 [ conditions ] benchmark_name/story_name [ Skip ]

and consists of:

* A crbug tracking the story failure

* A list of space-separated tags describing the platforms on which the story will be disabled. A full list of these tags is available [at the top of the file](https://cs.chromium.org/chromium/src/tools/perf/expectations.config?type=cs&q=tools/perf/expectations.config&sq=package:chromium&g=0&l=5). (Note that these conditions are combined via a logical AND, so a platform must meet all conditions to be disabled.)

* The benchmark name followed by a "/"

* The story name, or an asterisk if the entire benchmark should be disabled

* The string "`[ Skip ]`", which denotes that the test should be skipped

Add a new line for each story that you need to disable, or an asterisk if you're disabling the entire benchmark. Multiple lines are also necessary to disable a single story on multiple platforms that lack a common tag.

For example, an entry disabling a particular story might look like:

    crbug.com/738453 [ Nexus_6 ] blink_perf.layout/subtree-detaching.html [ Skip ]


whereas an entry disabling a benchmark on an entire platform might look like:

    crbug.com/593973 [ Android_Svelte ] blink_perf.layout/* [ Skip ]

## Submit changes

Once you've committed your changes locally, your CL can be submitted with:

- `No-Try: True`
- `Tbr: `someone from [`tools/perf/OWNERS`](https://cs.chromium.org/chromium/src/tools/perf/OWNERS?q=tools/perf/owners&sq=package:chromium&dr)
- `CC: `benchmark owner found in [this spreadsheet](https://docs.google.com/spreadsheets/u/1/d/1xaAo0_SU3iDfGdqDJZX_jRV0QtkufwHUKH3kQKF3YQs/edit#gid=0)
- `Bug: `tracking bug

*Please make sure to CC the benchmark owner so that they're aware that they've lost coverage.*

The `Tbr:` and `No-Try:` are permitted and recommended so long as the only file changed is `tools/perf/expectations.config`. If your change touches real code rather than just that configuration data, you'll need a real review before submitting it.

# How to disable a failing gtest perf test

See generic Chromium build sheriff directions for how to disable a gtest [here](https://www.chromium.org/developers/tree-sheriffs/sheriff-details-chromium#TOC-How-do-I-disable-a-flaky-test-)
To find the logs of the failing test from Milo, click into the
isolated output directory of the shard that the failing test
was run on and the benchmark log for the failing test should be
there.
