# renderer/core/frame

The code in this directory implements the `Frame` concept in Blink, which is
the fundamental container for a web document and its associated properties.

[TOC]

## Throttling

Blink employs throttling mechanisms to reduce resource consumption for
out-of-viewport and background frames, improving performance and battery life
for the foreground content. There are two primary forms of throttling, both
triggered when a frame's viewport intersection becomes empty.

### Render Throttling

Render throttling aims to reduce rendering work for cross-origin iframes that
are outside of the viewport. When a cross-origin iframe is scrolled out of
view, Blink stops running requestAnimationFrame, style, layout, and paint for
that frame on every `BeginMainFrame`. Same-origin iframes are not subject to
this throttling. Script execution continues in a render-throttled frame. The
implementation details can be found in
`FrameView::UpdateRenderThrottlingStatus` and are further explained in the
[Render Throttling design doc](https://docs.google.com/document/d/1Dd4qi1b_iX-OCZpelvXxizjq6dDJ76XNtk37SZEoTYQ/edit?tab=t.0).

### Timer Throttling

Timer throttling is a mechanism that reduces the frequency of timers for
out-of-viewport frames. This affects `setTimeout` and `setInterval` timers,
limiting their execution to save resources. While related to a frame's
visibility status, the specifics of timer throttling are part of a broader set
of scheduling policies. You can find more information about timer throttling
and other scheduling behaviors in
[Task Scheduling in Blink](../../../platform/scheduler/TaskSchedulingInBlink.md#throttling).
The visibility update that triggers this is in
`FrameView::UpdateFrameVisibility`.
