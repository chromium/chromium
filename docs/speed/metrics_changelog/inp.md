# Interaction to Next Paint Changelog

This is a list of changes to [Interaction to Next Paint](https://web.dev/inp).

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
