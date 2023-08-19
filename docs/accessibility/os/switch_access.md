# Switch Access (for developers)

Switch Access is a Chrome OS feature to control the computer with 1 or 2
**switches**, or button-like inputs. It is targeted at supporting users with
motor or mobility impairments, for whom other methods of controlling the device
are not feasible.


## Using Switch Access

Go to Chrome OS settings > Accessibility settings > "Manage accessibility
features" > Enable "Switch Access". You can assign switches to actions and
enable or disable automatic scanning on the Switch Access settings subpage.
For most development purposes, buttons on either the built-in keyboard or an
external keyboard can be used as switches, rather than needing a dedicated
switch device.

With this feature enabled, you can navigate to and interact with **actionable**
elements (or **nodes**) onscreen. Because there are so many nodes onscreen at
any given moment, they are organized into a nested system of **groups** based on
proximity and other semantic information.


### Navigation

Switch Access supports two methods of navigation between nodes: manual scanning
and automatic scanning (or **auto-scan**). Manual scanning means the user
navigates from one element to the next by pressing one of their switches.
Automatic scanning means that Switch Access moves from one element to the next
after a set period of time (the **scanning speed**).

The user can identify which element is currently focused because it is
surrounded by a **focus ring**, which is two concentric rounded rectangles of
contrasting colors (currently fixed at light and dark blue) that surround the
element. Sometimes, a second focus ring will appear onscreen, which has dashed
line for the inner rectangle. This dashed focus ring provides a preview of which
node will be focused next, if the user selects the current node. It also
provides a hint that the current node is a navigational node, and is not
directly actionable.

There are two types of navigational nodes: nodes that group other nodes
together, and the **back button**. The back button allows a user to exit the
current group, and move outwards towards the top-level node.


### Selection

All users have one switch dedicated to selecting elements. For some users, this
is the only input they have (they rely on auto-scan to advance to the next
node). So a user needs to be able to perform any action based on some series of
select actions. To support this, when multiple actions are available for a
single actionable node, we open the **action menu**, which displays the
available actions for the given node. The user can then navigate to the action
they wish to perform, and when they select an action the action is performed,
and in most cases the menu is closed.

Sometimes there is only one action available for a given node. In this case, the
action menu does not open. Instead, the available action is performed
automatically when the user presses *select*.


## Reporting bugs

