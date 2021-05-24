# Achievements

This is a non-exhaustive list of results that various chrome people and teams have achieved. It is likely that some big changes are unintentionally missing here, in which case please directly add them without asking.

Note, this is a community curated list. No one is sitting around trying to tabulate so if you did something cool, it's your job to add it. :)

  * [Reduced >50 MB](https://groups.google.com/a/chromium.org/forum/#!topic/memory-dev/u4TJOd8FXao) from many System Health benchmarks by dropping CC's image caches on a page navigation
  * [Shipped Purge+Suspend](https://codereview.chromium.org/2668663002/) to background tabs of Android Chrome. The purging reduces 17 MB per background tab at 50%-tile and 43 MB at 75%-tile.
  * [Reduced >100 MB](https://bugs.chromium.org/p/chromium/issues/detail?id=669348) in some scenarios from bitmaps for ResourceManager's UI.
  * ["More than 900KiB" bucket population dropped from 50% to 7%](https://bugs.chromium.org/p/chromium/issues/detail?id=641008) in BrotliFilter.
  * [Reduced 1.5 MB](https://bugs.chromium.org/p/chromium/issues/detail?id=652456) in some scenarios by lazily allocating SSLClientSocketImpl buffers.
  * [Reduced 6 - 12 MB](https://bugs.chromium.org/p/chromium/issues/detail?id=662019) from large LevelDB databases in some scenarios.
  * [Reduced 1 - 5 MB](https://docs.google.com/document/d/1k-vivOinomDXnScw8Ew5zpsYCXiYqj76OCOYZSvHkaU/edit) from long-running renderers by implementing a heap compaction on Oilpan's GC.
  * [Reduced 35MB](https://chromeperf.appspot.com/report?sid=149453e8cd5a25621f8fbfc0e4fd4488016f4c5481dc20ff388a88e465a573bd&start_rev=405220&end_rev=413071) of average (50MB peak) memory consumption of v8 for NYTimes. See further [improvements](https://chromeperf.appspot.com/report?sid=7184bf6318b1731a2a3c0aaceb8ceb0d6315c5ad3add8fe503dd6322cb9dc805&start_rev=412061&end_rev=417931) on mobile browsing benchmarks (~25% reduced max v8 heap memory and ~50% reduced max v8 zone memory)
  * [Reduced 15 MB from Tumblr and many image-heavy websites](https://groups.google.com/a/chromium.org/forum/#!topic/project-trim/iLmZoFxall4/discussion) by dropping duplicated encoded images
  * [Reduced an average of 17.2 MB on Nexus 9 and 5.8 MB](https://docs.google.com/document/d/1bKqev1DDb5siDabTnV1oDUdXES75aHMVdG4GtbPE9jU/edit#heading=h.41gx3tjq7iut) on Android one using one-copy tile updates.
  * [Reduced 9MB](https://chromeperf.appspot.com/report?sid=de0b38c645b4724a67133fc4d37d41134f583ca3c16c9deb3546d9fdafa8445d) on N5x of V8 zone peak memory usage.
  * [Reduced 8 MB from Facebook](https://codereview.chromium.org/1808633002) by fixing a color format on low-RAM devices
  * [Reduced 7.4 MB](https://bugs.chromium.org/p/chromium/issues/detail?id=596881#c17) from vimeo.com and ebay.com Nexus 5 using GPU raster.
  * [Reduced 5 MB from the browser process](https://codereview.chromium.org/1953703004) by purging DOMStorage's cache
  * [Reduced 3MB](https://chromeperf.appspot.com/report?sid=033d52b8e590bed23d15466eaec8b25809245dafa98ed6a34f169a5b40988daf&start_rev=402092&end_rev=402943) on low-end Android devices with the V8 interpreter
  * [Reduced 10 MB](https://bugs.chromium.org/p/chromium/issues/detail?id=482727) in graphics memory inside browser on Svelte by turning off hardware acceleration
  * [Reduced 1.3 MB](https://codereview.chromium.org/1156003008) from browser private dirty by decoding images from ImageManager cache on demand
  * [Reduced 2-10MB](https://codereview.chromium.org/1377483003) from browser process by shrinking IPC buffers after large messages were passed
  * [Reduced < 2 MB](https://chromium-review.googlesource.com/c/chromium/src/+/945748) from background tabs of both Chrome and WebView on Android Go device. See further [information](https://bugs.chromium.org/p/chromium/issues/detail?id=833769) on the internal bots for Android.
  * Developed [Memory-Infra](https://chromium.googlesource.com/chromium/src/+/main/components/tracing/docs/memory_infra.md), a timeline-based memory profiling system integrated into chrome:://tracing
  * Developed [System health benchmarks](https://docs.google.com/document/d/1BM_6lBrPzpMNMtcyi2NFKGIzmzIQ1oH3OlNG27kDGNU/edit?ts=57e92782), a set of benchmarks that give us consistent metrics for our reduction efforts
  * [Visualized Chrome's memory consumption in real-world website](https://docs.google.com/document/d/1JfnW6RpRDuuZITQ3xuFUIRBfC_KOG5xXUuW8U_UePJU/edit)s
