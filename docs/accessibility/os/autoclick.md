# Automatic clicks (for developers)

Automatic clicks is a Chrome OS feature to automatically generate mouse events
when the cursor dwells in one location.


Dwell control supports users with motor impairments. A user who is unable to
use a mouse or unable to click with a mouse, but who is able to control the
cursor position (often using an alternative input method) will need to use
dwell control to perform mouse or trackpad actions, including left-click,
right-click, double-click, click-and-drag and scroll.

## Using automatic clicks

Go to Chrome settings, Accessibility settings, “Manage accessibility Features”,
and in the “mouse and input” section enable “Automatically click when the
cursor stops”. You can adjust timing, radius, stabilization, and whether to
revert to a left-click after another type of action has been taken from the
settings page.


A on-screen menu bubble will appear in the corner. Dwell over this menu to
change the action taken, or pause the feature. There is also a button to
re-position the menu to another corner of the screen.

## Reporting bugs

Use bugs.chromium.org, filing bugs under the component UI>Accessibility with
the label “autoclick” (or, use
[this template](https://bugs.chromium.org/p/chromium/issues/entry?summary=Autoclick%20-%20&status=Available&cc=katie%40chromium.org%2C%20qqwangxin%40google.com&labels=Pri-3%2C%20autoclick%2C&components=UI>Accessibility)).


Open bugs have the label
“[autoclick](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=label%3Aautoclick)”.

## Developing

### Code location

Automatic clicks code lives mainly in three places:

- A controller, event-rewriter, and widgets to draw around click locations, in
ash/accessibility/autoclick/

- UI through menu bubbles and their controllers, in
ash/system/accessibility/autoclick*

- A component extension to provide Accessibility tree information, in
chrome/browser/resources/chromeos/accessibility/accessibility_common/

In addition, there are settings for automatic clicks in
chrome/browser/resources/ash/settings/os_a11y_page/cursor_and_touchpad_page.*

### Tests

Tests are in ash_unittests and in browser_tests:

```
out/Release/ash_unittests --gtest_filter=”Autoclick*”
out/Release/browser_tests --gtest_filter=”Autoclick*”
```

### Debugging

Developers can add log lines to any of the autoclick C++ files and see output
in the console. To debug the Accessibility Common extension, the easiest way is
from an external browser. Start Chrome OS on Linux with this command-line flag:

```
out/Release/chrome --remote-debugging-port=9222
```

Now open http://localhost:9222 in a separate instance of the browser, and debug
the Accessibility Common extension background page from there.

## How it works

AutoclickController is a pre-target EventHandler with very high priority,
which means it can receive and act on mouse events before other parts of
Chrome OS get them.


AutoclickController::OnMouseEvent receives mouse events and checks whether
the event is close to the current dwell target (in which case dwell count-down
should continue), or far enough away to clear the target and set a new one.


There is a small delay before the user sees the count-down timer ring UI appear
(AutoclickRingHandler) show up, which is controlled by the start_gesture_timer_.
Performing the click is controlled by the autoclick_timer_.


When the autoclick_timer_ completes it calls
AutoclickController::DoAutoclickAction. This function first checks if the
target point is over either of the autoclick bubbles, the menu or the scroll
bubble. If it is over a bubble the gesture will not be handled with a synthetic
event, but instead sent directly to that menu. This keeps focus from shifting
and things like dialogs or context menus from closing when the user interacts
with an autoclick bubble. But, if the target was not over the bubble, a
synthetic event is generated as follows:

### Left-click, right-click and double-click

Synthetic mouse events for ui::EventType::kMousePressed and
ui::EventType::kMouseReleased are created with the appropriate mouse button
flags, and sent to the WindowTreeHost under the target point for processing.
For double-click, a second press and release pair are also sent.

### Click-and-drag

A synthetic mouse event for ui::EventType::kMousePressed is created at the first
dwell. An AutoclickDragEventRewriter is enabled and begins re-writing all
ui::EventType::kMouseMoved events to ui::EventType::kMouseDragged events to
create the illusion of a drag. This occurs in
AutoclickDragEventRewriter::RewriteEvent.

A final synthetic mouse event for ui::EventType::kMouseReleased is created at
the second dwell, and the AutoclickDragEventRewriter is disabled.

### Scroll

On a dwell during scroll, the scroll target point is changed. No scroll events
are generated from AutoclickController until the user hovers over the scroll
pad buttons (AutoclickScrollButton class). The AutoclickScrollButtons track
whether they are currently hovered, and while hovered they fire a repeating
timer to ask the AutoclickController to do a scroll event.

#### Scroll location

By default, the scroll position is at the center of the active screen. Users
may dwell anywhere on the screen to change the scroll location.


When the scroll location is changed, the AutoclickController will request the
bounds of the nearest scrollable view from the Accessibility Common extension
via the AccessibilityPrivate API. The Accessibility Common component extension
has access to accessibility tree information, and using a HitTest is able to
find the view at the scroll location, then walks up the tree to find the first
view which can scroll, or stops at the nearest window or dialog bounds. This
logic takes place in autoclick.js, onAutomationHitTestResult_. When the
scrolling location is found, the bounds of the scrollable area are highlighted
with a focus ring. In addition, the bounds are sent back through the
AccessibilityPrivate API, routed to the AutoclickController, which passes it via
the AutoclickMenuBubbleController to the AutoclickScrollBubbleController, which
does layout accordingly.

### Bubble Menus: interface and positioning

The AutoclickController owns the AutoclickMenuBubbleController, which controls
the widget and AutoclickMenuView view for the autoclick bubble menu.
AutoclickMenuView inherits from TrayBubbleView for styling, and contains all
the buttons to change click types, pause, and update the menu position.


Similarly, AutoclickMenuBubbleController also owns
AutoclickScrollBubbleController so that it can pass on messages about
positioning and activation from AutoclickController.
AutoclickScrollBubbleController owns the widgets and AutoclickScrollView for
the autoclick scroll bubble. AutoclickScrollView inherits from TrayBubbleView
for styling, and contains the scroll pad buttons, including all the logic to
draw the custom shape buttons for left/right/up/down scrolling.

#### Menu positioning

The autoclick bubble menu can be positioned in the four corners of the screen
and defaults to the same location as the volume widget (which depends on
LTR/RTL language). AutoclickMenuBubbleController takes a preferred
FloatingMenuPosition enum and uses that to determine the best position for
the menu in AutoclickMenuBubbleController::SetPosition. This function finds
the ideal corner of the screen, then uses CollisionDetectionUtils (also used
by Picture-in-Picture) to refine the position to avoid collisions with system
UI.

#### Scroll bubble positioning

The scroll bubble starts out anchored to the automatic clicks bubble menu, but
if the user selects a new scroll point it will move. When a scroll point is
selected, if the scrollable region found by the Accessibility Common extension
is large enough, the scroll bubble will be anchored near the scroll point
itself, similarly to the way the context menu is anchored near the cursor on
a right click. When the scrollable region is small, the scroll bubble will be
anchored to the closest side of the scrollable region to the scroll point, as
long as there is space for it on that side.

#### Clicking on the bubble menus

The AutoclickController cannot generate synthetic click events over the
bubbles, because that would cause context and focus changes. For example, if
the user has a drop-down menu open, clicking the autoclick menu bubble will
cause the drop-down to close. Instead, the AutoclickController must check to
see if an event will take place over a bubble menu, and if so, request that
AutoclickMenuBubbleController forward the event to the bubble via
AutoclickMenuBubbleController::ClickOnBubble. This generates a synthetic mouse
event which does not propagate through the system, so there is no focus or
context change, allowing users to continue to interact with whatever was on
screen.

## For Googlers

Googlers could check out the automatic clicks feature design docs for more
details on design as well as autoclick’s UMA.

- Overall product design, [go/chromeos-dwell-design](go/chromeos-dwell-design)

- On-screen menu design,
[go/chromeos-dwell-menu-design](go/chromeos-dwell-menu-design)

- Scrolling design,
[go/chromeos-dwell-scroll-design](go/chromeos-dwell-scroll-design)

- UX mocks, [go/cros-dwell-ux](go/cros-dwell-ux)
