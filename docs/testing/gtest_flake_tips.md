# Addressing Flaky GTests

## Understanding builder results

The [Flake portal](https://analysis.chromium.org/p/chromium/flake-portal/flakes)
links to the flake occurrences of various tests on various bots. On the flake
occurrences page for a specific test, clicking on any of the timestamps takes
you to the bot run that flaked.

![flake_portal_occurrences]

You can then search the page for the flaky test name to view
details on how it failed. The failure will either be in a unittest run or
browsertest run. Note that the flake may either be detected as a flaky failure
if it passed on a following run (a "cq hidden flake" on the flake portal), or it
could have flaked multiple times causing the bot run to fail (a "cq false
rejection" on the flake portal). The build step output provides a link to your
test output. If your flaky test has both hidden flakes and false rejections,
take a look at the output for both as they may provide different hints toward
the issue.

![flaky_build_step]

Compare the flake output to the expected output when the test passes. Sometimes
observing the output is enough to narrow down the issue. However, sometimes
there’s very little output or it’s not that useful, such as when the test times
out. In this case you can try and add more logging and reproduce the flake.

## Reproducing the flaky test

If debugging via bot is too slow or you otherwise need to drill further into the
cause of the flake, you can try to reproduce the flake locally. Reproducing the
flake can be difficult, so it can help to try and replicate the test environment
as closely as possible.

Copy the gn args from one of the bots where the flake occurs, and try to choose
a bot close to your system, i.e. linux-rel if you're building on linux. To get
the gn args, you can again click on the timestamp in the flake portal to view
the bot run details, and search for the "lookup GN args" build step to copy the
args.

![bot_gn_args]

Build and run the test locally. Depending on the frequency of the flake, it may
take some time to reproduce. Some helpful flags:
 - --gtest_repeat=100
 - --gtest_also_run_disabled_tests (if the flaky test(s) you're looking at have
been disabled)

If you're unable to reproduce the flake locally, you can also try uploading your
patch with the debug logging and flaky test enabled to try running the bot to
reproduce the flake with more information.

>TODO: Add more tips for reproducing flaky tests

## Debugging the flaky test

If the test is flakily timing out, consider any asynchronous code that may cause
race conditions, where the test subject may early exit and miss a callback, or
return faster than the test can start waiting for it (i.e. make sure event
listeners are spawned before invoking the event).

For browsertest flakes, consider possible inter-process issues, such as the
renderer taking too long or returning something unexpected.

>TODO: Add more tips for common flake causes

## Preventing similar flakes

Once you understand the problem and have a fix for the test, think about how the
fix may apply to other tests, or if documentation can be improved either in the
relevant code or this flaky test documentation.


[flake_portal_occurrences]: images/flake_portal_occurrences.png
[flaky_build_step]: images/flaky_build_step.png
[bot_gn_args]: images/bot_gn_args.png