# Unrealized `WebState`

> **Status**: launched.

On iOS, each tab is implemented by a `WebState` and some TabHelpers. As users
can have many tabs open at the same time, but only few of them visible, an
optimisation to reduce the memory pressure is to allow `WebState`s to exist
in an incomplete state upon session restoration.

This incomplete state is called "unrealized". When in the unrealized state,
a `WebState` will not have a corresponding WKWebView, nor any of the objects
that implement navigation (such as NavigationManager, ...).

WebState can transition from "unrealized" to "realized" either lazily when
the client code request a functionality that cannot be provided in that
state (such as accessing the NavigationManager, displaying, ...) or it can
be forced by calling the `WebState::ForceRealized()` method. This is a one
way transition, it is not possible for a "realized" WebState to get back
into the "unrealized" state (at least in the initial implementation).

To avoid unnecessary transition to the "realized" state , the
`WebState::IsRealized()` method can be called. This can be used by TabHelpers
to delay their initialisation until the `WebState` become "realized". To be
informed of the transition, they can listen for the
`WebStateObserver::WebStateRealized()` event which will be invoked upon
transition of the `WebState` from "unrealized" to "realized".

## Features available on "unrealized" WebState

An "unrealized" `WebState` supports the following features:

-   registering and removing Observers
-   registering and removing WebStatePolicyDecider
-   `const` property getters (*)
-   retrieving saved state (**)
-   attaching tab helpers

(*) : all of the `const` property getters can be called on an "unrealized"
`WebState` but they may return a default value (`false`, `nil`, `nullptr`,
empty string, ... if the information cannot be retrieved from the serialised
state).

(**): retrieving the saved state is supported to save the session (as the
code to save the session currently needs the state of all `WebState`s).

## When are `WebState` created in "unrealized" state

The `WebState` are usually created in the "unrealized" state when a session
is restored. The reason is that restoring a session may create many `WebState`
when only few of them will be immediately used, while other ways to create a
`WebState` (opening a new tab, preloading, ...) lead to immediate use.

As seen previously, `WebState` can only transition from the "unrealized" to
the "realized" state. This means that `WebState` in the "unrealized" state
would have been created directly in that state.

The `WebState` are not necessarily created in the "unrealized" state upon
session restoration. This is controlled by the `enable_unrealized_web_states`
gn variable (compilation) and the `#lazily-create-web-state-on-restoration`
flag (runtime).

The transition to "realized" state does not require any action from the client
of the `WebState`. Only internal state of the `WebState` will be affected. The
observers registered will still be valid, as are the policy decider, the script
callbacks, ... The client code may want to listen to the transition to activate
itself if its behaviour depends on internal objects of the `WebState` (such as
the `NavigationManager`).

## Example of `FindTabHelper`

`FindTabHelper` is a TabHelper that implements the "find in page" feature. It
wants to create a `FindInPageController` which needs a "realized" `WebState`.
To support "unrealized" `WebState`, the creation of the `FindInPageController`
is delayed until the `WebState` transitions to "realized" state.

This is done in the following way:

```cpp
FindTabHelper::FindTabHelper(web::WebState* web_state) {
  DCHECK(web_state);
  web_state_observation_.Observe(web_state);
  if (web_state->IsRealized()) {
    CreateFindInPageController(web_state);
  }
}

void FindTabHelper::SetResponseDelegate(
    id<FindInPageResponseDelegate> response_delegate) {
  if (!_controller) {
    response_delegate_ = response_delegate;
  } else {
    controller_.responseDelegate = response_delegate;
  }
}

bool FindTabHelper::CurrentPageSupportsFindInPage() const {
  // As sending a message to `nil` returns the default value for a type
  // (`false` for `bool`), it is not needed to check `controller_` first.
  return [controller_ canFindInPage];
}

// Other FindTabHelper methods that implement the "find in page" feature.

void FindTabHelper::CreateFindInPageController(web::WebState* web_state) {
  DCHECK(!controller_);
  DCHECK(web_state->IsRealized());
  controller_ = [[FindInPageController alloc] initWithWebState:web_state];
  if (response_delegate_) {
    controller_.responseDelegate = response_delegate_;
    response_delegate_ = nil;
  }
}

void FindTabHelper::WebStateRealized(web::WebState* web_state) {
  CreateFindInPageController(web_state);
}
```
