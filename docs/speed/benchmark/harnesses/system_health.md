# System Health tests

[TOC]

## Overview

The Chrome System Health benchmarking effort aims to create a common set of user
stories on the web that can be used for all Chrome speed projects. Our
benchmarks mimic average web users’ activities and cover all major web platform
APIs & browser features.

The web is vast, the possible combination of user activities is endless. Hence,
to get a useful benchmarking tool for engineers to use for preventing
regressions, during launches and day to day work, we use data analysis and work
with teams within Chrome to create a limited set of stories that can fit a
budget of 90 minutes machine time.

These are our key cover areas for the browser:
* Different user gestures: swipe, fling, text input, scroll & infinite scroll
* Video
* Audio
* Flash
* Graphics: css, svg, canvas, webGL
* Background tabs
* Multi-tab switching
* Back button
* Follow a link
* Restore tabs
* Reload a page
* ... ([Full tracking sheet](https://docs.google.com/spreadsheets/d/1t15Ya5ssYBeXAZhHm3RJqfwBRpgWsxoib8_kwQEHMwI/edit#gid=0))

Success to us means System Health benchmarks cast a wide enough net to
catch major regressions before they make it to the users. This also means
performance improvements to System Health benchmarks translate to actual wins
on the web, enabling teams to use these benchmarks for tracking progress with
the confidence that their improvement on the suite matters to real users.

To achieve this goal, just simulating user’s activities on the web is not
enough. We also partner with
[chrome-speed-metrics](https://groups.google.com/a/chromium.org/forum/#!forum/speed-metrics-dev)
team to track key user metrics on our user stories.


## How do I debug System Health regressions?

System health benchmarks run test cases against Chrome's key performance metrics.
There is more documentation about the metrics and debugging information for
regressions in the documentation in the docs for these benchmarks:
* [Memory](../../../memory-infra/memory_benchmarks.md) - memory:* metrics
* [Loading](loading.md) - timeToFirstContentfulPaint, timeToFirstMeaningfulPaint
* [Power](power_perf.md) - cpu_time_percentage_avg


## Where are the System Health stories?

All the System Health stories are located in
[tools/perf/page_sets/system_health/](../../../../tools/perf/page_sets/system_health/).

There are few groups of stories:
1. [Accessibility stories](../../../../tools/perf/page_sets/system_health/accessibility_stories.py)
2. [Background stories](../../../../tools/perf/page_sets/system_health/background_stories.py)
3. [Browsing stories](../../../../tools/perf/page_sets/system_health/browsing_stories.py)
4. [Chrome stories](../../../../tools/perf/page_sets/system_health/chrome_stories.py)
5. [Loading stories](../../../../tools/perf/page_sets/system_health/loading_stories.py)
6. [Multi-tab stories](../../../../tools/perf/page_sets/system_health/multi_tab_stories.py)
7. [Media stories](../../../../tools/perf/page_sets/system_health/media_stories.py)

## What is the structure of a System Health story?
A System Health story is a subclass of
[SystemHealthStory](https://cs.chromium.org/chromium/src/tools/perf/page_sets/system_health/system_health_story.py?l=44&rcl=d5f1f0821489a8311dc437fc6b70ac0b0d72b28b), for example:
```
class NewSystemHealthStory(SystemHealthStory):
  NAME = 'case:group:page:2018'
  URL = 'https://the.starting.url'
  TAGS = [story_tags.JAVASCRIPT_HEAVY, story_tags.INFINITE_SCROLL]
  SUPPORTED_PLATFORMS = platforms.ALL_PLATFORMS  # Default.
                                                 # or platforms.DESKTOP_ONLY
                                                 # or platforms.MOBILE_ONLY
                                                 # or platforms.NO_PLATFORMS

  def _Login(self, action_runner):
    # Optional. Called before the starting URL is loaded.

  def _DidLoadDocument(self, action_runner):
    # Optional. Called after the starting URL is loaded
    # (but before potentially measuring memory).
```

The name must have the following structure:
1.  **Case** (load, browse, search, …). User action/journey that the story
    simulates (verb). Stories for each case are currently kept in a separate
    file.
    Benchmarks using the System Health story set can specify which cases they want to
    include (see
    [SystemHealthStorySet](https://cs.chromium.org/chromium/src/tools/perf/page_sets/system_health/system_health_stories.py?l=16&rcl=e3eb21e24dbe0530356003fd9f9a8a94fb91d00b)).
2.  **Group** (social, news, tools, …). General category to which the page
    (item 3) belongs.
3.  **Page** (google, facebook, nytimes, …). The name of the individual page. In
    case there are multi pages, one can use the general grouping name like
    "top_pages", or "typical_pages".
4.  **Year** (2017, 2018, 2018_q3, ...). The year (and quarter if necessary for
    disambiguating) when the page is added. Note: this rule was added later,
    so the old System Health stories do not have this field.

In addition, each story also has accompanied tags that define its important
characteristics.
[Tags](../../../../tools/perf/page_sets/system_health/story_tags.py) are used as
the way to track coverage of System Health stories, so they should be as
detailed as needed to distinguish each System Health story from the others.

## How are System Health stories executed?
Given a System Health story set with N stories, each story is executed sequentially as
follows:

1.  Launch the browser
2.  Start tracing
3.  Run `story._Login` (no-op by default)
4.  Load `story.URL`
5.  Run `story._DidLoadDocument` (no-op by default)
6.  Measure memory (disabled by default)
7.  Stop tracing
8.  Tear down the browser

All the benchmarks using System Health stories tear down the browser after single story.
This ensures that every story is completely independent and modifications to the
System Health story set won’t cause as many regressions/improvements on the perf dashboard.

## How do I run a story locally?

Running on Pinpoint is best to determine the performance of a change, but
running locally is useful when you would like to dive deep into a story or
iterate faster on potential solutions to the problem. In order to run locally,
run the following:

```
$ tools/perf/run_benchmark benchmark_name --story-filter story_name \
    --allow-software-compositing
```

Here, `benchmark_name` could be something like `system_health.common_desktop`,
`story_name` could be `load:news:nytimes:2018` (find the exact name on Pinpoint),
and `--allow-software-compositing` is often required because System Health will
force a crash when the GPU is not available in order to avoid getting skewed
results.

## Should I add new System Health stories and how?

First, check this list of [System Health stories](https://docs.google.com/spreadsheets/d/1t15Ya5ssYBeXAZhHm3RJqfwBRpgWsxoib8_kwQEHMwI/edit#gid=0)
to see if your intended user stories are already covered by existing ones.

If there is a good reason for your stories to be added, please make one CL for
each of the new stories so they can be landed (and reverted if needed)
individually. On each CL, make sure that the perf trybots all pass before
committing.

Once your patch makes it through the CQ, you’re done… unless your story starts
failing on some random platform, in which case the perf bot sheriff will very
likely revert your patch and assign a bug to you. It is then up to you to figure
out why the story fails, fix it and re-land the patch.

Add new SystemHealthStory subclass(es) to either one of the existing files or a
new file in [tools/perf/page_sets/system_health/](../../../../tools/perf/page_sets/system_health).
The new class(es) will automatically be picked up and added to the story set.

Next you will need to record the story, validate it works, upload the new recording, and submit the
test case. To do so, follow
[these instructions](https://source.chromium.org/chromium/chromium/src/+/main:tools/perf/recording_benchmarks.md).
