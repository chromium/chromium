---
name: ios/guard-high-frequency-state-updates-with-early-return
description: In high-frequency callbacks like scroll or fullscreen progress updates, return early if transient UI overlays are uninvoked, inactive, or already match target state
rubric: design
severity: should_fix
---

**Rule.** In observer callbacks or event handlers triggered at high frequency
(such as scroll observers like `-scrollViewDidScroll:`, animation ticks, or
fullscreen callbacks like
`FullscreenControllerObserver::FullscreenProgressUpdated` and
`FullscreenUIElement` methods), verify whether an update is necessary and return
early if:

1. The target component or transient overlay is inactive, uninvoked, or
   temporarily hidden before modifying layout properties, geometry offsets, or
   opacity.
2. The UI visibility or internal state already matches the target state (such as
   already visible or hidden), avoiding redundant state mutations, view layout
   passes, or provider bridge dispatches.

**Why.** High-frequency observers fire continuously during user scrolling and
frame animation ticks. Unconditionally updating state flags, recalculating
geometry offsets, modifying opacity, or dispatching provider bridge calls when
an overlay is inactive or when state has not changed causes continuous CPU
thrashing, unnecessary layout passes, visual glitches (such as flashing
uninvoked views or applying stale geometry), and frame-rate drops.

## Examples

Bad:

```cpp
// Variant 1: Unconditional geometry update on inactive overlay in progress callback.
void GeminiBrowserAgent::FullscreenProgressUpdated(
    FullscreenController* controller, CGFloat progress) {
  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(), progress);
}

// Variant 2: Missing check if already in target visibility state during scroll.
void GeminiBrowserAgent::ShowFloatyIfInvoked(bool animated) {
  if (!is_floaty_invoked_) {
    return;
  }
  is_floaty_temporarily_hidden_ = false;
  ios::provider::UpdateGeminiViewState(last_shown_view_state_, animated);
}
```

Good:

```cpp
// Variant 1: Guard progress update by checking if invoked and not hidden.
void GeminiBrowserAgent::FullscreenProgressUpdated(
    FullscreenController* controller, CGFloat progress) {
  if (!is_floaty_invoked_ || is_floaty_temporarily_hidden_) {
    return;
  }
  ios::provider::UpdateOverlayOffsetWithOpacity(GetFloatyOffset(), progress);
}

// Variant 2: Guard visibility update by checking if already shown.
void GeminiBrowserAgent::ShowFloatyIfInvoked(bool animated) {
  if (!is_floaty_invoked_ || !is_floaty_temporarily_hidden_) {
    return;
  }
  is_floaty_temporarily_hidden_ = false;
  ios::provider::UpdateGeminiViewState(last_shown_view_state_, animated);
}
```
