# Content Accessibility Support

An overview of key concepts and features of accessibility (AX) support for a
`//content` browser.

[TOC]

## AXMode flags

[`AXMode`](/ui/accessibility/ax_mode.h) is a bitfield that specifies how much
AX data the browser needs from renderers. The browser's own UI becomes
accessible when the `kNativeAPIs` bit is set (this is typically called "native
accessibility"). The pages displayed in `WebContents` become accessible when the
`kWebContents` bit is set (typically called "web accessibility"). Additional
mode flags increase the information from pages that are made available to an AX
tool (AT). Some mode flags may enable other features in web content (e.g., image
labeling).

## Mode flag relevance

A renderer produces AX data according to the mode flags that it is given by its
`WebContents`. This may contain flags not present in the process-wide mode
(`BrowserAccessibilityState::GetAccessibilityMode()`) due to targeting (see
[Targeted accessibility](#Targeted-accessibility)).

## Enabling process-wide accessibility

Typically, AX processing is enabled for the browser when it detects the presence
of an AT. This is done via platform-specific mechanisms, which may involve
heuristics. A distinction is made between an AT that is definitively a screen
reader vs. some other tool (e.g., a form filler). When a screen reader is
detected, `kExtendedProperties` is added to the browser's `AXMode`. This may be
used as a signal for other components of the browser that need special behavior
when a screen reader is in use.

AX processing is enabled by creation of a `ScopedAccessibilityMode` (SAM)
instance, and stays active throughout the lifetime of said instance. AX
processing is enabled for the entire browser process via creation of a SAM with
one or more mode flags; see
`BrowserAccessibilityState::CreateScopedModeForProcess`. In this case, all
`WebContents` in the process receive the new mode flags (see [Progressive
Accessibility](#Progressive-Accessibility) for more details). A process-wide SAM
is the only way to enable native accessibility, as native accessibility applies
to all native UI components (i.e., Views) in the process.

## Targeted accessibility

A component that requires AX data from a specific `WebContents` uses
`BrowserAccessibilityState::CreateScopedModeForWebContents` to create a SAM
targeting that one `WebContents` (with at least the `kWebContents` mode flag).

A SAM can target all `WebContents` belonging to a particular `BrowserContext` (a
"Profile" in Chrome terms) with
`BrowserAccessibilityState::CreateScopedModeForBrowserContext`. This is most
useful for enabling mode flags associated with a user preference (e.g., image
labeling). Due to filtering performed by `BrowserAccessibilityState`, a
component may speculatively enable mode flags that depend on more fundamental
flags. For example, `kLabelImages` is filtered out if `kExtendedProperties` is
not also present.

## Data flow

A `WebContents` tells its `RenderFrame`s that it wants AX data (e.g., trees and
updates) by sending a set of `AXMode` flags to its
`blink::mojom::RenderAccessibility` interface. These mode flags tell the
renderer how much AX data the browser needs. As explained
[above](#AXMode-flags), the bare minimum mode for web AX data is `kWebContents`.

A `RenderFrame` sends AX data to its `RenderFrameHost` in the browser by way of
`RenderAccessibilityHost`. The frame's `WebContents` and its observers are given
an opportunity to view/modify the AX data before it is given to the
`BrowserAccessibilityManager` for merging into the `WebContents`'s existing
tree.

## Progressive Accessibility

Mode flag changes are not distributed immediately to `WebContents` that are
hidden. Such changes are held by the `WebContents` and only sent to its
renderers when it is unhidden (becomes visible or occluded). One implication of
this is that `WebContents::GetAccessibilityMode()` on an instance that has never
been drawn will return an empty set.

## See also

*   [Accessibility Overview](/docs/accessibility/overview.md)

