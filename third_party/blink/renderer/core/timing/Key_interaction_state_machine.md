# Event Timing - Key Interaction State Machine
Patricija Cerkaite, May 29 2023

[TOC]

## Background
A keyboard interaction is a group of event handlers that fire when pressing a key on a keyboard. For example, A single "key pressed" interaction should include set order of events, such as `keydown`, `input`, and `keyup`. [EventTiming](https://w3c.github.io/event-timing/) group up certain events as interactions by assigning the same & non-trivial [interactionId](https://www.w3.org/TR/2022/WD-event-timing-20220524/#dom-performanceeventtiming-interactionid) following a state machine logic located in [`responsiveness_metrics.cc -> ResponsivenessMetrics::SetKeyIdAndRecordLatency()`](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/responsiveness_metrics.cc#327). This doc visualizes this state machine to help people understand its logic.
- - - -

## Diagram

![state diagram](/docs/images/Key_interaction_state_machine_diagram.svg)

*** note
### Note:
* Each [key_code](https://w3c.github.io/uievents/#keys-codevalues) has its own state machine. This means that multiple keyboard interactions can occur simultaneously hence, you should expect multiple state machines running concurrently. Additionally, each key_code allows to identify the physical key associated with the keyboard event.
***

- - - -


## States

### `[1]` No entry
The initial state. Either no entry of the key_code has been seen or previous ones have been cancelled.
`key_code_entry_map_` does not contain any entry with a key equal to key_code.

### `[2]` Have keydown entry
An intermediate state. In this state, we have seen the `keydown` entry for the current interaction, and are waiting for the matching `keyup` entry.
`key_code_entry_map_` currently contains the `keydown` entry of the interaction that this state machine represent. In this state, `keydown` entry waiting for a **matching** `keyup` entry to finish the current interaction.

### `[3]` Composition Event
The state indicates that `keydown` has initiated the composition. Since the composition events are not part of the keyboard interactions this intermediate state holds until the interactionId is produced.

### `[4]` Interaction finished
This is the end of an interaction lifecycle. The `keydown` entry was paired with the corresponding `keyup` entry and the key_code from the `key_code_entry_map_` was errased.

- - - -

## Transitions

### `[5]` keydown

Save the `keydown` key_code value to the `key_code_entry_map_`.

### `[6]` keydown

If key_code value is not equal to 229, then generate a new interactionId for the [`|previous_entry|`](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/blink/renderer/core/timing/responsiveness_metrics.cc;l=329;drc=2425fac374aaa944c34b2340b8f53c9c7fc49533#:~:text=if%20(key_code_entry_map_.Contains(*key_code))%20%7B). This transition could be triggered by holding a key down for an extended period.

### `[7]` compositionstart
The keydown event initiates the composition session. The `isComposition` parameter is set to true.

### `[8]` input
The input event within the composition finishes the interaction and produces the interactionId.

### `[9]` [keyup key_code = keydown key_code] keyup

The transition occurs if the keyup event is fired and there is a matching key_code of a keydown event. In this transition the following steps are executed:
   1. Generate a new interactionId for the keydown-keyup pair (`keydown` and `keyup`). 
   2. Delete the key_code of the pair from the `key_code_entry_map_`.

### `[10]` MaybeFlushKeyboardEntries
[MaybeFlushKeyboardEntries](https://source.chromium.org/chromium/chromium/src/+/refs/heads/main:third_party/blink/renderer/core/timing/window_performance.cc;l=677;drc=66941d1f0cfe9155b400aef887fe39a403c1f518;bpv=1;bpt=1) free up `keydown` entries stuck in `key_code_entry_map_` upon the first event timing entry after 500ms. `Keydown` gets stuck in the map for reasons like [crbug/1428603](https://bugs.chromium.org/p/chromium/issues/detail?id=1428603). This flush mechanism has a known [issue](https://bugs.chromium.org/p/chromium/issues/detail?id=1420716).
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
have_key_down : Have keydown entry [2]
have_composition : Composition Event [3]
interaction_finished: Interaction finished [4]

   [*] --> no_entry
   no_entry --> have_key_down : keydown [5]
   have_key_down --> have_key_down : keydown [6]
   no_entry --> have_composition: compositionstart [7]
   have_composition --> interaction_finished : input [8]
   have_key_down --> interaction_finished : [keyup key_code = keydown key_code] keyup [9]
   have_key_down --> interaction_finished : MaybeFlushKeyboardEntries [10]
   interaction_finished --> [*]

```