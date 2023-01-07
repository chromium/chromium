# Event Timing - Pointer Interaction State Machine
Aoyuan Zuo, August 09 2022

[TOC]

## Background
A pointer interaction is a group of event handlers that fire during the same logical user gesture from a pointer device such as mouse, pen, or finger. For example, A single "tap" interaction on a touchscreen device should include multiple events, such as `pointerdown`, `pointerup`, and `click`. [EventTiming](https://w3c.github.io/event-timing/) group up certain events as interactions by assigning the same & non-trivial [interactionId](https://www.w3.org/TR/2022/WD-event-timing-20220524/#dom-performanceeventtiming-interactionid) following a state machine logic located in [`responsiveness_metrics.cc -> ResponsivenessMetrics::SetPointerIdAndRecordLatency()`](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/responsiveness_metrics.cc#216). This doc visualizes this state machine to help people understand its logic.

## Diagram

![state diagram](/docs/images/Pointer_interaction_state_machine_diagram.png)

*** note
### Note:
* Each [pointer_id](https://www.w3.org/TR/pointerevents3/#dom-pointerevent-pointerid) has its own state machine. That means, when there are multiple pointer interactions happening at the same time, you should expect multiple state machines running at the same time, and each pointer_id maps to a unique state machine. Most of the time state machines won't interfere with each other, except for the transition [click/flush[11]](#click_flush) -- when any state machine triggers a flush(either by `pointerdown`, or by 1 sec timer times out from `pointerup`), it will flush for all exiting state machines(i.e. any state machine that is currently at [Waiting click[3]](#waiting-click) state, transition [click/flush[11]](#click_flush) will be triggered and move to [Interaction finished[4]](#interaction-finished) state).
* `Click`'s pointer_id can be inaccurate. Whenever we receive a `click` event, we'll use the pointer_id of the most recent `pointerdown` or `pointerup` event we've seen among all state machines as if current `click` entry's pointer_id.
* All pointer events except `pointerdown` are always dispatched at the time we receive it for event timing, which will not be explicitly mentioned in this doc. Only `pointerdown` events are delayed for dispatching event timing. We do so since `pointerdown`'s interactionId cannot be determined at the time we receive it. Thus, dispatching `pointerdown` has been specially mentioned in each transition in the [transitions section](#transitions) below.
***

*** promo
## Terminologies:
* **Flush the map**:

  We flush all the entries in `pointer_id_entry_map_` that are waiting for `click`(at [state[3]](#waiting-click)) to finish up current interaction. We do so since either we have waited long enough(1 sec) or we know we won't be able to see any from now on(this is the case when `last_pointer_id_` has been overwritten by a new `pointerdown` event).

  What it actually does is, for all entries in `pointer_id_entry_map_`:
    * If it's an `pointerup` entry, record tap or click UKM. Then erase from the map.
    * If it's an `pointerdown` entry with timestamp length > 1(i.e. We've seen both `pointerdown` & `pointerup` in its interaction), dispatch `pointerdown` for event timing and record tap or click UKM. Then erase from the map.
    * Others, that is a `pointerdown` entry with timestamp length == 1(i.e. We've only seen `pointerdown` in its interaction), keep it in the map and do nothing, as it's still waiting for more events(either `pointerup` or `click` or both) to show up to finish their interactions.

* **Flush timer**:

  `pointer_flush_timer_` is a 1 second unique timer shared between all state machines. When it times out, it'll trigger a flush for `pointer_id_entry_map_`.
***

- - - -

## States

### `[1]` No entry
The initial state. Either no entry of the pointer_id has been seen or previous ones have been cancelled.
`pointer_id_entry_map_` does not contain any entry with key equal to pointer_id.

### `[2]` Have pointerdown entry
An intermediate state. In this state, we have seen the `pointerdown` entry for the current interaction, and are waiting for more events(`pointerup` or `click` or both) from the same interaction to show up.
`pointer_id_entry_map_` currently contains the `pointerdown` entry of the interaction that this state machine represent.

### `[3]` Waiting click
An intermediate state. In this state, we have seen either the `pointerdown` entry, or `pointerup` entry, or both for the current interaction, and are solely waiting for `click` to finish current interaction, which may or may not show up.

**Note** `pointer_id_entry_map_` could contain different entries in this state depending on how you transited into this state:
* if transited from [No entry](#no-entry) state through [pointerup[8]](#explanation-pointerup-2), then the map should contain a `pointerup` entry.
* if transited from [Have pointerdown entry](#have-pointerdown-entry) state through [pointerup[7]](#explanation-pointerup-1), then the map should contain a `pointerdown` entry with its timestamp length > 1.

### `[4]` Interaction finished
This is the end of an interaction lifecycle.

- - - -

## Transitions

### `[5]` pointerdown
*** aside
Flush pointer map and stop any ongoing flush timer from any state machines.

Save the `pointerdown` entry to the map and update `last_pointer_id_` with current `pointerdown` entry's pointer_id for potential future click entry.
***

### `[6]` pointercancel
This can happen when dragging an element on the page. Since dragging is a continuous interaction which will be covered separately by smoothness metrics, the `pointerdown`'s interactionId & the `pointercancel`'s will remain 0.
*** aside
Dispatch the `pointerdown` entry saved in `pointer_id_entry_map_` for event timing and erase it from the map.

Clear `last_pointer_id_`.
***

### `[7]` pointerup
*** aside
Generate a new interaction id and assign it to both current `pointerup` entry and the saved `pointerdown` entry in map.

Dispatch the `pointerdown` entry saved in `pointer_id_entry_map_` for event timing.

Add `pointerup`'s timestamp to the saved `pointerdown` entry in map.

Start 1 sec flush timer if it's currently not active.

Update `last_pointer_id_` with current `pointerup` entry's pointer_id for potential future `click` entry.
***

### `[8]` pointerup
*** aside
Generate a new interaction id and assign it to current `pointerup` entry.

Save current `pointerup` entry to `pointer_id_entry_map_` in case a click event show up in future.

Start 1 sec flush timer if it's currently not active.

Update `last_pointer_id_` with current `pointerup` entry's pointer_id for potential future click entry.
***

### `[9]` pointercancel
*** aside
Dispatch the entry saved in `pointer_id_entry_map_` for event timing if it's `pointerdown`; no need to dispatch if it's `pointerup`.

Erase it from the map and clear `last_pointer_id_`.
***

### `[10]` click
In this case we've only seen `pointerdown` and `click`. This could happen for instances like contextmenu.
*** aside
Generate a new interaction id and assign it to both current `click` entry and the saved `pointerdown` entry in map.

Add `click`'s timestamp to the saved `pointerdown` entry and record click UKM.

Erase `pointerdown` entry from the map and clear `last_pointer_id_`.
***

### `[11]` click/flush
This transition can be triggered in two different scenarios:
* by a click event: In this case, we received a click event, and by treating `last_pointer_id_` as its pointer_id, we've found a matching entry in the map -- either a `pointerdown` entry with timestamp length > 1(i.e. we've seen both `pointerdown` & `pointerup`), or a `pointerup` entry in the map(i.e. we've only seen the `pointerup` entry).

  In this case, we do:
  *** aside
  Assign map entry(either a `pointerdown` or `pointerup`)'s interactionId to the current `click` entry.

  Add `click`'s timestamp to the map entry's timestamps and record click UKM.

  Erase the map entry from the map and clear `last_pointer_id_`.
  ***

* by a map flush that's triggered by:
  * the flush timer times out and the timer was initiated by the `pointerup` event of current interaction.
  * the flush timer times out and the timer was initiated by a `pointerup` event from other interactions.
  * a `pointerdown` event from other interactions.

  In this case, we do:
  *** aside
  Dispatch the saved `pointerdown` entry in map for event timing if it's an `pointerdown` entry with timestamp length > 1(i.e. We've seen both `pointerdown` & `pointerup` in this interaction).

  Record tap or click UKM.

  Erase the entry from the map.
  ***

### `[12]` click
In this case, there is no previous pointerdown or pointerup entry. This can happen when the user clicks using a non-pointer device.
*** aside
Generate a new interactionId. No need to add to the map since this is the last event in the interaction.

Record click UKM and clear `last_pointer_id_`.
***

- - - -

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

no_entry : No entry [1]
have_pointer_down : Have pointerdown entry [2]
have_pointer_up : Waiting click [3]
interaction_finished: Interaction finished [4]

   [*] --> no_entry
   no_entry --> have_pointer_down : pointerdown [5]
   have_pointer_down --> no_entry : pointercancel [6]
   have_pointer_down --> have_pointer_up : pointerup [7]
   no_entry --> have_pointer_up : pointerup [8]
   have_pointer_up --> no_entry : pointercancel [9]
   have_pointer_down --> interaction_finished : click [10]
   have_pointer_up --> interaction_finished : click/flush [11]
   no_entry --> interaction_finished : click [12]
   interaction_finished --> [*]

```
