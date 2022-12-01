#  About Mac Hotkeys and Virtual Keycodes
This doc is useful if you need to:
* Examine the definition of hotkeys or add new ones
* Determine why a certain hotkey press on a given keyboard layout maps to a particular command
* Generally understand the flow from hotkey press to execution
<br>
<br>
## Hotkey Definitions

Many Mac apps define hotkeys in nib files. We configure them in code.
<br>
### __Menu Hotkeys__
* [AcceleratorsCocoa](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/cocoa/accelerators_cocoa.mm?q=acceleratorscocoa) sets up the mapping of Chrome commands to hotkeys
* [MainMenuBuilder](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/cocoa/main_menu_builder.mm?q=file:main_menu_builder%20AcceleratorsCocoa%5C:%5C:GetInstance&ss=chromium%2Fchromium%2Fsrc) consults AcceleratorsCocoa as it assigns hotkeys to menu items

### __Hidden Hotkeys__
* [global_keyboard_shortcuts_mac.mm](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/global_keyboard_shortcuts_mac.mm?q=%5C:%5C%20GetMenuItemsNotPresentInMainMenu%5C(%5C)%20&ss=chromium%2Fchromium%2Fsrc) sets up hotkeys that aren't associated with any menu item, such as ⌘1 which selects the current browser window's first tab

