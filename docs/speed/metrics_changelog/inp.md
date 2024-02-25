# Interaction to Next Paint Changelog

This is a list of changes to [Interaction to Next Paint](https://web.dev/inp).

* Chrome 122
  * Launch feature: [Enable EventTimingMatchPresentationIndex by default](https://chromium.googlesource.com/chromium/src/+/50dfc1969242c9566d35763c96caddc5694299c9)
  * Note: this experiment was first landed in Chrome 114: [Event Timing Presentation Promise Handling Redesign](https://chromium.googlesource.com/chromium/src/+/ce150839f2930a7d59b3850ca8e7d02210101f08), but was rolled out slowly.
* Chrome 121
  * Metric bug fix: [Event Timing flush pointerdown & keydown on contextmenu](https://chromium.googlesource.com/chromium/src/+/7ed67e1b59cfe5cb1c4674dc7db59c2c5ae90cbd)
* Chrome 116
  * Metric bug fix: [Event Timing - Fallback artificial events ending time to processingEnd](https://chromium.googlesource.com/chromium/src/+/851b8bd317b3af1a678b72e48dbd791954e373ed)
* Chrome 112
  * Metric bug fix: [Event Timing Pointer Map Flush Timer Bug Fixes](https://chromium.googlesource.com/chromium/src/+/086f954a9d2daaeb4eec9077afcaf410c9c3fa24)
* Chrome 111
  * Metric bug fix: [Event Timing API no longer reports very long durations when interaction leads to "open in new tab"](2023_04_inp.md)
* Chrome 109
  * Implementation optimizations: [A change in Chrome to prioritize compositing after input events caused a significant improvement to INP](2023_01_inp.md)
* Chrome 96
  * Experimental metric exposed via API: [Event Timing InteractionID](https://web.dev/inp/) available via [PerformanceObserver API](https://www.w3.org/TR/event-timing/)
