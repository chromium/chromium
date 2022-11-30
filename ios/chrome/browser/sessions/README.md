## Session saving and restoration.

A *Session* is the data Chrome saves to an iOS device's storage that preserves
the state of open tabs in a given Browser. On a device that only supports a 
single Chrome window, there will be one saved session for the regular tabs,
and one saved session for the Incognito tabs. On devices that support multiple
windows, there will be one or two (two if there are Incognito tabs) saved 
sessions for each window. 

Sessions are the mechanism by which a user's open tabs are restored when they
laucnh Chrome. Given that iOS can terminate Chrome in the background at any 
time -- and can disconnect individual scenes when there are multiple windows --
it's critical that sessions are saved and restored in a way that is quick and
lossless for the user.

(Don't confuse this Chrome-specific sense of *session* with the UIKit concept 
of a *scene session*, implemented in the `UISceneSession` class. While both are
concerned with persisting application state data, they are not interchangeable,
and Chrome's sessions are not implemented using UISceneSessions for storage.)

### Session saving

The design intent is that sessions are saved before Chrome enters a state where
it might be suddenly terminated. 

Saving a session is done by means of the `SessionRestorationBrowserAgent` for
the browser being saved, using the `SaveSession()` method. Calling this saves
the session for the browser the agent is attached to.

<!-- Add details on how sessions are saved; this depends on if the iOS15 APIs
for saving WKWebView interactionState are being used or not. Also describe
how per-window session saving is done. -->

`SaveSession()` is called in the following circumstances:
1. Whenever a sene moves to the background, `SaveSession()` is called on its 
   browsers. 
2. When the UI for a scene is destroyed (when the scene's `SceneController` is 
   shutting down), `SaveSession()` is called if it hasn't been called aready.
3. When a tab is inserted, removed, activated, replaced, re-ordered, or 
   completes a navigation, `SaveSession()` on the Browser that owns its
   WebStateList.
4. When a prerendered tab is loaded, replacing an existing webState, 
   `SaveSession()` on the browser whose WebStateList contains the new tab.

Case (1) is handled by `SessionSavingSceneAgent`, which watches for scene state
changes and tracks if the current scene has been foregrounded since the last 
time it was saved. Such scenes are marked as needing to have their sessions 
saved the next time they background.

Case (2) is handled by `SceneController` itself when it shuts down; it in turn
asks its `SessionSavingSceneAgent` to save its sessions if needed.

Case (3) is handled by `SessionRestorationBrowserAgent` via WebState and 
WebStateList observation.

Case (4) is handled by the `PrerenderService`; it directly calls 
`SaveSession()`, regardless of whether the session is in the background.

Cases (3) and (4) save after a short delay, and repeated session saves within
that delay are ignored. Cases (1) and (2) save immediately (canceling any 
pending delayed saves). `SessionServiceIOS` handles this. 

### Session Restoration

TODO: Describe when sessions are restored.

### Session Recovery

TODO: Describe the logic for post-crash session recovery and how sessions are
set aside and (possibly) discarded.

### Incognito Sessions

TODO: Describe how Incognito sessions are stored, and how it differs from how
regular session are.
