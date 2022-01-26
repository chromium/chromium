# App history ordering tests

These are meant to test the ordering between various events and promises; they
don't fit into any particular sibling directory.

Some of them test simple cases rather-exhaustively, and others test tricky cases
(e.g. reentrancy or navigations aborting previous navigations) in a more focused
way.

Note:

* Variants specifically exist for `currentchange` because an event listener
  existing for `currentchange` causes code to run, and thus microtasks to run,
  at a very specific point in the navigation-commit lifecycle. We want to test
  that it doesn't impact the ordering.
* Similarly we test that `transitionWhile(Promise.resolve())` does not change
  the ordering compared to no `transitionWhile()` call, for same-document
  navigations.

TODOs:

* Also test `appHistory.transition.finished` when it is implemented.
* Also test `popstate` and `hashchange` once
  <https://github.com/whatwg/html/issues/1792> is fixed.
