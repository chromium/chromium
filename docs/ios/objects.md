
# ApplicationContext

ApplicationContext is a global singleton. It gives access to other global
objects and settings that are shared by all user sessions. Those settings
are called "local settings" as they are not synchronised and only affect
the current device (as the settings are shared by all sessions there is
no account to sync them to).

The corresponding object on desktop is BrowserProcess.

# ChromeBrowserState

ChromeBrowserState objects correspond to a user browsing session. There is
one for the off-the-record session, and one per user created in the UI (at
the time this document is written, there is only one user on iOS so at most
there are two ChromeBrowserStates).

The ChromeBrowserState objects are owned by the ProfileManagerIOS that can be
accessed via the ApplicationContext. It is possible to access the off-the-record
ChromeBrowserState from a non-incognito instance.

Each ChromeBrowserState, including the off-the-record ChromeBrowserState,
have a directory used to store some state (current session, settings, ...).
The settings may be synchronised if the user has logged in and has enabled
the synchronisation (thus they are non-local).

The off-the-record ChromeBrowserState needs to record some state because the
application can be killed at any time when the application is in the background
and the state needs to be persisted as this termination should be transparent to
the user. The state is deleted when the last off-the-record tab is closed
and the off-the-record ChromeBrowserState is deleted.

The ChromeBrowserStates support attaching base::SupportsUserData::Data
objects to tie some objects to the ChromeBrowserState lifetime. Check the
documentation of base::SupportsUserData for more information.

A special case of base::SupportsUserData::Data is the KeyedService. They
are managed by the BrowserStateKeyedServiceFactory infrastructure. This
infrastructure allows to declare dependencies between services and ensure
that they are created and destroyed in an order compatible with those
dependencies.

It should never be required to extend ChromeBrowserState. Instead consider
adding a preference to the settings, a base::SupportsUserData::Data if the
change is just to add some data or a KeyedService if behaviour needs to be
added.

The corresponding object on desktop is Profile.

# BrowserList

BrowserList is a container owning Browser instances. It is owned by the
ChromeBrowserState and each ChromeBrowserState has one associated
BrowserList.

The BrowserList owns the WebStateListDelegate that is passed to all the
created Browsers (and then forwarded to their WebStateList).

The corresponding object on desktop is BrowserList but the API is
different. On desktop, it is a singleton and it points to all the
Browsers instances whereas on iOS there is one per ChromeBrowserState.

# Browser

Browser is the model for a window containing multiple tabs. Currently
on iOS there is only one window per ChromeBrowserState, thus there is
a single Browser per BrowserList.

The Browser owns a WebStateList and thus indirectly owns all the tabs
(aka WebState and their associated tab helpers). The Browser also owns
the CommandDispatcher and ChromeBroadcaster used for dispatching UI
commands and property synchronisation.

The corresponding object on desktop is Browser.

# WebStateList

WebStateList represents a list of WebStates. It maintains a notion of
an active WebState and opener-opened relationship between WebStates.

The WebStateList exposes an API to observe modification to the list of
WebStates (additions, moves, removals, replacements).

The WebStateList also has a delegate that is invoked when a WebState
is added to or removed from the WebStateList. This is used to attach
all necessary tab helpers to the WebState.

The corresponding object on desktop is TabStripModel.

# WebStateListDelegate

WebStateListDelegate is the delegate for WebStateList. It is invoked
before a WebState is inserted to the WebStateList.

Each WebStateList points to a WebStateListDelegate but does not own
it to allow sharing the same delegate for multiple WebStateList. In
general, the WebStateListDelegate role is to attach tab helpers to
the WebState when it is added to the WebStateList (and optionally to
shut them down).

The corresponding object on desktop is TabStripModelDelegate.

# WebState

WebState wraps a WKWebView and allows navigation. A WebState can have
many tab helpers attached. A WebState in a WebStateList corresponds to
an open tab and the corresponding tab helpers can be assumed to be
created.

As the tab helpers are only added to a WebState upon insertion into a
WebStateList (or in a limited number of code paths), code should not
assume that a tab helper will be available when using a WebState but
instead should support the tab helper being unavailable.

A WebState and all the attached tab helpers are sometimes called a
tab because they implement what the user sees and interacts with in
a browser tab.

The corresponding object on desktop is WebContents.
