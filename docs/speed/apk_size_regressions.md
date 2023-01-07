# How to Deal with Android Size Alerts

Googlers, see also: go/chrome-binary-size-garderning

 >
 > Not all alerts should not have a bug created for them. Please read on...
 >

[TOC]

## Step 1: Identify the Commit

### Monochrome.minimal.apks Alerts (Single Commit)

 * Zoom in on the graph to make sure the alert is not
   [off-by-one](https://github.com/catapult-project/catapult/issues/3444)
   * Replace `&num_points=XXXX` with `&rev=COMMIT_POSITION` in the URL.
   * It will be obvious from this whether or not the point is off. Use the
     "nudge" feature to correct it when this happens.

### Monochrome.minimal.apks Alerts (Multiple Commits or Rolls)

 * Bisects [will not help you](https://bugs.chromium.org/p/chromium/issues/detail?id=678338).
 * For rolls, you can sometimes guess the commit(s) that caused the regression
   by looking at the `android-binary-size` trybot result for the roll commit, or
   by looking for "Binary-Size:" footers in the blamelist.
 * For V8 rolls, try checking the [V8 size graph](https://chromeperf.appspot.com/report?sid=59435a74c93b42599af4b02e2b3df765faef4685eb015f8aaaf2ecf7f4afb29c)
   to see if any jumps correspond with a CL in the roll.
 * Otherwise, use [diagnose_bloat.py](/tools/binary_size/README.md#diagnose_bloat_py)
   in a [local Android checkout](/docs/android_build_instructions.md)
   to build all commits locally and find the culprit.
   * If there were multiple commits due to a build breakage, use `--apply-patch`
     with the fixing commit (last one in the range).

**Example:**

     tools/binary_size/diagnose_bloat.py AFTER_GIT_REV --reference-rev BEFORE_GIT_REV --all [--subrepo v8] [--apply-patch AFTER_GIT_REV]

 * You can usually find the before and after revs in the roll commit message
([example](https://chromium.googlesource.com/chromium/src/+/10c40fd863f4ae106650bba93b845f25c9b733b1))
    * You may need to click through for the list of changes to find the actual
      first commit hash since some rollers (e.g. v8's) use an extra commit for
      tagging. In the linked example `BEFORE_GIT_REV` would actually be
      `876f37c` and not `c1dec05f`.

### SystemWebviewGoogle.apk Alerts

* Check if the same increase happened in Monochrome.minimal.apks.
   * The goal is to ensure nothing creeps into webview unintentionally.

## Step 2: File Bug or Silence Alert

* If the commit message's `Binary-Size:` footer clearly justifies the size
  increase, silence the alert.
* If the commit is a revert / reland, silence the alert.
* If the `android-binary-size` bot on the associated code review did not
  detect a regression, and the size increase is from native code, then the
  disparity is likely due to AFDO profiles not being active on perf bots.
  Silence the alert.

Otherwise, file a bug.

 * Change the bug's title from `X%` to `XXkb`
 * Assign to commit author (often this is done automatically)
 * Set description to (replacing **bold** parts):

> Caused by "**First line of commit message**"
>
> Commit: **abc123abc123abc123abc123abc123abc123abcd**
>
> Link to size graph:
> [https://chromeperf.appspot.com/report?sid=6269078068c45a41e23f5ee257da65d3f02da342849cdf3bde6aed0d5c61e450&num_points=10&rev=**$CRREV**](https://chromeperf.appspot.com/report?sid=6269078068c45a41e23f5ee257da65d3f02da342849cdf3bde6aed0d5c61e450&num_points=10&rev=480214)<br>
> Link to trybot result:
> [https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size/**$TRYJOB_NUMBER**](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size/11111)
>
> Debugging size regressions is documented at:
> https://chromium.googlesource.com/chromium/src/+/main/docs/speed/apk_size_regressions.md#Debugging-Apk-Size-Increase
>
> Based on the trybot result: **20kb of native code, 8kb of pngs. *(or some other explanation as to what caused the growth).***
>
> It's not clear to me whether or not this increase was expected.<br>
> Please have a look and either:
>
> 1. Close as "Won't Fix" with a short justification, or
> 2. Land a revert / fix-up.
>
> _**Optional addition:**_
>
> It typically takes about a week of engineering time to reduce binary size by
> 50kb so we'd really appreciate you taking some time exploring options to
> address this regression!

* If the regression is >50kb, add ReleaseBlock-Stable **M-##** (next branch cut).*
* If the regression was caused by a non-Googler, assign it to the closest Googler
  to the patch (e.g. reviewer). The size graphs are [not public](https://bugs.chromium.org/p/chromium/issues/detail?id=962483).

# Debugging Apk Size Increase

It typically takes about a week of engineering time to reduce binary size by
50kb so it's important that an effort is made to address all new regressions.
For more about binary size, see [binary_size_explainer.md].

[binary_size_explainer.md]: /docs/speed/binary_size/binary_size_explainer.md

## Step 1: Identify what Grew

Figure out which file within the `.apk` increased (native library, dex, pak
resources, etc.) by looking at the trybot results or size graphs that were
linked from the bug (if it was not linked in the bug, see above).

**See [//docs/speed/binary_size/metrics.md](/docs/speed/binary_size/metrics.md)
for a description of high-level binary size metrics.**

**See [//tools/binary_size/README.md](/tools/binary_size/README.md)
for a description of binary size tools.**

## Step 2: Analyze

See [optimization advice](/docs/speed/binary_size/optimization_advice.md).

## Step 3: Give Up :/

If you aren't sure where to start and would like help with the investigation,
comment on the bug or reach out to binary-size@chromium.org to ask for help.

If you are pretty sure that your implementation is optimal(ish), add a comment
to the bug with the following:

1) A description of where the size is coming from (show that you spent the time
   to understand why your code translated to a large binary size).
2) What things you tried to reduce the size (show that you've at least read this
   doc and tried any relevant guidance).
3) Why your commit is "worth" the size increase. For new features, feel free
   to link to a design doc (which presumably includes the motivation for adding
   the feature).

Close the bug as "Won't Fix".
