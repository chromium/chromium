# CQ

This document describes how the Chromium Commit Queue (CQ) is structured and
managed. This is specific for the Chromium CQ. Questions about other CQs should
be directed to infra-dev@chromium.org.

[TOC]

## Purpose

The Chromium CQ exists to test developer changes before they land into
[chromium/src](https://chromium.googlesource.com/chromium/src/). It runs all the
test suites which a given CL affects, and ensures that they all pass.

## Options

The Chromium CQ supports a variety of options that can change what it checks.

> These options are supported via git footers. They must appear in the last
> paragraph of your commit message to be used. See `git help footers` or
> [git_footers.py][1] for more information.

* `Commit: false`

  You can mark a CL with this if you are working on experimental code and do not
  want to risk accidentally submitting it via the CQ. The CQ will immediately
  stop processing the change if it contains this option.

* `Cq-Include-Trybots: <trybots>`

  This flag allows you to specify some additional bots to run for this CL, in
  addition to the default bots. The format for the list of trybots is
  "bucket:trybot1,trybot2;bucket2:trybot3".

* `No-Presubmit: true`

  If you want to skip the presubmit check, you can add this line, and the commit
  queue won't run the presubmit for your change. This should only be used when
  there's a bug in the PRESUBMIT scripts. Please check that there's a bug filed
  against the bad script, and if there isn't, [file one](https://crbug.com/new).

* `No-Tree-Checks: true`

  Add this line if you want to skip the tree status checks. This means the CQ
  will commit a CL even if the tree is closed. Obviously this is strongly
  discouraged, since the tree is usually closed for a reason. However, in rare
  cases this is acceptable, primarily to fix build breakages (i.e., your CL will
  help in reopening the tree).

* `No-Try: true`

  This should only be used for reverts to green the tree, since it skips try
  bots and might therefore break the tree. You shouldn't use this otherwise.

* `Tbr: <username>`

  [See policy](https://chromium.googlesource.com/chromium/src/+/master/docs/code_reviews.md#TBR-To-Be-Reviewed)
  of when it's acceptable to use TBR ("To be reviewed"). If a change has a TBR
  line with a valid reviewer, the CQ will skip checks for LGTMs.

## FAQ

### What exactly does the CQ run?

CQ runs the jobs specified in [commit-queue.cfg][2]. See
[`cq-builders.md`](https://chromium.googlesource.com/chromium/src/+/master/src/infra/config/generated/cq-builders.md)
for an auto generated file with links to information about the builders on the
CQ.

Some of these jobs are experimental. This means they are executed on a
percentage of CQ builds, and the outcome of the build doesn't affect if the CL
can land or not. See the schema linked at the top of the file for more
information on what the fields in the config do.

The CQ has the following structure:

* Compile all test suites that might be affected by the CL.
* Runs all test suites that might be affected by the CL.
    * Many test suites are divided into shards. Each shard is run as a separate
      swarming task.
    * These steps are labeled '(with patch)'
* Retry each shard that has a test failure. The retry has the exact same
  configuration as the original run. No recompile is necessary.
    * If the retry succeeds, then the failure is ignored.
    * These steps are labeled '(retry shards with patch)'
    * It's important to retry with the exact same configuration. Attempting to
      retry the failing test in isolation often produces different behavior.
* Recompile each failing test suite without the CL. Rerun each failing test
  suite in isolation.
    * If the retry fails, then the fail is ignored, as it's assumed that the test
      is broken/flaky on tip of tree.
    * These steps are labeled '(without patch)'
* Fail the build if there are tests which failed in both '(with patch)' and
  '(retry shards with patch)' but passed in '(without patch)'.

### Why did my CL fail the CQ?

Please follow these general guidelines:

1. Check to see if your patch caused the build failures, and fix if possible.
1. If compilation or individual tests are failing on one or more CQ bots and you
   suspect that your CL is not responsible, please contact your friendly
   neighborhood sheriff by filing a
   [sheriff bug](https://bugs.chromium.org/p/chromium/issues/entry?template=Defect%20report%20from%20developer&labels=Sheriff-Chromium&summary=%5BBrief%20description%20of%20problem%5D&comment=What%27s%20wrong?).
   If the code in question has appropriate OWNERS, consider contacting or CCing
   them.
1. If other parts of CQ bot execution (e.g. `bot_update`) are failing, or you
   have reason to believe the CQ itself is broken, or you can't really
   tell what's wrong, please file a [trooper bug](https://g.co/bugatrooper).

In both cases, when filing bugs, please include links to the build and/or CL
(including relevant patchset information) in question.

### How do I add a new builder to the CQ?

There are several requirements for a builder to be added to the Commit Queue.

* All the code for this configuration must be in Chromium's public repository or
  brought in through [src/DEPS](../../DEPS).
* Setting up the build should be straightforward for a Chromium developer
  familiar with existing configurations.
* Tests should use existing test harnesses i.e.
  [gtest](../../third_party/googletest).
* It should be possible for any committer to replicate any testing run; i.e.
  tests and their data must be in the public repository.
* Median cycle time needs to be under 40 minutes for trybots. 90th percentile
  should be around an hour (preferrably shorter).
* Configurations need to catch enough failures to be worth adding to the CQ.
  Running builds on every CL requires a significant amount of compute resources.
  If a configuration only fails once every couple of weeks on the waterfalls,
  then it's probably not worth adding it to the commit queue.

Please email dpranke@chromium.org, who will approve new build configurations.

### How do I ensure a trybot runs on all changes to a specific directory?

Several builders are included in the CQ only for changes that affect specific
directories. These used to be configured via Cq-Include-Trybots footers
injected at CL upload time. They are now configured via `location_regexp` fields
in [commit-queue.cfg][2], e.g.

```
  builders {
    name: "chromium/try/my-specific-trybot"
    location_regexp: ".+/{+]/path/to/my/specific/directory/.+"
  }
```

## Flakiness

The CQ can sometimes be flaky. Flakiness is when a test on the CQ fails, but
should have passed (commonly known as a false negative). There are a few common
causes of flaky tests on the CQ:

* Machine issues; weird system processes running, running out of disk space,
  etc...
* Test issues; individual tests not being independent and relying on the order
  of tests being run, not mocking out network traffic or other real world
  interactions.

The CQ mitigates flakiness by retrying failed tests. The core tradeoff in retry
policy is that adding retries increases the probability that a flaky test will
land on tip of tree sublinearly, but mitigates the impact of the flaky test on
unrelated CLs exponentially.

For example, imagine a CL that adds a test that fails with 50% probability. Even
with no retries, the test will land with 50% probability. Subsequently, 50% of
all unrelated CQ attempts would flakily fail. This effect is cumulative across
different flaky tests. Since the CQ has roughly ~20,000 unique flaky tests,
without retries, pretty much no CL would ever pass the CQ.

## Help!

Have other questions? Run into any issues with the CQ? Email
infra-dev@chromium.org, or file a [trooper bug](https://g.co/bugatrooper).


[1]: https://chromium.googlesource.com/chromium/tools/depot_tools/+/HEAD/git_footers.py
[2]: ../../infra/config/commit-queue.cfg
