# Event Timing - Key Interaction State Machine

Patricija Cerkaite, created on May 29 2023

Aoyuan Zuo, updated on Sept. 09 2024

[TOC]

## Background
A keyboard interaction is a group of event handlers that fire when pressing a key on a keyboard. For example, A single "key pressed" interaction should include set order of events, such as `keydown`, `keypress`, `input`, and `keyup`. [EventTiming](https://w3c.github.io/event-timing/) group up certain events as interactions by assigning the same & non-trivial [interactionId](https://www.w3.org/TR/2022/WD-event-timing-20220524/#dom-performanceeventtiming-interactionid) following a state machine logic located in [`responsiveness_metrics.cc -> ResponsivenessMetrics::SetKeyIdAndRecordLatency()`](https://chromium.googlesource.com/chromium/src/+/main/third_party/blink/renderer/core/timing/responsiveness_metrics.cc#327). This doc visualizes this state machine to help people understand its logic.
- - - -

## Diagram

![state diagram](/docs/images/Key_interaction_state_machine_diagram.png)

*** note
### Note:
* Each [key_code](https://w3c.github.io/uievents/#keys-codevalues) has its own state machine. This means that multiple keyboard interactions can occur simultaneously hence, you should expect multiple state machines running concurrently. Additionally, each key_code allows to identify the physical key associated with the keyboard event.
***

- - - -


## States

### `[1]` Non Composition
The initial state. No entry of the key_code has been seen. `key_code_to_interaction_info_map_` does not contain any entry with a key equal to key_code.

### `[2]` Have Keydown
An intermediate state. In this state, we have seen the `keydown` entry for the current interaction, and are waiting for potentially `keypress` or `contextmenu` events, and a matching `keyup` entry.
`key_code_to_interaction_info_map_` and `sequence_based_keyboard_interaction_info_` currently contains the `keydown` entry.

### `[3]` Have Keypress
An intermediate state. In this state, we have seen the `keydown` and `keypress` entries for the current interaction, and are waiting for the matching `keyup` entry.
`key_code_to_interaction_info_map_` currently contains the timestamps of both `keydown` and `keypress` entries of the interaction that this state machine represent. In this state, we are waiting for the **matching** `keyup` entry to finish the current interaction.

### `[4]` Have Contextmenu
When pressing the menu key on a keyboard, a `contextmenu` event would get dispatched right after the `keydown` event. This is a valid user interaction, but the `keyup` event could possibly be dropped due to the showing of contextmenu overlay. Thus, while we waiting for the potentially coming `keyup` event, we also setup a 1 second timer to wrap up this interaction in case `keyup` is not coming.

### `[5]` Composition Continue Ongoing Interaction
The state indicates that `compositionstart` has initiated the composition. Since the composition events (`compositionstart`, `compositionupdate`, `compositionend`) are not part of the keyboard interactions this intermediate state holds until the interactionId is produced.

### `[6]` Composition Start New Interaction On Keydown
This state means we're currently under composition, and we're ready to start recording a new interaction upon next `keydown`.

### `[7]` Composition Start New Interaction On Input
This state means we're currently under composition, and we have not seen `keydown` thus no interactionId has been generated yet for the current interaction. As a result, we will generate one on `input`.

### `[8]` End Composition On Keydown
We have get out of the composition session, but are still waiting for potentially coming `keyup`s to wrap up the last interaction in composition.

### `[9]` Interaction finished
This is the end of an interaction lifecycle. The `keydown` entry was paired with the corresponding `keyup` entry and the key_code from the `key_code_to_interaction_info_map_` was erased.

- - - -

## Transitions

### `[10]` Keydown
Generate a new interactionId. Assign it to the `keydown` entry. Save the `keydown`'s key_code, interactionId, timestamps into `key_code_to_interaction_info_map_` for UKM reporting later once the interaction is fully finished..

### `[11]` Key Holding
When holding a key down for an extended period. A serious of `keydown` events with the same key_code will be dispatched. We treat each individual `keydown` event as a valid interaction and assign a unique interactionId. Only the last one will be matched with `keyup`.

### `[12]` Keypress
A `keypress` event following a `keydown` event belongs to the same interaction as the `keydown`. We assign them the same interactionId and also save `keypress`'s timestamp into `key_code_to_interaction_info_map_` for UKM reporting later.

### `[13]` Keyup
A `keyup` event always finish up an interaction by assigning the same interactionId as `keydown`'s and report interaction duration calculated by all events belongs to the interaction to UKM. The `key_code` will be removed from `key_code_to_interaction_info_map_` after reporting, and `sequence_based_keyboard_interaction_info_` will be cleared.

### `[14]` Contextmenu
When pressing the menu key on a keyboard, a `contextmenu` event would get dispatched right after the `keydown` event. This is a valid user interaction, but the `keyup` event could possibly be dropped due to the showing of contextmenu overlay. Thus, while we waiting for the potentially coming `keyup` event, we also setup a 1 second timer to wrap up this interaction in case `keyup` is not coming.

### `[15]` Keyup (after contextmenu)
A `keyup` after contextmenu stops the timer and wraps up the interaction immediately, which means getting the same interactionId assigned; reporting the interaction to UKM; erasing the interaction info from `key_code_to_interaction_info_map_` and clears `sequence_based_keyboard_interaction_info_`.

### `[16]` Compositionstart
The `compositionstart` event following a `keydown` event initiates the composition session.

### `[17]` Keydown (under composition)
Some IME (Input Method Editor) could repeatedly dispatch keydowns for the same user interaction. When it happens, we will assign the same interactionId to all of them.

### `[18]` Compositionupdate
`Compositionupdate` is the center of a keyboard user interaction under composition. Once we see it, we'll start wrapping up this interaction with matching the interactionId to the rest of events including `Input` and `keyup`.

### `[19]` Compositionstart
This is an edge case that exists for some IMEs where `keydown` events are missing. `Compositionstart` would initiate the composition session without a `keydown`.

### `[20]` Compositionupdate (keydown missing)
This is an edge case that exists for some IMEs where `keydown` events are missing. Since interactionIds are usually generated with `keydown` events, we'll fallback to generate a new interactionId on `input` to make sure we can still capture and report this interaction.

### `[21]` Input (keydown missing)
A new interactionId is generated since one wasn't generated before due to the missing `keydown`.

### `[22]` Input
Assign the current interactionId (previously generated from `keydown`) to the `input` event.

### `[23]` Keyup
Assign the current interactionId (previously generated from `keydown`) to the `keyup` event.

### `[24]` Keydown (next interaction)
Under composition session, we interpret a new keydown as the start of a new discrete user interaction on a keyboard, which also marks the end of the current interaction.

### `[25]` Compositionend
`Compositionend` marks the end of an user interaction under composition through a one second timer, in case when more than one `keyup` events are dispatched we can still group them up into the same user interaction, which could happen to some IMEs as an edge case.

### `[26]` Keyup
A `keyup` from the same user interaction could come after `compositionend`, and it will get the same interactionId assigned.

### `[27]` Keydown (next interaction) / timer
Either after 1 second by a timer, or the next `keydown` event coming in before the timer times up, we'll wrap up the current interaction and report to UKM.
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

non_composition : Non Composition [1]
have_keydown : Have keydown [2]
have_keypress : Have Keypress [3]
have_contextmenu : Have Contextmenu [4]
composition_continue_ongoing_interaction : Composition Continue Ongoing Interaction [5]
composition_start_new_interaction_on_keydown : Composition Start New Interaction On Keydown [6]
composition_start_new_interaction_on_input : Composition Start New Interaction On Input [7]
end_composition_on_keydown : End Composition On Keydown [8]
interaction_finished: Interaction finished [9]

   [*] --> non_composition
   non_composition --> have_keydown : Keydown [10]
   have_keydown --> have_keydown : Key Holding [11]
   have_keydown --> have_keypress : Keypress [12]
   have_keypress --> interaction_finished : Keyup [13]
   have_keydown --> have_contextmenu : Contextmenu [14]
   have_contextmenu --> interaction_finished : Keyup [15]
   have_keydown --> composition_continue_ongoing_interaction : Compositionstart [16]
   composition_continue_ongoing_interaction --> composition_continue_ongoing_interaction : Keydown [17]
   composition_continue_ongoing_interaction --> composition_start_new_interaction_on_keydown : Compositionupdate [18]
   non_composition --> composition_continue_ongoing_interaction : Compositionstart [19]
   composition_continue_ongoing_interaction --> composition_start_new_interaction_on_input : Compositionupdate (keydown missing) [20]
   composition_start_new_interaction_on_input --> composition_start_new_interaction_on_keydown : Input [21]
   composition_start_new_interaction_on_keydown --> composition_start_new_interaction_on_keydown : Input [22]
   composition_start_new_interaction_on_keydown --> composition_start_new_interaction_on_keydown : Keyup [23]
   composition_start_new_interaction_on_keydown --> interaction_finished : Keydown (next interaction) [24]
   composition_start_new_interaction_on_keydown --> end_composition_on_keydown : Compositionend [25]
   end_composition_on_keydown --> end_composition_on_keydown : Keyup [26]
   end_composition_on_keydown --> interaction_finished : Keydown (next interaction) / timer [27]
   interaction_finished --> [*]

```