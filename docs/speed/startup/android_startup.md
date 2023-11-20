# Android Start-up Performance

## Stages of Start-up

Every Android component (activities, broadcast receivers, etc) can be a
start-up entry point for Chrome. The main start-up scenarios we optimize for
are:

 * Launching from the home screen
 * Handling a View intent
 * Showing a Chrome Custom Tab

For a home screen launch, we consider start-up to be complete once the
Activity is rendered. For the latter two, we care both about:

* The first [Navigation Commit] in the app lifetime
* The initial [First Contentful Paint]

[Navigation Commit]: https://developer.chrome.com/docs/extensions/reference/webNavigation/#event-onCommitted
[First Contentful Paint]: https://web.dev/fcp/

## UMA

Here are some UMA metrics that measure start-up (UMA's UI has descriptions):

* `Startup.Android.Cold.TimeToFirstVisibleContent2`
* `Startup.Android.Cold.TimeToFirstNavigationCommit2.Tabbed`
* `Browser.PaintPreview.TabbedPlayer.TimeToFirstBitmap`
* `Startup.Android.Cold.TimeToFirstContentfulPaint.Tabbed`

This one can be useful for measuring dex optimization changes:

* `Startup.LoadTime.ProcessCreateToApplicationStart`

For Googlers, there's more background in [this doc].

[this doc]: https://docs.google.com/document/d/1ahGc_uIRk76znPGg3KopOteRLmLLo4-sfXdq4Kt4Jwk/edit#heading=h.zgb0nx9k2mr0

## ChromePerf

Here are some [chromeperf metrics] that measure start-up in a lab
environment:

* `ChromiumPerf/android-pixel4-perf/startup.mobile / messageloop_start_time`
* `ChromiumPerf/android-pixel4-perf/startup.mobile / navigation_commit_time`
* `ChromiumPerf/android-pixel4-perf/startup.mobile / first_contentful_paint_time`

[chromeperf metrics]: https://chromeperf.appspot.com/report?sid=06a1fe93dd4da84479b7ee8987ed6a7668c7cef3cdf2ba1d9e3234d31c773cf8

## Pinpoint

When using [Pinpoint] to test start-up changes, make sure to:

* Use bundles rather than APKs.
  * For 32-bit: `--browser=android-trichrome-bundle`
  * For 64-bit: `--browser=android-trichrome-chrome-google-64-32-bundle`
* Compile DEX: `--compile-apk=speed`

Some start-up changes can improve start-up for high-end devices but degrade
it for low-end ones (or vice versa). It is important to test both.

For small regressions (e.g. ~1%), it can help to add `--pageset-repeat 10`
or `--pageset-repeat 20` in order to increase the number of samples collected.
A single repeat produces 8 samples, then Pinpoint normally runs it 10 times
(total 80). With `--pageset-repeat=20` the total number of samples is 1600.

* Use `android-go-*` devices to test low-end.
* Use `android-pixel6-*` to test high-end. They set
  `is_high_end_android=true`.
* The full list of bots [is here](/docs/speed/perf_lab_platforms.md).

[Pinpoint]: https://pinpoint-dot-chromeperf.appspot.com/
