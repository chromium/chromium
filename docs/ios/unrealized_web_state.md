# Unrealized `WebState`

> **Status**: launched.

On iOS, each tab is implemented by a `WebState` and a set of `TabHelpers`
and they are stored in a `WebStateList`. As users can have many tabs open
at the same time (some users have over a thousand tabs open), but will
only interact with few of them in a given session. As an optimisation to
reduce the memory pressure, the tab can exist in an incomplete state upon
session restoration.

This incomplete state is called "unrealized". When in the unrealized state,
a `WebState` will not be connected to any WebView. In fact, they will only
store their identifier, their creation time, their last active time, the
title and URL of the visible page, and the number of navigation item. No
TabHelpers can be attached to an "unrealized" `WebState`.

A `WebState` can transition from "unrealized" state to "realized" state
when explicitly calling `ForceRealization(...)` on them or automatically
when a method that cannot be provided in the "unrealized" state (e.g.
trying to access the WebView, or to start a navigation, ...). In general,
code should avoid forcing a `WebState` to transition to the "realized"
state, unless this is unavoidable (e.g. the user is trying to interact
with the tab).

By default, `Browser` will ensure that the `WebState` at the active index
in its `WebStateList` is automatically "realized" upon selection (except
for the "inactive tabs" `Browser` which corresponds to the tabs that have
not been used by the user for other 21 days by default).

Code that act on the "active" tab of a `Browser` can assume that it is
"realized" and thus any method can be called on it, and all its TabHelpers
have been created.

Code that act on arbitrary tabs (e.g. iterate over a `WebStateList`) must
try to avoid forcing the realization of the `WebState`. They can use the
method `WebState::IsRealized()` to check whether a `WebState` is "realized"
and if this is not the case, they should avoid the realization if possible.

As "unrealized" `WebState` do not have a WebView and cannot perform any
navigation, the code should assume that they are displaying the page at
their visible URL with the given title and based their decision on that
if this is enough.

If code wants to perform an action as soon as a `WebState` become "realized"
then can wait for the `WebStateObserver::WebStateRealized()` method to be
called. If they want to do that for all `WebState` owned by a `Browser`, they
can use the `TabsDependencyInstaller` API.

## Features available on "unrealized" WebState

An "unrealized" `WebState` supports the following features:

-   registering and removing Observers
-   registering and removing WebStatePolicyDecider
-   `const` property getters (*)

(*) : all of the `const` property getters can be called on an "unrealized"
`WebState` but they may return a default value (`false`, `nil`, `nullptr`,
empty string, ... if the information cannot be retrieved from the serialised
state).

The following is not supported by "unrealized" `WebState`:

-   retrieving the saved state
-   attaching a TabHelper
-   starting a navigation
-   ...

## When are `WebState` created in "unrealized" state

The `WebState` are usually created in the "unrealized" state when a session
is restored. The reason is that restoring a session may create many `WebState`
when only few of them will be immediately used, while other ways to create a
`WebState` (opening a new tab, preloading, ...) lead to immediate use.

`WebState` can only transition from the "unrealized" to the "realized" state.
They cannot migrate in the other direction. This means that `WebState` in the
"unrealized" state would have been created directly in that state.

The transition to "realized" state does not require any action from the client
of the `WebState`. Only internal state of the `WebState` will be affected. The
observers registered will still be valid, as are the policy decider, the script
callbacks, ... The client code may want to listen to the transition to activate
itself if its behaviour depends on internal objects of the `WebState` (such as
the `NavigationManager`).
