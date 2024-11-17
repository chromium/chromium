# How Chrome Accessibility Works, Part 3

This document explains the technical details behind Chrome accessibility
code by starting at a high level and progressively adding more levels of
detail.

See [Part 1](how_a11y_works.md) and
[Part 2](how_a11y_works_2.md) first.

[TOC]

## Abstracting platform-specific APIs

In [Part 1](how_a11y_works.md) we talked about how each platform has its own
accessibility API. Chromium originally had the platform-specific accessibility
APIs scattered throughout the code, but today a large fraction of the APIs for
Windows (including IAccessible, IAccessible2, and UI Automation), Linux, and
macOS have all been isolated and abstracted in one place that makes it
relatively easy to write cross-platform accessibility code.

These abstractions are all in the
[ui/accessibility/platform](
https://source.chromium.org/chromium/chromium/src/+/main:ui/accessibility/platform/)
directory.

First, gfx::NativeViewAccessible is a typedef used throughout Chromium to
represent an instance of the platform-specific accessible object on the current
platform. It's defined alongside gfx::NativeView, gfx::NativeEvent, and other
similar types that have equivalents on each platform. Note that these are not
wrappers or abstractions; they're just typedefs enabling you to write a function
that returns an instance of the appropriate type on each platform. For
accessibility, gfx::NativeViewAccessible is defined to be IAccessible* on
Windows, id on Mac (where 'id' is the type for a generic Objective-C object,
which has to implement the informal NSAccessibility protocol), and AtkObject* on
Linux.

The main class in ui/accessibility/platform is AXPlatformNode. When you call
AXPlatformNode::Create, you'll get back an object that implements the
correct interfaces for the platform you're running on - currently Windows,
macOS, and desktop Linux are supported.

For each AXPlatformNode, you need to provide an AXPlatformNodeDelegate - an
instance of a class that you implement in order to provide all of the
accessibility details about that node, in a cross-platform way.

While AXPlatformNodeDelegate is pure virtual, a base class is provided,
AXPlatformNodeDelegateBase, with default implementations of nearly all
of the virtual functions. You can inherit from AXPlatformNodeDelegateBase
and override only a few functions in order to easily get a working object.

As a brief sketch, if you had a custom-drawn button and you wanted to
make it accessible, you could define a subclass like this:

```C++
class MyButtonAXPlatformNodeDelegate
    : public AXPlatformNodeDelegateBase {
    MyButtonAXPlatformNodeDelegate()
        : AXPlatformNodeDelegateBase() {
        ...
    }

    const AXNodeData& GetData() const override {
        ...
    }

    int GetChildCount() const override {
        ...
    }

    gfx::NativeViewAccessible ChildAtIndex(int index) const override {
        ...
    }

    gfx::NativeViewAccessible GetParent() const override {
        ...
    }
};
```

Then to construct the accessible object, you could just write this:
```C++
MyButtonAXPlatformNodeDelegate delegate;
AXPlatformNode* accessible = AXPlatformNode::Create(&delegate);
```

## Events

In the Chromium codebase, accessibility events are notifications sent from
the browser to assistive technology that something has happened. This is
the mechanism by which assistive technology can provide real-time feedback
as the user is interacting with the browser. Some common events found on
nearly all platforms include:

* Focus changed
* Control value changed
* Bounding box changed
* Children changed (a node added, removed, or reordered one or more children)
* Load complete (a web page finished loading)

While many platforms share the same types of events, they're not standardized
at all, and platforms have very different names for events and different
semantics around which events are fired when, and where. As a few examples:

* On macOS, there are separate events for expanding and collapsing a row
  in a table or tree, vs expanding or collapsing a pop-up menu
* On Android there's a separate event for the checked state changing, while
  on other platforms, there's just a generic state changed event
* On Windows there are SHOW and HIDE events that need to be fired when
  a node or subtree is created or destroyed

In many cases, assistive technology is co-developed with initial accessibility
support in a platform's native widget toolkit - for example, TalkBack was
co-developed with the accessibility support in Android Views,
and NSAccessibility was co-developed
with AppKit's initial accessibility support. One thing that invariably seems to
happen is that event notifications get added to let assistive technology know
about changes to state of the app.

Then when a new app comes along and needs to do some custom drawing or otherwise
implement some custom accessibility code, implementing those events ends up
being tricky. If the right events aren't fired in exactly the right order, the
assistive technology gets confused, since it was only built and tested with one
event sequence.

This forms an implicit contract between the server and client, but it's one
that's rarely properly documented.

For a cross-platform product like Chromium that needs to support the right
set of events to fire across so many platforms this gets very tricky. In
the early days we tried to have Blink fire the superset of all events needed
on any platform, but this often resulted in duplicate events or subtle
bugs, and a tendency for an event-related fix for one platform to accidentally
break another platform.

Chrome's solution to this now is what we call "implicit" events. Blink, and
other parts of the codebase that build an accessibility tree simply notify that
an accessibility node is dirty, or an entire subtree is dirty. The
infrastructure crawls the dirty nodes and creates a tree mutation and propagates
it to all client interfaces.

At the level of the client interface, we generate implicit events based on
changes to the accessibility tree as observed from that client's perspective,
using a class called AXEventGenerator.

This allows us to keep the code that implements a particular contract in one
place and eliminate subtle differences between different types of content.

### AXEventGenerator

AXEventGenerator is based on the idea of applying atomic updates to an
accessibility tree. As described in
[Data structures used by the accessibility cache](
how_a11y_works_2.md#Data-structures-used-by-the-accessibility-cache)
in part 2, an AXTree is a "live tree" that's currently being served,
and an AXTreeUpdate is a serializable data structure that represents
either a snapshot of a tree or an atomic update to apply to an existing
tree. When AXTree applies an atomic AXTreeUpdate, it allows listeners to
get callbacks for any changes that happened to the tree. In particular,
it keeps both the old and new data for each changing node temporarily
so that listeners can trigger actions based on changes.

AXEventGenerator is thus an AXTree listener. It considers every node
that changed in the tree and figures out what events to fire. It builds
up the set of events and continues modifying it until the atomic update
is finished, enabling it to consolidate and remove duplication.

As one example of that, a live region is a portion of a web page that
may trigger assistive technology to notify whenever an update occurs.
On some platforms, Chromium needs to fire a "live region changed"
announcement on the root of the live region whenever it changes.
AXEventGenerator keeps track of any changes that happen within a live
region and ensures that exactly one "live region changed" event is
fired on the live region root.

There are a small number of exceptions - events that can't be fired
via AXEventGenerator. These are things that can't be inferred just
from tree changes. One such example is the "autocorrection occurred"
event. When the browser performs an autocorrection while the user is
typing, the state change just looks like any other edit. The event
ensures assistive technology can announce the autocorrection.

### Focus events

Focus events are one of the most important types of events, because
changing focus is often one of the most important events for assistive
technology to announce, and the focused node is the one that will be
the target of any input events.

However, one of the challenges with focus events is that there's only
one element on the entire desktop that has focus at any one time, but
individual windows or iframes might not always be aware of the global
state of the entire desktop at the time they experience a focus change
within their scope. This can lead to a race condition.

As an example, suppose that a user clicks a button in a web page, which after a
couple of seconds pops up a dialog and brings focus to an OK button.  At the
same time, the user clicks on a different window to activate it, moving
focus to that window's active element.

Because the windows come from different processes, the two focus events
(from the first window's dialog, and from the second window's active element)
could arrive at the browser process in either order. Here's an illustration
of this race condition:

![This diagram illustrates a race condition where the user clicks a button
to open a dialog in one window, then before it opens activates another
window that focuses a text field. The focus events could arrive to the
browser process in either order.](
figures/focus_race.png)

From the standpoint of the browser, there's always only one node that has
focus. What's important here is that accessibility is completely consistent
with the browser in terms of reporting the correct node that has focus.

The solution here is that only the browser is the source of truth when it
comes to which window has focus. Once we know which window has focus, each
accessibility tree tells us which node has focus within that tree.

As a result, when a focus change happens in an AXTree, we can't just fire
a platform-specific focus event directly. Instead, we use that as a cue to
compute global focus and fire an update if needed. Here's an outline of the
algorithm:

* Anytime focus changes in any accessibility tree, OR when the focused
  window or iframe changes, recompute the focus.
* To compute focus, start with the focused window (or active window,
  depending on the platform). If focus is in web content,
  see what node is focused there. If that node is an iframe, recursively
  jump into that iframe to see what's focus.
* Take the resulting deepest focused node and compare it to the last focused
  node we computed. If it's different, fire a platform-specific accessibility
  focus event.

This ensures that accessibility focus events are always reliable and in sync.

No other accessibility events have the same issue. Events like value changed,
selection changed, etc., are safe to fire even if a window is in the
background. Some assistive technology may be paying attention to background
windows.

## Actions

In Chromium accessibility terminology, Actions flow the opposite direction from
events. Actions are when assistive technology wants to modify or interact with
the app on behalf of the user, such as clicking a button, selecting text, or
changing a control value.

Note that screen readers rely very heavily on events, and partially on
actions. Users often use a combination of accessibility actions along with the
keyboard to directly drive an application, or have the screen reader warp the
mouse cursor directly to an element and simulate a click on that element.

In contrast, assistive technology such as voice control makes heavy use of
actions and relies much less on events. Voice control relies heavily on
actions that enable directly changing control values, entering text,
activating buttons and links, and scrolling the page.

Other assistive technology such as magnifiers are in-between - they may
follow focus events a lot but make heavy use of scroll actions.

For the most part implementing actions is relatively straightforward.
The action is received by the part of the code that implements the
platform-specific accessibility APIs. It forwards the action to the
corresponding accessibility wrapper node in Blink, and that node
calls the appropriate internal APIs to directly manipulate the underlying
element, such as clicking a button or changing the value of a control.

One minor complication is that on many platforms, actions are supposed to
return a success/failure code. Since actions are obviously implemented
asynchronously, Chromium can't know for sure if an action succeeded, so it
has to return success if an action seems valid, even though there's a
chance it might not actually succeed.

## Hit testing

One specific special case of an action is a hit test. This is an API where
the assistive technology gives the x, y coordinates of a location on the
screen and asks the application (Chromium) to return which accessible
object is at that location.

Applications of hit testing include:

* Touch exploration on a touch-screen, or features to describe the
  element as you're hovering over it with the mouse
* Using accessibility debuggers where you can click on an element and
  get its accessibility properties

Unfortunately on some platforms a hit test is a synchronous API. This is
a challenge because it's difficult to properly compute the correct element
at a location given just the accessibility tree, but blocking to wait for
a proper hit test in the render process can lead to deadlock and jankiness.
So Chromium employs the following approach:

* The first time a hit test is received, it does an approximate hit test
  based on the bounding boxes in the accessibility tree. This often returns
  the correct result, but could fail in cases of complex layering or
  non-rectangular objects.
* Subsequently, it makes an async call to the render process to do a proper
  hit test and get the correct resulting element, and also the visible
  bounding box of that element.
* The next hit test that's received, if the coordinates are within the
  bounding box of the most recent proper hit test result, it returns that
  result, which is correct. If the coordinates are outside of that bounding
  box, go back to the first step.

This algorithm works very well in practice when the user is moving the mouse
or dragging their finger across the screen, because we get dozens of hit
tests per second. At the edges of objects, the wrong result may be returned
for a few milliseconds, but as soon as the async result comes back, the
correct result is then returned.

So for interactive use, it's quite seamless and reliable for users, while
still providing reasonable behavior in the less common circumstances where
a single hit test is called.

## Relative coordinates

Up until now we've hinted about the fact that every node in the accessibility
tree stores a bounding box, but we haven't gone into much detail as to
how that bounding box is stored.

If we always stored the bounding box in screen coordinates, then every time
a window is dragged or scrolled, or any time any part of the page moves or
scrolls, all of the affected bounding boxes would need to be recomputed,
which would involve a lot of recomputation and sending information from
render processes to the browser process.

To minimize that work, in Chromium accessibility nodes store relative
coordinates.

In particular, every node stores the following fields in a struct
called AXRelativeBounds:

```C++
struct AX_BASE_EXPORT AXRelativeBounds final {
  int offset_container_id;
  Rect bounds;
  Optional<Transform> transform;
};
```

The first field is the ID of the node's container, which can be any ancestor of
a node. That's the node that the bounds are relative to.

The next field is the local bounding rect, relative to that container.

The last field is an optional 4x4 transformation matrix, which can be
used to encode things like scale factors or even 3-D rotations. If this
concept is unfamiliar to you, search for tutorials on 4x4 transformation
matrices in the context of 3-D computer graphics.

Computing the global bounding rect of a node is meant to be straightforward.
Start with the local rect. As long as the node isn't the root, keep walking
to the container node, applying the transformation matrix and adding the
bounds origin as you go.

In addition, there are a couple of other fields relevant to the bounds
computation that are stored as sparse attributes in AXNodeData. These also
affect the bounds computation.

* bool clips_children;
* int x_scroll_offset;
* int y_scroll_offset;

For more information on bounding boxes, clipping, and offscreen, see
[Offscreen, Invisible and Size](offscreen.md).

## Text bounding boxes

Most platform-specific accessibility APIs have a number of features
specifically to deal with text. Some of those APIs allow querying the
bounding box of an arbitrary range of text - often the text caret or
selection, but not necessarily. Applications include:

* Highlighting text as it's read aloud
* Scrolling one particular text range into view
* Drawing highlights around the caret or selection to make it easier
  for users to see them

Because these APIs are synchronous, they must be served directly out of the
accessibility cache. That means that the accessibility cache needs to have
enough information to be able to retrieve the bounding box of any arbitrary
range of text on-screen.

It would require quite a bit of memory to store the bounding box of
every individual character. To save memory, the following representation
is used:

In the accessibility tree, we keep track of text nodes called "inline text
boxes". This corresponds to a similar concept in Blink, which is also
sometimes called a "text run". The idea is that given a single text node,
the text can be broken down into a sequence of text runs that each have
the following properties:

* Each text run is on a single line
* Text goes a single direction (left-to-right, for example)
* The characters in that text run are all contiguous

In the most common scenario, a single text node contains multiple
lines of text (potentially due to automatic wrapping with soft
line breaks). In the accessibility tree that node would have multiple
inline text box children, one for each line.

Imagine we have the following paragraph, that's very narrow so it
wraps as follows:

```
The quick brown fox
jumps over the
lazy dog.
```

In the accessibility tree, it might be represented like this:

```
Paragraph
    Static Text "The quick brown fox jumps over the lazy dog."
        Inline text box "The quick brown fox "
        Inline text box "jumps over the "
        Inline text box "lazy dog."
```

Each inline text box comes with its own bounding box and text direction.
Then, to store the bounding box of every character, all we need to
do is store the width of each character. Since we know all of the
characters are written continuously in a line going the same direction,
we can use the bounds of the inline text box and the width of each
character to compute the bounding box of any individual character.

The AXPosition class abstracts most of this computation.

## Iframes

The last piece of complexity to address is that up until now we've
assumed that a single web page corresponds to a single frame, so a
web page is a single process.

In Chromium, for security reasons iframes can also be running in
separate processes. This isn't always the case - for one thing, if
system resources are low, Chromium won't keep creating new processes,
and also, frames from the same origin (i.e. from the same website)
need to be in the same process so they can communicate synchronously
via JavaScript. But, frames from different sites can be in different
processes so accessibility code needs to deal with that.

The essential challenge is that each frame, which may be in its own
process, needs to maintain an accessibility tree - but the end result
needs to be stitched together into a final resulting accessibility
tree in the browser process. Iframes are mostly just an implementation
detail; users and assistive technology are rarely concerned with this
detail.

In order to stitch frames together:

* Each accessibility tree gets a globally unique ID, we call it an AXTreeID.
  For security reasons this is an UnguessableToken.
* An iframe element in an accessibility tree contains the AXTreeID of its
  child frame.
* In the browser process, we keep a hash map of all of the trees, and
  also cache the reverse direction (e.g. the map from the root of a
  tree to its parent node).

In order to reduce complexity, Chromium accessibility is built around the
concept that every frame is its own accessibility tree, no matter whether
the frame is in a different process or not. The advantage of this approach
is that the same codepath is used whether iframes are in the same process
or a remote process. If iframes break, they all break - that simplifies
testing and reduces the number of cases to consider.

The concept of embedding one accessibility tree in another using an
AXTreeID is also exploited even more in Chrome OS accessibility, where
it's used to embed Android applications and more.
