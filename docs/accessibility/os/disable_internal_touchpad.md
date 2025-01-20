# ChromeOS Disable internal touchpad

To prevent accidental clicks and improve the experience for external mouse users,
ChromeOS allows you to disable the built-in touchpad. This accessibility feature
offers options for always-off or disabling the touchpad only when a mouse is connected.

## Summary

### User flow

ChromeOS allows users to disable the built-in touchpad through the accessibility
settings. Here's the user flow:

1. **Navigate to Settings**: Go to Accessibility > Cursor and touchpad >
Disable internal touchpad.

2. **Choose a mode**: Select one of the following options:
* **Never**: The touchpad is always enabled.
* **Always**: The touchpad is always disabled.
* **When a mouse is connected**: The touchpad is disabled only when an external
mouse is connected.

3. **Confirmation (Applies when mode is "Always" or "When a mouse is connected"):**
The internal touchpad is disabled when the confirmation dialog is shown. This
ensures you don't accidentally disable the touchpad without an alternative
input method (keyboard or mouse). If the dialog isn't confirmed or canceled before
the 30 second timeout the touchpad will be re-enabled.

* Once confirmed, a notification appears in the bottom right corner indicating
the touchpad is disabled. You can re-enable the touchpad directly from this
notification.

4. **Device Page**: The touchpad's status (enabled/disabled) is also reflected
in the Devices page.

5. **"When a mouse is connected" mode specifics**:
* The confirmation dialog appears only the first time an external mouse is connected
after enabling this mode.

### Technical Overview

The internal touchpad disabling feature in ChromeOS is implemented using Event Rewriters.
Event Rewriters intercept and modify event streams, allowing the system to selectively
block touchpad input.

Key Components:

1. [DisableTouchpadEventRewriter](https://source.chromium.org/chromium/chromium/src/+/main:ash/accessibility/disable_touchpad_event_rewriter.cc?q=DisableTouchpadEventRewriter):
* Determines the source of mouse events (internal touchpad or external mouse).
* Detects the presence of a connected external mouse.
* Disables mouse events from the internal touchpad based on the configured mode
and mouse connection status.
* Includes a mechanism (e.g. pressing the Shift key 5 times) to re-enable
the touchpad.

2. [Disable Touchpad Dropdown](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/os_a11y_page/cursor_and_touchpad_page.ts):
* Provides the user interface for selecting the touchpad disabling mode.
* Displays instructions to the user on how to re-enable the touchpad (e.g. by pressing the Shift key 5 times).

3. [Devices Page](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/resources/ash/settings/device_page/per_device_touchpad_subsection.ts):
* This page displays the current status of the touchpad (enabled or disabled), providing visual feedback
to the user.

4. [AccessibilityController](https://source.chromium.org/chromium/chromium/src/+/main:ash/accessibility/accessibility_controller.cc):
* Manages the display of confirmation dialogs and notifications to the user.
* Monitors user preferences for touchpad disabling mode and informs the DisableTouchpadEventRewriter.

### Testing
* [Settings Tests](https://source.chromium.org/chromium/chromium/src/+/main:chrome/test/data/webui/chromeos/settings/os_a11y_page/cursor_and_touchpad_page_test.ts):
Verify that user selections in the "Cursor and touchpad" settings page correctly update
the underlying preferences.

* [DisableTouchpadEventRewriter Tests](https://source.chromium.org/chromium/chromium/src/+/main:ash/accessibility/disable_touchpad_event_rewriter_unittest.cc):
Validate the core logic of the DisableTouchpadEventRewriter, ensuring it correctly
disables and enables the touchpad based on different scenarios (mode selection,
mouse connection status, etc.).

* [AccessibilityController Tests](https://source.chromium.org/chromium/chromium/src/+/main:ash/system/accessibility/floating_accessibility_controller_unittest.cc):
Confirm that the AccessibilityController correctly manages the display of confirmation
dialogs and notifications to the user.