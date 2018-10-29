# How to Deal with Android Size Alerts

 >
 > Not all alerts should not have a bug created for them. Please read on...
 >

[TOC]

## Step 1: Identify the Commit

### MonochromePublic.apk Alerts (Single Commit)

 * Zoom in on the graph to make sure the alert is not
   [off-by-one](https://github.com/catapult-project/catapult/issues/3444)
   * Replace `&num_points=XXXX` with `&rev=COMMIT_POSITION` in the URL.
   * It will be obvious from this whether or not the point is off. Use the
     "nudge" feature to correct it when this happens.

### MonochromePublic.apk Alerts (Multiple Commits or Rolls)

 * Bisects [will not help you](https://bugs.chromium.org/p/chromium/issues/detail?id=678338).
 * For rolls, you can sometimes guess the commit(s) that caused the regression
   by looking at the `android-binary-size` trybot result for the roll commit.
 * Otherwise, use [diagnose_bloat.py](https://chromium.googlesource.com/chromium/src/+/master/tools/binary_size/README.md#diagnose_bloat_py)
   in a [local Android checkout](https://chromium.googlesource.com/chromium/src/+/master/docs/android_build_instructions.md)
   to build all commits locally and find the culprit.
 * Go to step 2.

**Example:**

     tools/binary_size/diagnose_bloat.py AFTER_GIT_REV --reference-rev BEFORE_GIT_REV --subrepo v8 --all

 * You can usually find the before and after revs in the roll commit message
([example](https://chromium.googlesource.com/chromium/src/+/10c40fd863f4ae106650bba93b845f25c9b733b1))
    * Note that you may need to click through the link for the list of changes
      in order to find the actual first commit hash and use that one instead
      since some rollers (including v8) use extra commits for tagging not in
      master. In the linked example `BEFORE_GIT_REV` would actually be
      `876f37c` and not `c1dec05f`.

### Monochrome.apk (downstream) Alerts

 * The regression most likely already occurred in the upstream
   MonochromePublic.apk target. Look at the
   [graph of Monochrome.apk and MonochromePublic.apk overlaid](https://chromeperf.appspot.com/report?sid=cfc29eed1238fd38fb5e6cf83bdba6c619be621b606e03e5dfc2e99db14c418b&num_points=1500)
   to find the culprit and de-dupe with upstream alert.
 * If no upstream regression was found, look through the downstream commits
   within the given date range to find the culprit.
    * Via `git log --format=fuller` (be sure to look at `CommitDate` and not
      `AuthorDate`)
 * If the culprit is not obvious, follow the steps from the "multiple commits"
   section above, filing a bug and running `diagnose_bloat.py`
   (with `--subrepo=clank`).

## Step 2: File Bug or Silence Alert

* If the commit message's `Binary-Size:` footer clearly justifies the size
  increase, silence the alert.
* If the regression is < 100kb and caused by an AFDO roll, silence the alert.

Otherwise, file a bug (TODO: [Make this template automatic](https://github.com/catapult-project/catapult/issues/3150)):

 * Change the bug's title from `X%` to `XXkb`
 * Assign to commit author
 * Set description to (replacing **bold** parts):

> Caused by "**First line of commit message**"
>
> Commit: **abc123abc123abc123abc123abc123abc123abcd**
>
> Link to size graph:
> [https://chromeperf.appspot.com/report?sid=bb23072657e2d7ca892a1c3fa4643b1ee29b3a0a44d0732adda87168e89c0380&num_points=10&rev=**$CRREV**](https://chromeperf.appspot.com/report?sid=bb23072657e2d7ca892a1c3fa4643b1ee29b3a0a44d0732adda87168e89c0380&num_points=10&rev=480214)<br>
> Link to trybot result:
> [https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size/**$TRYJOB_NUMBER**](https://ci.chromium.org/p/chromium/builders/luci.chromium.try/android-binary-size/11111)
>
> Debugging size regressions is documented at:
> https://chromium.googlesource.com/chromium/src/+/master/docs/speed/apk_size_regressions.md#Debugging-Apk-Size-Increase
>
> Based on the graph: **20kb of native code, 8kb of pngs.**
>
> It's not clear to me whether or not this increase was expected.<br>
> Please have a look and either:
>
> 1. Close as "Won't Fix" with a short justification, or
> 2. Land a revert / fix-up.
>
> _**Optional addition for commits > 75kb:**_
>
> It typically takes about a week of engineering time to reduce binary size by
> 100kb so we'd really appreciate you taking some time exploring options to
> address this regression!

If the regression is >50kb, add ReleaseBlock-Stable **M-##** (next branch cut).*

# Debugging Apk Size Increase

## Step 1: Identify what Grew

Figure out which file within the `.apk` increased (native library, dex, pak
resources, etc.) by looking at the trybot results or size graphs that were
linked from the bug (if it was not linked in the bug, see above).

**See [//docs/speed/binary_size/metrics.md](https://chromium.googlesource.com/chromium/src/+/master/docs/speed/binary_size/metrics.md)
for a description of high-level binary size metrics.**

**See [//tools/binary_size/README.md](https://chromium.googlesource.com/chromium/src/+/master/tools/binary_size/README.md)
for a description of binary size tools.**

## Step 2: Analyze

### Growth is from Translations

 * There is likely nothing that can be done. Translations are expensive.
 * Close as `Won't Fix`.

### Growth is from Native Resources (pak files)

 * Ensure `compress="gzip"` is used for all `chrome:` pages.
 * Look at the SuperSize reports from the trybot to look for unexpected
   resources, or unreasonably large symbols.

### Growth is from Images

  * Would [a VectorDrawable](https://codereview.chromium.org/2857893003/) be better?
  * If it's lossy, consider [using webp](https://codereview.chromium.org/2615243002/).
  * Ensure you've optimized with
    [tools/resources/optimize-png-files.sh](https://cs.chromium.org/chromium/src/tools/resources/optimize-png-files.sh).
  * There is some [Googler-specific guidance](https://goto.google.com/clank/engineering/best-practices/adding-image-assets) as well.

### Growth is from Native Code

 * Look at the SuperSize reports from the trybot to look for unexpected symbols,
   or unreasonably large symbols.
 * If the diff looks reasonable, close as `Won't Fix`.
 * Otherwise, try to refactor a bit (e.g.
 [move code out of templates](https://bugs.chromium.org/p/chromium/issues/detail?id=716393)).
   * Use [//tools/binary_size/diagnose_bloat.py](https://chromium.googlesource.com/chromium/src/+/master/tools/binary_size/README.md)
     or the android-binary-size trybot to spot-check your local changes.
 * If symbols are larger than expected, use the `Disassemble()` feature of
   `supersize console` to see what is going on.

### Growth is from Java Code

 * Look at the SuperSize reports from the trybot to look for unexpected methods.
 * Ensure any new Java deps are as specific as possible.
 * If the change doesn't look suspect, check to see if the regression still
   exists when internal proguard is used (see
   [downstream graphs](https://chromeperf.appspot.com/report?sid=83bf643964a326648325f7eb6767d8adb85d67db8306dd94aa7476ed70d7dace)
   or use `diagnose_bloat.py -v --enable-chrome-android-internal REV`
   to build locally)

### Growth is from "other lib size" or "Unknown files size"

 * File a bug under [Tools > BinarySize](https://bugs.chromium.org/p/chromium/issues/list?q=component%3ATools%3EBinarySize)
   with a link to your commit.

### You Would Like Assistance

 * Feel free to email [binary-size@chromium.org](https://groups.google.com/a/chromium.org/forum/#!forum/binary-size).

# For Binary Size Sheriffs

## Step 1: Check Work Queue Daily

 * Bugs requiring sheriffs to take a look at are labeled `Performance-Sheriff`
   and `Performance-Size` [here](https://bugs.chromium.org/p/chromium/issues/list?q=label:Performance-Sheriff%20label:Performance-Size&sort=-modified).
 * After resolving the bug by finding an owner or debugging or commenting,
   remove the `Performance-Sheriff` label.

## Step 2: Check Alerts Regularly

 * **IMPORTANT: Check the [perf bot page](https://ci.chromium.org/buildbot/chromium.perf/Android%20Builder%20Perf/)
 several times a day to make sure it isn't broken (and ping/file a bug if it is).**
   * At the very least you need to check this once in the morning and once in
   the afternoon.
   * If you don't and the builder is broken either you or the next sheriff will
   have to manually build and diff the broken range (via. `diagnose_bloat.py`)
   to see if we missed any regressions.
   * This is necessary even if the next passing build doesn't create an alert
   because the range could contain a large regression with multiple offsetting
   decreases.
 * Check [alert page](https://chromeperf.appspot.com/alerts?sheriff=Binary%20Size%20Sheriff) regularly for new alerts.
 * Join [g/chrome-binary-size-alerts](https://goto.google.com/chrome-binary-size-alerts).
 * Deal with alerts as outlined above.