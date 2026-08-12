# Interaction to Next Paint Changelog

This is a list of changes to [Interaction to Next Paint](https://web.dev/inp).

* Chrome 150
  * Metric bug fix: Nested clicks (such as `<label>` clicks forwarding to form controls) assigned `interactionId = 0` to align with web standards ([7870025](https://chromium-review.googlesource.com/c/chromium/src/+/7870025)).
* Chrome 148
  * Metric bug fix: [Report only web-exposed event targets in Event Timing](2026_03_inp.md)
* Chrome 147
  * Metric definition improvement: [Early interaction ID assignment and context menu fallback](2026_02_inp.md)
* Chrome 144
  * Launch feature: [Launch performance.interactionCount API to stable](2025_10_inp.md)
* Chrome 135
  * Metric definition improvement: Updated `interactionId` type to `unsigned long long` (`uint64_t`) per W3C specification ([6300797](https://chromium-review.googlesource.com/c/chromium/src/+/6300797)).
* Chrome 134
  * Implementation optimizations: [Defer non-urgent renderer tasks after input by default](2025_01_inp.md)

* Chrome 133
  * Launch feature: [Enable EventTimingSelectionAutoScrollNoInteractionId by default](2025_02_inp.md)
* Chrome 130
  * Launch feature: [Enable EventTimingTapStopScrollNoInteractionId by default](2024_10_inp.md)
  * Launch feature: [Enable EventTimingHandleKeyboardEventSimulatedClick by default](2024_10_inp.md)
  * Launch feature: [Enable ReportEventTimingAtVisibilityChange by default](2024_10_inp.md)
* Chrome 129
  * Launch feature: [Enable ContinueEventTimingRecordingWhenBufferIsFull by default](2024_09_inp.md)
* Chrome 128
  * Metric bug fix: [Enable EventTimingHandleOrphanPointerup by default](2024_08_inp.md)
* Chrome 127
  * Launch feature: [Enable EventTimingKeypressAndCompositionInteractionId by default](2024_07_inp.md)
  * Launch feature: [Enable EventTimingFallbackToModalDialogStart by default](2024_07_inp.md)
* Chrome 126
  * Launch feature: [Enable NewPresentationFeedbackTimeStamps on Mac to improve the accuracy of the frame display time](2024_06_inp_lcp_fcp.md)
* Chrome 122
  * Launch feature: [Enable EventTimingMatchPresentationIndex by default](2024_02_inp.md)
* Chrome 121
  * Metric bug fix: [Event Timing flush pointerdown & keydown on contextmenu](2024_01_inp.md)
* Chrome 116
  * Metric bug fix: [Event Timing - Fallback artificial events ending time to processingEnd](2023_08_inp.md)
* Chrome 112
  * Metric bug fix: [Event Timing Pointer Map Flush Timer Bug Fixes](2023_04_inp.md)
* Chrome 111
  * Metric bug fix: [Event Timing API no longer reports very long durations when interaction leads to "open in new tab"](2023_03_inp.md)
* Chrome 109
  * Implementation optimizations: [A change in Chrome to prioritize compositing after input events caused a significant improvement to INP](2023_01_inp.md)
* Chrome 96
  * Experimental metric exposed via API: [Event Timing InteractionID](https://web.dev/inp/) available via [PerformanceObserver API](https://www.w3.org/TR/event-timing/)
