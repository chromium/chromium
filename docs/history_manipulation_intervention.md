# History manipulation intervention in Chromium

Reference: [PSA on blink-dev](https://groups.google.com/a/chromium.org/g/blink-dev/c/T8d4_BRb2xQ/m/WSdOiOFcBAAJ)

## Summary
Some pages make it difficult or impossible for the user to use the browser back
button to go back to the page they came from. Pages accomplish this using
redirects or by manipulating the browser history, resulting in an
abusive/annoying user experience.

The history manipulation intervention mitigates such abuse by making the
browser’s back button skip over pages that added history entries or redirected
the user without ever getting a user activation. Note that the intervention only
impacts the browser back/forward buttons and not the `history.back()/forward()`
APIs.

Here’s an example:
1) User is on a.com and clicks to go to b.com
2) b.com adds a history entry using `pushState` or navigates the user to another
page (c.com) without ever getting a user activation.
3) If the user presses back, the browser will skip b.com and go back to a.com
instead.

## Spec
Because this only impacts browser UI, this is allowed by the spec, which only
governs the behavior of `history.back/forward`.
However, it might be good to spec this anyway, so that users get consistent
experiences in all browsers. That work is tracked at
https://github.com/whatwg/html/issues/7832

## Invariants
At a high level, the intervention ensures the back/forward buttons always
navigate to a page the user either navigated to or interacted with. It
guarantees the following invariants:
1. Only back/forward navigations triggered by the back/forward buttons will ever
   skip history entries. This ensures that the history API's behavior is
   unaffected.
2. The intervention marks a history entry as skippable if the document creates
   another history entry without a user activation.
3. If a document receives a user activation (before or after creating history
   entries), its history entry is not skippable. With an activation, the
   document can create many unskippable same-document history entries, until
   either a cross-document navigation or a back/forward occurs. Note that
   same-document back/forwards do not normally reset any prior user activation,
   but the intervention stops honoring such activations for creating new
   entries until a new activation is received, per https://crbug.com/1248529.
4. All same-document history entries will have the same skippable state. When
   marking an entry unskippable after a user activation, this ensures that the
   rest of the document's entries work as well. When marking an entry as
   skippable, this ensures that all entries for the offending document will be
   skipped.
5. Revisiting a skippable history entry does not change its skippable status,
   unless it receives a user activation. This ensures that history.back() will
   not bypass the intervention, per https://crbug.com/1121293.
6. The intervention applies to history entries created by subframes as well. A
   user activation on any frame on the page is sufficient to make the entry
   unskippable, per https://crbug.com/953056.

## Details
1. The intervention works by setting the `should_skip_on_back_forward_ui_`
   member for a `NavigationEntryImpl` object. The member is initially set to
   false, and it is set to true if any document in the page adds a history entry
   without having a user activation.
2. `NavigationController::CanGoBack()` will return false if all entries are
   marked to be skipped on back/forward UI. On desktop this leads to the back
   button being disabled. On Android, pressing the back button will close the
   current tab and a previous tab could be shown as it would normally happen on
   Android when the back button is pressed from the first entry of a tab.
3. The oldest `NavigationEntryImpl` that is marked as skippable is the one
   that is pruned if max entry count is reached.
