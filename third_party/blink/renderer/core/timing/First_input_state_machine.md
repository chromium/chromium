# First Input State Machine
Aoyuan Zuo, July 11 2022

[TOC]

## Background
First input is a subset of [EventTimingAPI](https://w3c.github.io/event-timing/)
that provides timing information about the latency of the first discrete user
interaction. It takes in a pipeline of performance event entries and output a
single first input event entry following a state machine logic located in
[`window_performance.cc -> WindowPerformance::ReportEventTimings()`](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/window_performance.cc#515)
. This doc visualizes the state machine to help people understand its logic.

## Diagram

![state diagram](/docs/images/First_input_state_machine_diagram.png)

## Explanation

### `[1]` No pointer down entry
first_pointer_down_event_timing_ == null.

### `[2]` pointerdown
Save pointerdown entry into first_pointer_down_event_timing_.

### `[3]` Have pointer down entry
first_pointer_down_event_timing_ != null.

### `[4]` pointercancel
Clear first_pointer_down_event_timing_.

### `[5]` pointerdown
Update first_pointer_down_event_timing_ with new entry.

### `[6]` pointerup
Dispatch first_pointer_down_event_timing_ entry as first input timing.

### `[7]` mousedown/click/keydown
Dispatch current mousedown/click/keydown entry as first input timing.

### `[8]` First input dispatched
Once dispatched first input timing, we save it into first_input_timing_ and
start ignoring any following/incoming events.

## Mermaid diagram source file

We rely on gitiles to render out markdown files, however it does not support
rendering mermaid at the moment. Mermaid is the tool we use to generate the
state machine diagram. In order to make future maintenance easier, here I keep a
copy of the mermaid source file so that people can use it to regenerate the
diagram and make updates.

Note: When you update the state diagram, please keep the source file below up to
date as well.

```
stateDiagram-v2

no_pointer_down : No pointer down entry [1]
have_pointer_down : Have pointer down entry [3]
dispatched: First input dispatched [8]

   [*] --> no_pointer_down
   no_pointer_down --> have_pointer_down : pointerdown [2]
   have_pointer_down --> no_pointer_down : pointercancel [4]
   have_pointer_down --> have_pointer_down : pointerdown [5]
   have_pointer_down --> dispatched : pointerup [6]
   no_pointer_down --> dispatched : mousedown/click/keydown [7]
   dispatched --> [*]

```
