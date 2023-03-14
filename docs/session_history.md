# Session History

A browser's session history keeps track of the navigations in each tab, to
support back/forward navigations and session restore. This is in contrast to
"history" (e.g., `chrome://history`), which tracks the main frame URLs the user
has visited in any tab for the lifetime of a profile.

Chromium tracks the session history of each tab in NavigationController, using a
list of NavigationEntry objects to represent the joint session history items.
Each frame creates _session history items_ as it navigates. A _joint session
history item_ contains the state of each frame of a page at a given point in
time, including things like URL, partially entered form data, scroll position,
etc. Each NavigationEntry uses a tree of FrameNavigationEntries to track this
state.

[TOC]


## Pruning Forward Navigations

If the user goes back and then commits a new navigation, this essentially forks
the joint session history. However, joint session history is tracked as a list
and not as a tree, so the previous forward history is "pruned" and forgotten.
This pruning is performed for all new navigations, unless they commit with
replacement.


## Subframe Navigations

When the first commit occurs within a new subframe of a document, it becomes
part of the existing joint session history item (which we refer to as an "auto
subframe navigation"). The user can't go back to the state before the frame
committed. Any subsequent navigations in the subframe create new joint session
history items (which we refer to as "manual subframe navigations"), such that
clicking back goes back within the subframe.


## Navigating with Replacement

Some types of navigations can replace the previously committed joint session
history item for a frame, rather than creating a new item. These include:

 * `location.replace` (which is usually cross-document, unless it is a fragment
   navigation)
 * `history.replaceState` (which is always same-document)
 * Client redirects
 * The first non-blank URL after the initial empty document (unless the frame
   was explicitly created with `about:blank` as the URL).


## Identifying Same- and Cross-Document Navigations

Each FrameNavigationEntry contains both an _item sequence number_ (ISN) and a
_document sequence number_ (DSN). Same-document navigations create a new session
history item without changing the document, and thus have a new ISN but the same
DSN. Cross-document navigations create a new ISN and DSN. NavigationController
uses these ISNs and DSNs when deciding which frames need to be navigated during
a session history navigation, using a recursive frame tree walk in
`FindFramesToNavigate`.


## Classifying Navigations

Much of the complexity in NavigationController comes from the bookkeeping needed
to track the various types of navigations as they commit (e.g., same-document vs
cross-document, main frame vs subframe, with or without replacement, etc). These
types may lead to different outcomes for whether a new NavigationEntry is
created, whether an existing one is updated vs replaced, and what events are
exposed to observers. This is handled by `ClassifyNavigation`, which determines
which `RendererDidNavigate` helper methods are used when a navigation commits.


## Persistence

The joint session history of a tab is persisted so that tabs can be restored
(e.g., between Chromium restarts, after closing a tab, or on another device).
This requires serializing the state in each NavigationEntry and its tree of
FrameNavigationEntries, using a PageState object and other metadata.
See [Modifying Session History Serialization](modifying_session_history_serialization.md)
for how to safely add new values to be saved and restored.

Not everything in NavigationEntry is persisted. All data members of
NavigationEntryImpl and FrameNavigationEntry should be documented with whether
they are preserved after commit and whether they need to be persisted.

Note that the session history of a tab can also be cloned when duplicating a
tab, or when doing a back/forward/reload navigation in a new tab (such as when
middle-clicking the back/forward/reload button). This involves direct clones of
NavigationEntries rather than persisting and restoring.

## Invariants

 * The `pending_entry_index_` is either -1 or an index into `entries_`. If
   `pending_entry_` is defined and `pending_entry_index_` is -1, then it is a
   new navigation. If `pending_entry_index_` is a valid index into `entries_`,
   then `pending_entry_` must point to that entry and it is a session history
   navigation.
 * Newly created tabs have NavigationControllers with `is_initial_navigation_`
   set to true. They can have `last_committed_entry_index_` defined before the
   first commit, however, when session history is cloned from another tab. (In
   this case, `pending_entry_index_` indicates which entry is going to be
   restored during the initial navigation.)
 * Every FrameNavigationEntry that has committed in the current session (as
   opposed to those that have been restored) must have a SiteInstance.
 * A renderer process can only update FrameNavigationEntries belonging to a
   SiteInstance in that process. This especially includes attacker-controlled
   data like PageState, which could be dangerous to load into a different
   site's process.
 * Any cross-SiteInstance navigation should result in a new NavigationEntry
   with replacement, rather than updating an existing NavigationEntry.


## Caveats

 * Not every NavigationRequest has a pending NavigationEntry. For example,
   subframe navigations do not, and renderer-initiated main frame navigations
   may clear an existing browser-initiated pending NavigationEntry (using
   PendingEntryRef) without replacing it with a new one.
 * Some subframe documents may not have a corresponding FrameNavigationEntry
   after commit (e.g., see [issue 608402](https://crbug.com/608402)).
