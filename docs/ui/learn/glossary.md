# Glossary

## How to use

Terms are given below, ordered alphabetically. Corrections or requests for
addition? [Contact](/docs/ui/ask/index.md) the Views team.

## Terms

### Ash

The Chrome OS windowing environment, responsible for non-browser UI such as the
login screen, system tray, and various built-in control surfaces. Like Chrome,
Ash UI is built using the Views toolkit.

Sometimes, the phrase "Ash Chrome" is used to distinguish the historic
Chrome-on-CrOS design (that is, tightly integrated with the operating system)
from "LaCrOS Chrome" (where the browser is basically separate from the OS, as on
other desktop platforms).

### Aura

A cross-platform window manager abstraction used on desktop platforms other than
macOS. Aura handles tasks like showing windows onscreen and responding to native
input events. Because Aura is not the only "platform" Views targets, there is
some complexity in Views that [would not be necessary](http://crbug.com/327611)
if Mac used Aura as well; however, Mac platform conventions differ enough from
the platforms Aura currently supports that using Aura on Mac might require
significant redesign or impose undesirable API constraints. As of this writing,
the question of whether Mac should use Aura remains open.

### AX

An abbreviation for "accessibility". Many different classes with the "AX" prefix
are involved in exposing accessibility information for Views, but perhaps the
best starting point is one that lacks it:
[`ViewAccessibility`](/ui/views/accessibility/view_accessibility.h).

### Cocoa

The application environment on macOS. Originally, Chrome for Mac had UI written
exclusively in Cocoa; now the majority of the native UI uses Views, but there
are still pieces in Cocoa. Code interacting with Cocoa is generally written in
Objective-C++, whose source files typically end in `.mm`.
[More general background on Cocoa](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/CocoaFundamentals/WhatIsCocoa/WhatIsCocoa.html)

### Combobox

A control that displays a selected choice from a list of options. Sometimes
called a "select" or "dropdown" control. The specific
[control](/ui/views/controls/combobox/combobox.h) in Views has a focus ring and
a dropmarker by default, and can be customized in a variety of ways. The "Combo
Box" example in [`views_examples`](ui_debugging.md#views-examples) creates a
couple of simple comboboxes.

### Compositor

A system responsible for producing the final displayable graphics for a
`Widget`, given a `View` hierarchy. The compositor manages a `Layer` tree that
allows it to cache, transform, and blend various graphical layers so that it can
minimize repaints, accelerate scrolling, and sync animation to the underlying
display refresh rate.

### Focus Ring

A colored outline around an object that is visible when an object has focus
and hidden when focus is gone. It is often drawn by a
[`FocusRing`](/ui/views/controls/focus_ring.h) object, a `View` subclass that
handles drawing a colored border along a path. When creating focus rings, it's
important to color them such that they contrast with both the color inside and
the color outside the ring; see the uses of the
[`PickGoogleColorTwoBackgrounds`](/ui/color/color_transform.cc) function.

### HWND

Windows-specific. An opaque handle to a system-native window. Ultimately
[type-aliased to a `void*`](https://learn.microsoft.com/en-us/windows/win32/winprog/windows-data-types),
but you should never dereference this or examine the actual value; it is only
used to pass to various Windows native APIs. Used as the type for
[`AcceleratedWidget`](/ui/gfx/native_widget_types.h) on Windows.

### Ink drop

The collection of classes that implement hover and click effects on `View`s.
These came from the original Material Design and use dynamically-created layers
to draw translucent overlays that may be contained to the bounds of a `View` or
extend outside it. The name came from the appearance of a drop of ink spreading
in water, and the most prominent effect, known as a "ripple", spreads rapidly
outward from a single point (generally the cursor position). The base
[`InkDrop`](/ui/views/animation/ink_drop.h) class can also manage instant on/off
"hover highlight" effects and contains a variety of hooks for different uses.

### LaCrOS

An abbreviation for "Linux And Chrome OS". An architecture project to decouple
the Chrome browser on Chrome OS from the operating system stack; historically
these were tightly coupled and updated simultaneously.
[More general background on LaCrOS](/docs/lacros.md)

### Layer

A node in a tree of objects used by the compositor to manage textures. Calling
`View::SetPaintToLayer()` causes a `View` to paint to a `Layer`, which is
necessary to do things like paint outside the `View`'s own bounds or perform
certain kinds of operations (e.g. blending) in hardware instead of software;
however, layers have memory and performance costs and affect sibling paint
order, and should be used sparingly.

### Layout

An operation where a `View` sets the bounds of all of its children. Generally,
calling `InvalidateLayout()` on a `View` will recursively mark that `View` and
all ancestors as dirty; then at some point,
`Widget::LayoutRootViewIfNecessary()` will call `LayoutImmediately()` on the
`RootView`, which will call `Layout()` on its contents, and so forth down the
tree. `View`s can currently implement custom layout by overriding the virtual
`Layout()` method, but should instead use one of the available `LayoutManager`s
to describe how children should be arranged; then the `LayoutManager` will be
responsible for both computing the `View`'s preferred size and for updating
child bounds as necessary.

### NativeWidget

The platform-specific object that backs the cross-platform `Widget` object;
`Widget` implements its API by calling methods on the `NativeWidget`. This is
implemented separately for Aura and Mac, and at least on Aura is still an
abstraction over the true underlying OS windowing objects. Historically,
lifetime and ownership between `Widget` and `NativeWidget` instances could vary
and was error-prone; as of this writing, this is
[being addressed](https://crbug.com/1346381).

### NS*

An abbreviation for "NeXTSTEP". Seeing this prefix on a type generally means
that the type is a longstanding fundamental type in macOS, whose origins date to
the 1990s and Apple's acquisition of NeXT.

### Omnibox

The combined search and address bar in the browser window toolbar. Coined by
early PM Brian Rakowski (as "psychic omnibox") because it knows and does
everything. In code, the term "omnibox" generally refers specifically to the
text editing control and its associated popup; the larger visual "bar" it
appears in, which also contains affordances to indicate security state, allow
bookmarking, and display page action icons, is called the "location bar".

### Progressive Web App (PWA)

A website that may provide various native-app-like capabilities, such as
installability, integration with system hardware, and/or persistence. PWAs in
Chrome are given special UI treatment.
[More general background on PWAs](https://web.dev/progressive-web-apps/)

### SkColor

A 32bpp (8 bits per channel) non-premultiplied ARGB color value, stored as a
`uint32_t`. This is the most common color type used in Views when interacting
with drawing and painting APIs. Because this is a fixed (physical) color value
instead of a themed (logical) color identifier, computed `SkColor`s may need
recalculation on theme changes; in general, prefer to pass colors around as
`ColorId`s instead, where possible.

### Skia

The 2D graphics library Chrome uses. Skia is maintained by Google employees and
provides hardware-accelerated drawing primitives.
[More general background on Skia](https://skia.org/)

### Skia Gold

A web application used to compare pixel test result images against verified
baselines. A test in `browser_tests` or `interactive_ui_tests` may
[opt in](/testing/buildbot/filters/pixel_tests.filter) to pixel testing, and
will then produce result images that are managed by
[a Skia Gold instance](https://chrome-gold.skia.org/?corpus=gtest-pixeltests).
As of this writing, these pixel tests are Windows-only. To run them, add the
`--browser-ui-tests-verify-pixels`, `--enable-pixel-output-in-tests`, and
`--test-launcher-retry-limit=0` flags when invoking the test binary.

### Throbber

An animated object used to indicate definite or indefinite progress to the user.
The specific [control](/ui/views/controls/throbber.h) in Views is a 16 DIP
spinning circle that can optionally display a checkmark when an operation
completes. The "Throbber" example in
[`views_examples`](ui_debugging.md#views-examples) creates a throbber.

### Views (library)

A cross-platform UI toolkit used to create "native" UI (as opposed to UI built
from web technologies). Views is important to both the Chrome browser and the
Chrome OS native UI in Ash. Views was created in the early days of Chrome to
abstract away calls to platform-specific functions and make it possible to
build Chrome UI for multiple operating systems; unlike other cross-platform
toolkits of the time like wxWidgets or GTK, it aimed for a platform-native feel
(especially on Windows) and a minimal API that was designed to do just what
Chrome needed. Over time the Views look-and-feel moved away from
platform-native fidelity, toward a style called "Harmony" influenced by all
desktop platforms as well as Google's Material Design. However, the minimal API
surface persists; Views lacks many controls and features found in other UI
toolkits and is not supported on mobile.

### `View` (class)

A node in a tree of objects used to represent UI. A `View` has rectangular
bounds, a (possibly-empty) set of child `View`s, and a large set of APIs to
allow interaction and extension. Its primary goal is to abstract user
interactions and present data to the browser for display. Historically,
implementers have built subclasses as heavyweight objects containing all the
code to handle display, business logic, and the underlying data model; this
reduces boilerplate but makes classes difficult to test, reuse, or port to
other platforms. By taking the name literally and making a `View` subclass
handle only the "view" role in the MVC paradigm, implementers can produce more
robust code.

### Widget

The cross-platform abstraction that represents a "window" in the Views toolkit.
It is the gateway between the `Views` library and the OS platform. Each
`Widget` contains exactly one `RootView` object that in turn holds a hierarchy
of `View`s representing the UI in that window. `Widget` is implemented in turns
of a platform-specific `NativeWidget`, which itself may be backed by other
abstractions (at least on Aura). It's possible to have parent/child
relationships between widgets, for example when displaying a small floating
bubble anchored to (and clipped by) a larger window.
