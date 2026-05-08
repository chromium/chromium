# History manipulation intervention in Chromium

Reference: [PSA on blink-dev](https://groups.google.com/a/chromium.org/g/blink-dev/c/T8d4_BRb2xQ/m/WSdOiOFcBAAJ)

## Summary

Some pages make it difficult or impossible for the user to use the browser back
button to go back to the page they came from. Pages accomplish this using
redirects or by manipulating the browser history, resulting in an abusive or
annoying user experience.

The history manipulation intervention addresses this by targeting two abuse
patterns:

### 1. Original history manipulation intervention

The original history manipulation intervention makes the browser’s back button
skip over pages that added history entries or redirected the user without ever
getting a user activation.

**Example:**

1.  User is on `a.com` and clicks to go to `b.com`.
2.  `b.com` adds a history entry using `pushState` or navigates the user to
    another page (`c.com`) without ever getting a user activation.
3.  If the user presses back, the browser will skip `b.com` and go back to
    `a.com` instead.

### 2. Back-to-ad intervention

The back-to-ad intervention makes the browser’s back button skip over ad-related
pages that were silently inserted into session history, even if the page does
have a user activation. This is currently disabled by default and gated by the
`BackToAdIntervention` feature flag.

**Background:**

A script can be ad-tagged by [Chrome's Ad Tagging Infrastructure](https://chromium.googlesource.com/chromium/src/+/lkgr/docs/ad_tagging.md).

**Example:**

1.  User is on `a.com` and clicks to go to `b.com`.
2.  A user activation is granted to `b.com`.
3.  An ad script on `b.com` invokes `replaceState` (creating entry `b1`) and
    subsequently `pushState` (creating entry `b2`). The page appearance does not
    change.
4.  If the user presses back, the first same-document entry (`b1`) will be
    skipped because it was caused by an ad, and the browser will go back to
    `a.com`.

The heuristic assumes that if a session history entry is both created by an ad
script (e.g., target of `replaceState`) AND subsequently creates a new entry
(e.g., initiator of `pushState`), it is an ad-tagged entry. If the user
navigates back to such an entry, the ad script logic has a chance to inject a
new ad into the user's session history.

The back-to-ad intervention has one exception to limit it to cross-origin
navigations, which is described in greater detail
[below](#same-origin-exception).

## Spec

The intervention only impacts the browser back/forward buttons and not the
`history.back()/forward()` APIs. As a result, this is allowed by the spec, which
only governs the behavior of `history.back/forward`. However, it might be good
to spec this anyway, so that users get consistent experiences in all browsers.
That work is tracked at https://github.com/whatwg/html/issues/7832.

## Details

### Common Invariants
At a high level, the intervention ensures the back/forward buttons always
navigate to a page the user either navigated to or interacted with. It
guarantees the following invariants:
1. Only back/forward navigations triggered by the back/forward buttons will ever
   skip history entries. This ensures that the history API's behavior is
   unaffected.
2. Revisiting a skippable history entry does not change its skippable status,
   unless it receives a user activation. This ensures that history.back() will
   not bypass the intervention, per https://crbug.com/1121293.

### NavigationEntry tagging

#### 1. NavigationEntry tagging for original history manipulation intervention

The original history manipulation intervention tags history items (i.e.,
`NavigationEntryImpl` objects) as skippable using the
`should_skip_on_back_forward_ui_` member.

**State setting rules:**

* `should_skip_on_back_forward_ui_` is set to `false` by default.
* When a document adds a history entry without having a user activation, the
    `should_skip_on_back_forward_ui_` for all same-document history entries will
    be set to `true`.
* When a document receives a user gesture, the
    `should_skip_on_back_forward_ui_` for all same-document history entries will
    be set to `false`.

**Additional Logic:**

* The rule applies to history entries created by subframes as well. A user
    activation on any frame on the page is sufficient to set
    `should_skip_on_back_forward_ui_` to `false`.
* The oldest `NavigationEntryImpl` marked as `should_skip_on_back_forward_ui_`
    is the one pruned if the max entry count is reached.
* When a navigation entry is deemed skippable,
    `NavigationControllerImpl::SetSkippableForSameDocumentEntries()` is called
    and logs the skipped entry to the DevTools Issues Panel along with an
    explanatory message.

**Invariants for Original Intervention:**
1. A history entry is marked `should_skip_on_back_forward_ui_` if the document
   creates another history entry without a user activation.
2. If a document receives a user activation (before or after creating history
   entries), its history entry is not marked `should_skip_on_back_forward_ui_`.
   With an activation, the document can create many unskippable same-document
   history entries, until either a cross-document navigation or a back/forward
   occurs. Note that same-document back/forwards do not normally reset any prior
   user activation, but the intervention stops honoring such activations for
   creating new entries until a new activation is received, per
   https://crbug.com/1248529.
3. All same-document history entries will have the same
   `should_skip_on_back_forward_ui_` state. When marking an entry unskippable
   after a user activation, this ensures that the rest of the document's entries
   work as well. When marking an entry as skippable, this ensures that all
   entries for the offending document will be skipped.
4. The tagging applies to history entries created by subframes as well. A user
   activation on any frame on the page is sufficient to set the entry's
   `should_skip_on_back_forward_ui_` state to `false`, per
   https://crbug.com/953056.

#### 2. NavigationEntry tagging for back-to-ad intervention

The back-to-ad intervention tags history items (i.e., `NavigationEntryImpl`
objects) as skippable using the `is_entry_created_by_ad_` and
`is_ad_entry_creator_` members.

**State setting rules:**

* `is_entry_created_by_ad_` and `is_ad_entry_creator_` are set to `false` by
    default.
* When a same-document entry is added to session history (via `pushState()`,
    `replaceState()`, or fragment navigation), the new entry's
    `is_entry_created_by_ad_` state is set to `true` if the operation was
    performed by an ad script.
* When a `NavigationEntry` is active and the one of its documents creates
    another same-document entry (via `pushState()` or fragment navigation), the
    initiator's `is_ad_entry_creator_` state is set to `true` if the operation
    was performed by an ad script.

**Additional Logic:**

* A `NavigationEntry`'s tagging is affected by both subframe and same-document
    navigations.
* An entry where both `is_entry_created_by_ad_` and `is_ad_entry_creator_` are
    true is called an `is_possibly_skippable_ad_entry`.
* If the browser detects that a back UI navigation would skip an
    `is_possibly_skippable_ad_entry`, it proactively logs an explanatory message
    to the DevTools Issues Panel.

**Invariants for Back-to-ad Intervention:**
1. Tagging for the back-to-ad intervention operates independently of the
   original intervention.
2. Once the `is_entry_created_by_ad_` member is set to `true`, it cannot be
   unset.
3. If there is no user interaction, the back-to-ad intervention will only skip
   entries that the original intervention would have skipped (see
   [Intervention Logic](#intervention-logic) below).

### Intervention Logic

Upon back/forward button navigation, the browser will skip over entries that are
either marked as `should_skip_on_back_forward_ui_` (Original) or as an
`is_possibly_skippable_ad_entry` (Back-to-Ad).

<a id="same-origin-exception"></a>
**Same-origin exception to back-to-ad intervention:**

There is one exception to this behavior: if the calculated skip includes at
least one `is_possibly_skippable_ad_entry` and would land the user on a
same-origin page relative to the page where the navigation started, then the
back-to-ad skipping logic is bypassed. In this scenario, the browser only
executes the original intervention logic (i.e., skipping entries marked
`should_skip_on_back_forward_ui_`).

*Rationale:* We limit the scope of the intervention to known abuse (i.e.,
showing an ad on back-button press when the user tries to leave the origin). We
are currently lenient regarding potential back-to-ad abuse when the user
navigates within the same origin. If same-origin back-button abuse arises in the
future, we can revisit or remove this exception.

**UI Behavior:**
* `NavigationController::CanGoBack()` will return false if all entries are
    marked to be skipped on back/forward UI.
* On Android, pressing the back button will close the current tab, and a
    previous tab could be shown (as would normally happen on Android when the
    back button is pressed from the first entry of a tab).
* On desktop, the back button will be enabled in the browser UI, but clicking
    on it will do nothing. This allows a user to long-press the button and
    navigate to a skippable entry explicitly, while still protecting against the
    annoying/abusive experiences this intervention is intended for.
* For additional context, see `NavigationController::ShouldEnableBackButton()`
    and https://crbug.com/339188522.
    * This behavior is mirrored for the forward button as well. See
        `NavigationController::CanGoForward()` and
        `NavigationController::ShouldEnableForwardButton()` for details.
