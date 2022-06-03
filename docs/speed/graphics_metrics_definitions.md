# Graphics metrics: Definitions

We need to have a metric to understand the smoothness of a particular
interaction (e.g. scroll, animation, etc.). We also need to understand the
latency of such interactions (e.g. touch-on-screen to
scroll-displayed-on-screen), and the throughput during the interaction.

[TOC]

We define these metrics as follows:

## Responsiveness / Latency

Responsiveness is a measure of how quickly the web-page responds to an event.
Latency is defined as the time between when an event happens, (e.g. moving a
touch-point on screen) and when the screen is updated directly in response to
that event [1]. For example, the event could be a moving touch-point on the
touchscreen, and the update would be scrolled content in response to that
(may only require the compositor frame update). If a rAF callback was
registered, the event would be the one that caused the current script execution
(e.g. a click event which triggered rAF), and the update would be the displayed
frame after the rAF callback is run and the content from the main-thread has
been presented on screen.

## Throughput

The ratio between the number of times the screen is updated for a particular
interaction (e.g. scroll, animation, etc.), and the number of times the screen
was expected to be updated (see examples below). On a 60Hz display, there would
ideally be 60 frames produced during a scroll or an animation.

## DroppedFrames / SkippedFrames

The ratio between the number of dropped/skipped frames for a particular
interaction, and the number of times the screen was expected to be updated. This
is the other part data of Throughput so it is a "lower-is-better" metric and
works better with current out perf tools.

## Smoothness / Jank

Smoothness is a measure of how consistent the throughput is. Jank during an
interaction is defined as a change in the throughput for consecutive frames.
To explain this further:

Consider the following presented frames:

**f1**&nbsp;&nbsp;
**f2**&nbsp;&nbsp;
**f3**&nbsp;&nbsp;
**f4**&nbsp;&nbsp;
**f5**&nbsp;&nbsp;
**f6**&nbsp;&nbsp;
**f7**&nbsp;&nbsp;
**f8**&nbsp;&nbsp;
**f9**

Each highlighted **fn** indicates a frame that contained response from the
renderer[2]. So in the above example, there were no janks, and throughput was
100%: i.e. all the presented frames included updated content.

Considering the following frames:

**f1**&nbsp;&nbsp;
**f2**&nbsp;&nbsp;
f3&nbsp;&nbsp;
**f4**&nbsp;&nbsp;
f5&nbsp;&nbsp;
**f6**&nbsp;&nbsp;
**f7**&nbsp;&nbsp;
**f8**&nbsp;&nbsp;
**f9**

In this case, frames `f3` and `f5` did not include any updates (either
display-compositor was unable to submit a new frame, or the frame submitted by
the display compositor did not include any updates from the renderer).
Therefore, the throughput during this interaction is 78%.

To explain the jank, during the first two frames `[f1, f2]`, the throughput is
100%. Because of the subsequently missed frame `f3`, the throughput changes for
`[f2, f4]` drops to 67%. The throughput for `[f4, f6]` is also 67%. For
subsequent frames, the throughput goes back up to 100%. Therefore, there was a
single jank.

Consider the following two sequences:

**f1**&nbsp;&nbsp;
**f2**&nbsp;&nbsp;
**f3**&nbsp;&nbsp;
**f4**&nbsp;&nbsp;
f5&nbsp;&nbsp;
f6&nbsp;&nbsp;
f7&nbsp;&nbsp;
f8&nbsp;&nbsp;
**f9**

**f1**&nbsp;&nbsp;
f2&nbsp;&nbsp;
**f3**&nbsp;&nbsp;
f4&nbsp;&nbsp;
**f5**&nbsp;&nbsp;
f6&nbsp;&nbsp;
**f7**&nbsp;&nbsp;
f8&nbsp;&nbsp;
**f9**

In both cases, throughput is 55%, since only 5 out of 9 frames are displayed.
In the first sequence, there is a jank (`[f1, f2][f2, f3][f3, f4]` has 100%
throughput, but `[f4, f9]` has a throughput of 33%). However, in the second
sequence, there are no janks, since `[f1, f3]` `[f3, f5]` `[f5, f7]` `[f7, f9]`
all have 67% throughput.

[1]: Indirect updates in response to an event, e.g. an update from a
setTimeout() callback from an event-handler would not be associated with that
event.

[2]: Note that the response could be either an update to the content, or a
notification that no update is expected for that frame. For example, for a 30fps
animation in this frame-sequence, only frames `f1` `f3` `f5` `f7` `f9` will have
actual updates from the animation, and frames `f2` `f4` `f6` `f8` should still
have notification from the client that no update is expected.