Use bugs.chromium.org, filing bugs under the component
[OS>Accessibility>SwitchAccess](https://bugs.chromium.org/p/chromium/issues/list?sort=-opened&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified&q=component%3AOS%3EAccessibility%3ESwitchAccess%20&can=2).

## Developing

### Code location

Switch Access code lives mainly in four places:

- A component extension to do the bulk of the logic and processing,
`chrome/browser/resources/chromeos/accessibility/switch_access/`

- In the `AccessibilityEventRewriter`,
`ash/events/accessibility_event_rewriter.h`

- The Switch Access menu and back button code, in `ash/system/accessibility/`

- The Switch Access settings page,
`chrome/browser/resources/ash/settings/os_a11y_page/switch_access_subpage.*`


### Tests

Tests are in `unit_tests`, `ash_unittests`, and `browser_tests`:

```
out/Release/unit_tests --gtest_filter="*SwitchAccess*"
out/Release/ash_unittests --gtest_filter="*SwitchAccess*"
out/Release/browser_tests --gtest_filter="*SwitchAccess*"
```


### Debugging

Developers can add log lines to any of the C++ files and see output in the
console. To debug the Switch Access extension, the easiest way is from an
external browser. Start Chrome OS on Linux with this command-line flag:

```
out/Release/chrome --remote-debugging-port=9222
```

Now open http://localhost:9222 in a separate browser, and debug the Switch
Access extension background page from there.


## How it works

Like [Chromevox](chromevox.md) and [Select to Speak](select_to_speak.md),
Switch Access is implemented mainly as a component Chrome extension which
is always loaded and running in the background when enabled, and unloaded
when disabled. Unlike Chromevox and Select to Speak, the settings for
Switch Access are not located within the component extension. Instead,
they are found with the other settings in the Settings app.

There are a handful of tasks which, for various reasons, are handled in
C++ code. These are:

- Event forwarding. In order to capture the events before other system
functions, we use an Event Rewriter (shared with Chromevox).

- Rendering the menu. To allow the menu UI to match other system UI, the process
of creating the menu UI is done in C++.

The Switch Access extension does the following, at a high level:

1. Listens for SwitchAccessCommands, which are the user-initiated commands like
"select" or "next" that the user performs by pressing their switch.

2. Maps those commands to the appropriate behavior:

    - If it is a navigation command, the focused node is changed and focus rings
    are updated.

    - If it is a select command, the available actions are determined.

    - If there is only one action, it is performed (Note that for navigational
    nodes, this means entering or exiting a group). If more than one action is
    available, the menu is opened and focus jumps into the menu.

3. Listens for focus events, and moves Switch Access focus to follow system
focus.

4. Listens for location changes and other tree changes that affect the current
node or its parent or siblings, and update the focused node and focus rings when
necessary.


## Switch Access extension structure

### Managers

Switch Access divides tasks by function, each being handled by a manager class.
This section details what those managers are, their guiding principle, and what
tasks they are responsible for.

#### ActionManager

The ActionManager is responsible for what happens after a user presses their
*Select* switch. This includes determining what actions are available,
performing actions, and specifying what actions to display in and where to show
the action menu and sub-menus when needed.

The specifics of opening and closing the menu are not handled here, but are
encapsulated in the MenuManager. However, all calls to open or close the menu
should be made on the ActionManager, rather than the MenuManager directly,
because the ActionManager maintains state about sub-menus and similar, which
needs to remain in sync with the menu displayed.

#### AutoScanManager

The AutoScanManager handles the process of automatically scanning from one
element to the next. If the user has enabled auto-scan, it sets an interval
to call `NavigationManager.moveForward()` periodically. It also resets the
interval each time a command is received, or a focus event causes the focused
node to change.

#### FocusRingManager

The FocusRingManager determines what focus rings should be drawn and where to
show them, and calls the `accessibilityPrivate` API.

#### MenuManager

The MenuManager handles the details of displaying the action menu, given a
list of actions and a location. It also waits for the menu to load, and jumps
Switch Access focus to the menu when it is ready.

Calls to open or close a menu should be handled by the ActionManager, rather
than by the MenuManager directly, to keep state in sync.

#### NavigationManager

The NavigationManager handles tracking and changing the current Switch Access
focus. This includes handling moving forward or backward from a user command,
moving forward from auto-scan, jumping to a UI element when opened (such as the
keyboard or action menu), and following system focus. It also handles entering
and exiting groups when navigational nodes are selected.

The NavigationManager also has a method `getTreeForDebugging()` which can be
used to print either the current group or the entire tree to the console, to
help with debugging.

#### PreferenceManager

The PreferenceManager handles accessing user preferences, and updating
Switch Access' behavior accordingly. It also verifies whether the user has
a configuration that allows for full navigation.

#### TextNavigationManager

The TextNavigationManager handles tasks surrounding navigating through text.
Currently only text within editable text fields is supported, and only behind
the flag `enable-experimental-accessibility-switch-access-text`.


### Nodes

In addition to dividing the tasks by function, Switch Access delegates many
tasks to the nodes themselves. This allows specific behaviors to be changed
by changing what nodes are created based on the circumstances, and keeps the
ActionManager and NavigationManager from getting bloated with special cases,
and all the permutations of those special cases.

All of the nodes used by Switch Access have some relation to an **automation
node**, the underlying tree structure shared by all accessibility component
extensions. Most have a one-to-one association (one automation node for each
Switch Access node), but sometimes there are multiple Switch Access nodes for
the same automation node (such as when breaking a long list into smaller
groups).

#### SAChildNode and SARootNode

These are the two base types. They define the functions available on nodes
when they are functioning as either a selectable node, or the current group.

SAChildNode is an abstract class, but SARootNode can be used directly to
represent a group that does not have an underlying automation node.

#### BasicNode and BasicRootNode

These types implement a default behavior for nodes that have a one-to-one
association with an automation node. Information is derived directly from
the automation node to implement the interface provided by SAChildNode and
SARootNode.

#### BackButtonNode

The BackButtonNode is special because it is part of Switch Access' UI, and
it is shown for each group. Therefore, it automatically finds the automation
node corresponding to the back button by searching the tree, and instead
takes the SARootNode for its group in its constructor. It uses this to
determine where to show the back button, and what to do if it is selected.

#### ComboBoxNode

ComboBoxNode extends BasicNode, and only overrides a small number of functions
where combo boxes have special behavior. Specifically, combo boxes require some
special logic around moving focus into the dropdown when it opens.

#### DesktopNode

Represents the largest group possible, the entire computer desktop. Behaves
mostly like a SARootNode, but does not have a back button.

#### EditableTextNode

Editable text nodes have a very different set of actions. Currently supported
are opening the keyboard and using dictation, but many more actions (such as
selection and copy/paste) are available with the flag
`enable-experimental-accessibility-switch-access-text`.

#### GroupNode

A GroupNode represents a subset of the children of a single automation node,
used to break a single node with many children into intermediate nodes for
easier navigation.

Currently the only place this is used is to break the keyboard into rows, but
ideally this should be done for any group with more than some number of
children.

#### KeyboardNode and KeyboardRootNode

A KeyboardNode represents a button within the virtual keyboard. Because the
keys do not support actions through the automation API, the KeyboardNode
simulate a mouse press at the center of the button.

The KeyboardRootNode represents the keyboard as a whole. It handles finding
the buttons from the keyboard and grouping them into rows, as well as
finding the automation node representing the virtual keyboard and monitoring
the visibility of the keyboard, so if the user opens/closes the keyboard
other than via Switch Access we still detect it.

#### ModalDialogRootNode

Currently used when a menu is opened, it behaves exactly like a BasicRootNode
except that when the user exits, it fires an ESC key event to close the menu
or modal dialog.

#### SliderNode

Adds support for custom sliders by sending left/right arrow key events to
decrement/increment.

#### TabNode and ActionableTabNode

These two classes are primarily to allow the close button to both be
accessible via Switch Access while clearly displaying visually how to select
the tab. This is done by having TabNode be a group (when it contains a button),
and create an ActionableTabNode as a child whose location is just the portion of
the tab that doesn't overlap with the close button.

#### WindowRootNode

The WindowRootNode focuses a window when it is entered. Otherwise it behaves
exactly like a BasicRootNode.


### Other

There are some other classes that support Switch Access without being a manager
or node type directly.

#### background.js

This file is the first one run. Its primary job is to create an instance of
SwitchAccess, although it also overifies that there is not more than one instance
of Switch Access running simultaneously (this would normally happen on the sign
in page).

#### Commands

Commands translates a SwitchAccessCommand from the user into a call to the
appropriate function.

#### History

History helps store and recover state about what the current node and group are.
When a group is exited, the history provides the previous position to return to
(after verifying that it is still valid). It also builds a new history when
focus moves, capturing how the user would have gotten to that same node.

#### Metrics

A utility for recording metrics.

#### SACache

Implements a dynamic programming strategy for when walking the automation tree to
find interesting nodes. A cache is created for a single query, and should not be
used in nonconsecutive function calls, as the underlying data can change and the
SACache does not account for this.

#### SAConstants

This file contains most or all of the constants used throughout Switch Access.

#### SwitchAccess

Primarily, this class creates the managers that need to be explicitly initialized,
as well as the Commands object. It also handles creating errors (so we can track
metrics on how often different errors happen), and has a function to find a node
matching a predicate (or wait for it to be created).

#### SwitchAccessPredicate

Has a variety of predicates determining things about automation nodes, all
generally specific to Switch Access. Many of these functions utilize SACache.
It also creates the restrictions that can be passed to AutomationTreeWalker to
find the appropriate children for a given group node.


## For Googlers

For more, Googlers could check out the Switch Access feature design docs for
more details on design as well as UMA.

- Overall product design, [go/cros-switch](go/cros-switch)

- The navigation strategy, [go/cros-switch-navigation](go/cros-switch-navigation)

- The action menu, [go/cros-switch-menu](go/cros-switch-menu)

- The "preview" dashed focus ring, [go/cros-switch-dashed-focus](go/cros-switch-dashed-focus)

- The UX description slideshow, [go/cros-switch-access-ux](go/cros-switch-access-ux)

- The UI specification, [go/cros-switch-spec](go/cros-switch-spec)

- Testing, [go/cros-switch-testing](go/cros-switch-testing)

- Improved Text Input (still experimental), [go/cros-switch-text-input](go/cros-switch-text-input)

- Point Scanning (still under development), [go/cros-switch-point-scanning](go/cros-switch-point-scanning)
