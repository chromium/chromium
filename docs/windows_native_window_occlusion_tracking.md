# Windows Native Window Occlusion Detection

## Background

Ui::aura has an
[API](https://source.chromium.org/chromium/chromium/src/+/main:ui/aura/window_occlusion_tracker.h)
to track which aura windows are occluded, i.e., covered by
one or more other windows. If a window is occluded, Chromium treats foreground
tabs as if they were background tabs; rendering stops, and js is throttled. On
ChromeOS, since all windows are aura windows, this is sufficient to determine
if a Chromium window is covered by other windows. On Windows, we need to
consider native app windows when determining if a Chromium window is occluded.
This is implemented in
[native_window_occlusion_tracker_win.cc](https://source.chromium.org/chromium/chromium/src/+/main:ui/aura/native_window_occlusion_tracker_win.cc).

## Implementation
When the core WindowOcclusionTracker decides to track a WindowTreeHost, it
calls EnableNativeWindowOcclusionTracking. On non-Windows platforms, this
does nothing. On Windows, it calls ::Enable on the singleton
NativeWindowOcclusionTrackerWin object, creating it first, if it hasn't already
been created.

When NativeWindowOcclusionTrackerWin starts tracking a WindowTreeHost, it adds
the HWND of the host's root window to a map of Windows HWNDs it is tracking,
and its corresponding aura Window. It also starts observing the window to know
when its visibility changes, or it is destroyed.

The main work of occlusion calculation is done by a helper class,
WindowOcclusionCalculator, which runs on a separate COM task runner, in order
to not block the UI thread. If the WindowOcclusionCalculator is tracking any
windows, it 
[registers](https://source.chromium.org/chromium/chromium/src/+/main:ui/aura/native_window_occlusion_tracker_win.cc?q=WindowOcclusionCalculator::RegisterEventHooks)
a set of
[event](https://docs.microsoft.com/en-us/windows/win32/winauto/event-constants)
hooks with Windows, in order to know when
the occlusion state might need to be recalculated. These events include window
move/resize, minimize/restore, foreground window changing, etc. Most of these
are global event hooks, so that we get notified of events for all Windows
windows. For windows that could possibly
occlude Chromium windows, (i.e., fully visible windows on the current virtual
desktop), we register for EVENT_OBJECT_LOCATIONCHANGE events for the window's
process. pids_for_location_change_hook_ keeps track of which pids are hooked,
and is
[used to remove the hook](https://source.chromium.org/chromium/chromium/src/+/main:ui/aura/native_window_occlusion_tracker_win.cc;drc=eeee643ae963e1d78c7457184f8af93f48bba9d3;l=443)
if the process no longer has any windows open.

When the event handler gets notified of an event, it usually kicks off new
occlusion calculation, which runs after a 16ms timer. It doesn't do a new
occlusion calculation if the timer is currently running. 16ms corresponds to the
interval between frames when displaying 60 frames per second(FPS). There's
no point in doing occlusion calculations more frequently than frames are
displayed. If the user is in the middle of moving a window around, occlusion
isn't calculated until the window stops moving, because moving a window is
essentially modal, and there's no point in recalculating occlusion over and
over again for each incremental move event.

To calculate occlusion, we first mark minimized Chromium windows as hidden, and
Chromium windows on a different virtual desktop as occluded.  We compute the
SKRegion for the virtual screen, which takes multiple monitor configurations
into account, and set the initial unoccluded_desktop_region_ to the screen
region. Then, we enumerate all the HWNDs, in z-order (topmost window first).
For each occluding window (visible, not transparent, etc), we save the current
unoccluded_desktop_region_, and subtract the window's window_rect from the
unoccluded_desktop_region_ . If the hwnd is not a root Chromium window, we
continue to the next hwnd. If it is a root Chromium window, then we have seen
all the windows above it, and know whether it is occluded or not. We determine
this by checking if subtracting its window_rect from the
unoccluded_desktop_region_ actually changed the unoccluded_desktop_region_. If
not, that means previous windows occluded the current window's window_rect, and
it is occluded, otherwise, not.
Once the occlusion state of all root Chromium windows has been determined, the
WindowOcclusionTracker posts a task to the ui thread to run a callback on the
NativeWindowOcclusionTrackerWin object. That callback is
[NativeWindowOcclusionTrackerWin::UpdateOcclusionState](https://source.chromium.org/chromium/chromium/src/+/main:ui/aura/native_window_occlusion_tracker_win.cc;l=226?q=NativeWindowOcclusionTrackerWin::UpdateOcclusionState)
, and is passed
root_window_hwnds_occlusion_state_, which is a map between root window HWNDs
and their calculated occlusion state.
NativeWindowOcclusionTrackerWin::UpdateOcclusionState iterates over those HWNDs,
finds the corresponding root window, and calls SetNativeWindowOcclusionState on
its WindowTreeHost, with the corresponding HWND's occlusion state from the map.
If the screen is locked, however, it sets the occlusion state to OCCLUDED.

## Miscellaneous

 * If a window is falsely determined to be occluded, the content area will be
white.
 * When the screen is locked, all Chromium windows are considered occluded.
 * Windows on other virtual desktops are considered occluded.
 * Transparent windows, cloaked windows, floating windows, non-rectangular
 windows, etc, are not considered occluding.
