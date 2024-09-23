# Android Scroll Jank Metric.

Chromium uses a scroll jank metric on Android that tracks the percentage of dropped frames in a given window of 64 frames, and uses this metric to benchmark the end user's scrolling experience.

## How it works

Chromium assumes that if scrolling input events arrive in sequence during a scroll, then frames should be produced in sequence, without any drops or delays.

![Chromium Dropped Frame](chromium_dropped_frame.png)

# How is it decided that scrolling events arrived in sequence

If the time difference between two consecutive scrolling input events (i.e., the time between two consecutive screen touches during a scroll gesture) is less than the device's refresh rate interval (vsync interval), then it is expected that these two input events will generate two distinct frames, unless those inputs are coalesced for arriving too close within a single Vsync interval (input sampling rate higher than refresh rate).

```
scrolling_input_event_timestamp[i] - scrolling_input_event_timestamp[i - 1] < vsync_interval
```

Scrolling Input Events: These are moments when the user interacts with the touchscreen to initiate or continue a scrolling action.

Timestamp: Each scrolling input event is recorded with a timestamp indicating the exact time it occurred.

Vsync Interval: This is the time it takes for the device's screen to refresh its content. It's directly related to the screen's refresh rate (e.g., a 60Hz refresh rate has a vsync interval of approximately 16.67 milliseconds).

# Where to find the metric

The metric is emitted using UMA and there are multiple variants of it explained below:
* ```Event.ScrollJank.DelayedFramesPercentage.FixedWindow``` for the percentage of delayed/dropped frames every 64 frame window.
* ```Event.ScrollJank.MissedVsyncsSum.FixedWindow``` for the sum of missed vsyncs (presentation opportunities) every 64 frame window.
* ```Event.ScrollJank.MissedVsyncsMax.FixedWindow``` for the maxiumum continuous interval of frame drops in a 64 frame window

for more information, please check https://doc/1Y0u0Tq5eUZff75nYUzQVw6JxmbZAW9m64pJidmnGWsY