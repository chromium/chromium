# Addressing Flaky GTests

## Understanding builder results

[LUCI Analysis](https://luci-analysis.appspot.com/p/chromium/clusters) lists the
top flake clusters of tests along with any associated bug and failure counts in
different contexts.

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

Another good solution is to use
*Swarming* -- which will let you mimic bot conditions to better reproduce flakes
that actually occur on CQ bots.

### Swarming
For a more detailed dive into swarming you can follow this
[link](https://chromium.googlesource.com/chromium/src/+/master/docs/workflow/debugging-with-swarming.md#authenticating).

As an example, suppose we have built Chrome using the GN args from
above into a directory `out/linux-rel`, then we can simply run this command
within the `chromium/src` directory:

```
tools/run-swarmed.py out/linux-rel browser_tests -- --gtest_filter="*<YOUR_TEST_NAME_HERE>*" --gtest_repeat=20 --gtest_also_run_disabled_tests
```

This allows us to quickly iterate over errors using logs to reproduce flakes and
even fix them!

>TODO: Add more tips for reproducing flaky tests

## Debugging the flaky test

If the test is flakily timing out, consider any asynchronous code that may cause
race conditions, where the test subject may early exit and miss a callback, or
return faster than the test can start waiting for it (i.e. make sure event
listeners are spawned before invoking the event). Make sure event listeners are
for the proper event instead of a proxy (e.g. [Wait for the correct event in
test](https://chromium.googlesource.com/chromium/src/+/6da09f7510e94d2aebbbed13b038d71c511d6cbc)).

Consider possible bugs in the system or test infrastructure (e.g. [races in
glibc](https://bugs.chromium.org/p/chromium/issues/detail?id=1010318)).

For browsertest flakes, consider possible inter-process issues, such as the
renderer taking too long or returning something unexpected (e.g. [flaky
RenderFrameHostImplBrowserTest](https://bugs.chromium.org/p/chromium/issues/detail?id=1120305)).

For browsertest flakes that check EvalJs results, make sure test objects are not
destroyed before JS may read their values (e.g. [flaky
PaymentAppBrowserTest](https://chromium.googlesource.com/chromium/src/+/6089f3480c5036c73464661b3b1b6b82807b56a3)).

For browsertest flakes that involve dialogs or widgets, make sure that test
objects are not destroyed because focus is lost on the dialog (e.g [flaky AccessCodeCastHandlerBrowserTest](https://chromium-review.googlesource.com/c/chromium/src/+/3951132)).

## Preventing similar flakes

Once you understand the problem and have a fix for the test, think about how the
fix may apply to other tests, or if documentation can be improved either in the
relevant code or this flaky test documentation.


[bot_gn_args]: images/bot_gn_args.png