### __System Hotkeys__
* System Hotkeys (AKA Apple Symbolic Hotkeys) are things like "Mission Control" (^↑) which appear in the Shortcuts sheet in the Desktop and Dock pane in System Settings
* The [SystemHotkeyMap](https://source.chromium.org/chromium/chromium/src/+/main:content/browser/cocoa/system_hotkey_helper_mac.h?q=file:system_hotkey_helper_mac.h%20GetSystemHotkeyMap&ss=chromium%2Fchromium%2Fsrc) maintains a table of the user's system-reserved hotkeys
* We call SystemHotkeyMap::IsHotkeyReserved() to quickly check if an incoming hotkey is a system hotkey (and ignore it if so)
* See [Documenting com.apple.symbolichotkeys.plist](https://web.archive.org/web/20141112224103/http://hintsforums.macworld.com/showthread.php?t=114785) for more information on the different system hotkeys and how they're stored<br>
<br>
## Hotkey Execution
Hotkeys populate Chrome's menus but we don't use the normal AppKit machinery to trigger their commands. Websites can override most hotkeys, which means we want to sometimes defer first to the renderer before falling back on a command in the menus.
<br>
<br>


[`- [ChromeCommandDispatcherDelegate prePerformKeyEquivalent:window:]`](https://source.chromium.org/search?q=file:chrome_command_dispatcher_delegate.mm%20%5E.*prePerformKeyEquivalent%5C:.*)
* Starts hotkey processing
* Executes commands for hotkeys that can't be overridden such as File->New and Chrome->Quit as well as hotkeys registered by extensions
* Passes unconsumed hotkeys to the RenderWidgetHostViewCocoa
<br>
<br>


[`- [RenderWidgetHostViewCocoa performKeyEquivalent:]`](https://source.chromium.org/search?q=file:render_widget_host_view_cocoa.mm%20%5E.*performKeyEquivalent%5C:.*%5C%7B)
* Receives hotkeys for processing in RenderWidgetHostViewCocoa
* Returns `NO` for any system-reserved hotkeys per the SystemHotkeyMap
* Sends the rest to `-keyEvent:wasKeyEquivalent:`
<br>
<br>


[`- [RenderWidgetHostViewCocoa keyEvent:wasKeyEquivalent:]`](https://source.chromium.org/search?q=file:render_widget_host_view_cocoa.mm%20%5E.*keyEvent%5C:.*%5CwasKeyEquivalent%5C:.*%7B)
* Forwards incoming hotkeys to the renderer via `_hostHelper->ForwardKeyboardEventWithCommands()`, which gives the website a chance to consume them
* Tells Cocoa the hotkey was consumed even though it doesn't know for sure (renderer hotkey processing is asynchronous)
<br>
<br>


[`- [CommandDispatcher redispatchKeyEvent:]`](https://source.chromium.org/search?q=file:command_dispatcher.mm%20%5E.*redispatchKeyEvent%5C:.*%5C%7B)

* Receives hotkeys that weren't consumed by the renderer
* Redispatches hotkeys to the ChromeCommandDispatcherDelegate for final processing
<br>
<br>

[`- [ChromeCommandDispatcherDelegate postPerformKeyEquivalent:window:isRedispatch:]`](https://source.chromium.org/search?q=file:chrome_command_dispatcher_delegate.mm%20%5E%5B%5E%5C%2F%5D*postPerformKeyEquivalent%5C:.*)

* Calls `CommandForKeyEvent()` to locate the NSMenuItem with the corresponding hotkey and executes its command
<br>
<br>


[`CommandForKeyEvent()`](https://source.chromium.org/search?q=file:global_keyboard_shortcuts_mac.mm%20%5ECommandForKeyEventResult%5C%20CommandForKeyEvent%5C(.*%5C)%5Cs*%5C%7B)
* Calls `cr_firesForKeyEquivalentEvent:` on each menu item until it finds one with the same hotkey
* For hidden hotkeys (i.e. not associated with a menu item), this function calls `cr_firesForKeyEquivalentEvent:` on a set of invisible NSMenuItems that have been assigned the hidden hotkeys
<br>
<br>


[`- [NSMenuItem cr_firesForKeyEquivalentEvent:]`](https://source.chromium.org/search?q=file:nsmenuitem_additions.mm%20%5E.*cr_firesForKeyEquivalentEvent%5C:.*%5C%7B)
* Returns `YES` if the menu item's hotkey matches the event's
* For non-US keyboard layouts, uses the event's `characters`, `charactersIgnoringModifiers`, and virtual `keyCode` to decide if they match
  * For example, if the incoming event's keycode is for a number key (`kVK_ANSI_1` through `kVK_ANSI_0`), this method matches the event to the hotkeys that select a window tab (⌘1, etc.), regardless of the actual `characters` or `charactersIgnoringModifiers` in the event
<br>
<br>


## About Virtual Keycodes
* Virtual keycodes identify physical keys on a keyboard, providing a hardware- and language-independent way of specifying keyboard keys
  * For example, `kVK_Return` is the virtual keycode for the last key in the fourth row from the top in the US keyboard layout
  * See [HIToolbox.framework/Events.h](https://web.archive.org/web/20221028180946/https://github.com/phracker/MacOSX-SDKs/blob/master/MacOSX10.6.sdk/System/Library/Frameworks/Carbon.framework/Versions/A/Frameworks/HIToolbox.framework/Versions/A/Headers/Events.h) for kVK_* definitions
  * See [Complete list of AppleScript key codes](https://web.archive.org/web/20220925114939/https://eastmanreference.com/complete-list-of-applescript-key-codes) for a visual mapping of keycodes
  * See [Converting CGKeyCodes for International Keyboards](https://web.archive.org/web/20221028180852/https://stackoverflow.com/questions/5253972/converting-cgkeycodes-for-international-keyboards) for a discussion of the meaning of virtual keycodes
* Keycodes with "ANSI" in the name correspond to key positions on an ANSI-standard US keyboard
  * For example, `kVK_ANSI_A` is the virtual keycode for the key next to 'Caps Lock' in the US keyboard layout
* A key's virtual keycode does not change when a modifier key is pressed but the character it generates might
* Chrome uses Windows VKEY virtual keycodes, which differ from the Mac's keycodes, throughout its platform-independent code
  * Functions like [`KeyboardCodeFromKeyCode()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/events/keycodes/keyboard_code_conversion_mac.mm?q=KeyboardCode%5C%20KeyboardCodeFromKeyCode%5C%28%20file:keyboard_code_conversion_mac.mm&ss=chromium%2Fchromium%2Fsrc) and [`MacKeyCodeForWindowsKeyCode()`](https://source.chromium.org/chromium/chromium/src/+/main:ui/events/keycodes/keyboard_code_conversion_mac.mm?q=MacKeyCodeForWindowsKeyCode%5C%28%20file:keyboard_code_conversion_mac.mm&ss=chromium%2Fchromium%2Fsrc) translate between the two worlds
* "Located" keyboard codes identify specific keys with the same meaning
  * For example, `VKEY_LSHIFT` and `VKEY_RSHIFT` are 'located' keyboard codes while `VKEY_SHIFT` is their non-located representation
  * Similar mappings exist for the Option, Control, and numeric keypad keys
<br>
<br>


## Sample Bugs
When debugging new hotkey / keycode issues, the following fixed bugs may help suggest a place to start.

### __For RTL tabstrips, switching tabs with ⌘[ and ⌘] behaves opposite to expectations__ ([crbug.com/672876](https://bugs.chromium.org/p/chromium/issues/detail?id=672876))

The default hotkey assignments for ⌘[ and ⌘], which select the "next" and "previous" tabs, respectively, needed to be swapped for RTL users. These hotkey definitions live in [AcceleratorsCocoa()](https://source.chromium.org/chromium/chromium/src/+/main:chrome/browser/ui/cocoa/accelerators_cocoa.mm?q=acceleratorscocoa), which sets up the mapping of Chrome commands to hotkeys. After checking for RTL, the code exchanged the hotkey assignments.


### __Chrome instantly quits on Command-Q despite 'Warn Before Quitting' in non-US keyboard layout__ ([crbug.com/142944](https://bugs.chromium.org/p/chromium/issues/detail?id=142944))

In many non-US keyboard mappings, `[hotkeyEvent charactersIgnoringModifiers]` returns a non-ASCII character while `[hotkeyEvent characters]` returns an ASCII character. In these situations, as a quick hack we use the ASCII character string to determine which hotkey the user is triggering. Some keys in some non-US keyboard mappings, however, return ASCII for both -characters and -charactersIgnoringModifiers. This is the case for the `kVK_ANSI_Q` key in the Hebrew layout. As a result, pressing ⌘Q in the layout resulted in a ⌘\ hotkey event. No code looks for ⌘\, and the original event filtered down to a layer that processed the ⌘Q correctly, but without the "Warn Before Quitting" check.

The solution was to check for this special "Q" / "\" combination in `[NSMenuItem(ChromeAdditions) cr_firesForKeyEquivalentEvent:]` and interpret the event as ⌘Q.

### __Some Shortcut keys do not work on Dvorak-Right-Handed Keyboard__ ([crbug.com/1358823](https://bugs.chromium.org/p/chromium/issues/detail?id=1358823))

Chrome Mac interprets ⌘`kVK_ANSI_1` through ⌘`kVK_ANSI_0` as a tab switch hotkey. However, certain flavors of Dvorak don't generate numbers from the numbered ANSI keys. For example, `kVK_ANSI_8` generates a "p", so ⌘`kVK_ANSI_8` should be interpreted as ⌘P (Print) but instead switched the browser to the eighth tab.

The solution was to add an exception for the Dvorak keyboards in `[NSMenuItem(ChromeAdditions) cr_firesForKeyEquivalentEvent:]`.
