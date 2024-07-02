# INP, LCP, and FCP Changes in Chrome 126

## Launch NewPresentationFeedbackTimeStamps to 100% Stable on Mac

The GPU [PresentationFeedback](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/presentation_feedback.h)
parameters contain an estimated [timestamp](https://source.chromium.org/chromium/chromium/src/+/main:ui/gfx/presentation_feedback.h;l=57?q=%22base::TimeTicks%20timestamp;%22&ss=chromium%2Fchromium%2Fsrc)
for the time when the frame will be presented on a screen.
The old PresentationFeedback records the time when the CALayerTree of the
frame is populated to CoreAnimation instead of the actual time when the image
content of the CALayerTree is displayed on a screen. This feature improves the
metrics accuracy by using the image content display time for
PresentationFeedback. As a result, the time for producing a frame on a screen
increases.

## How does this affect a site's metrics?

On average, the feature NewPresentationFeedbackTimeStamps increases the latency
of INP, LCP and FCP by ~20 ms. The actual numbers can change according to the
hardware screen VSync frame interval.

## When were users affected?

This feature was launched to 100% Stable on Mac on June 17, 2024 for Chrome 124
and beyond during the release period of Chrome 126. Chrome 126 was released the
week of June 11.

Note: The [source code change](https://chromiumdash.appspot.com/commit/39dd592ae4ca58ea98263802a177c88c86f0d519)
for enabling this feature was merged into the M128 branch.

