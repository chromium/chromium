# Chrome Mac Team Triage Process

This document outlines how the Chrome Mac Team triages bugs. Triage is the
process of converting raw bug reports filed by developers or users into
actionable, prioritized tasks assigned to engineers.

*** promo
If you are not on the Chrome Mac Team and want to send a bug to them for triage,
please mark bugs as Untriaged with label `Mac` (not the OS label "Mac") and
allow the Mac triage rotation to assign them.
***

The Mac bug triage process is split into two phases. First-phase triage is done
daily in week-long shifts by a single person. Second-phase triage is done in a
standing meeting on Mondays by the Mac team as a group.

A key tool of the triage process is the "Mac" label (*not* the same as the Mac
OS tag), which makes bugs visible to the triaging step of the process. This
process deliberately doesn't look at bugs with OS=Mac status:Untriaged, because
maintaining the list of components that can be ignored during that triage step
is untenable.

## Quick Reference

1. During the week, turn [OS=Mac status:Unconfirmed][unconfirmed] bugs into
   [label:Mac status:Untriaged][untriaged-m] bugs.
2. During the triage meeting, turn [label:Mac status:Untriaged][untriaged-m]
   and [unlabelled Mac bugs][untriaged-c] into any of:
    * [label:Mac status:Available][available]
    * [label:Mac status:Assigned][assigned]
    * Untriaged in any component that does triage, without the Mac label
    * Assigned

## First-phase triage

First-phase triage is the step which ensures the symptoms and reproduction steps
of a bug are well-understood. This phase operates on [OS=Mac
status:Unconfirmed][unconfirmed] bugs, and moves these bugs to:

* Needs-Feedback, if awaiting a response from the user
* Untriaged bugs with the Mac label, if they are valid bug reports with working
  repro steps or a crash stack
* WontFix, if they are invalid bug reports or working as intended
* Duplicate, if they are identical to an existing bug

The main work of this phase is iterating with the bug reporter to get crash IDs,
repro steps, traces, and other data we might need to nail down the bug. If the
bugs is obviously very domain-specific (eg: "this advanced CSS feature is
behaving strangely", or "my printer is printing everything upside down"), feel
free to skip this iteration step and send the bug straight to the involved team
or people. Useful tags at this step are:

* Needs-Feedback, which marks the bug as waiting for a response from the
  reporter
* Needs-TestConfirmation, which requests that Test Engineering attempt the bug's
  repro steps
* Needs-Bisect, which requests that Test Engineering bisect the bug down to a
  first bad release

The latter two tags work much better when there are reliable repro steps for a
bug, so endeavour to get those first - TE time is precious and we should make
good use of it.

We wait **30 days** for user feedback on Needs-Feedback bugs; after 30 days
without a response to a question we move bugs to WontFix.

Some useful debugging questions here:

* What are your exact OS version and Chrome version?
* Does it happen all the time?
* Does it happen in Incognito? (this checks for bad cached data, cookies, etc)
* Does it happen with extensions disabled?
* Does it happen in a new profile?
* Does it happen in a new user-data-dir?
* If it's a web bug, is there a reduced test case? We generally can't act on "my
  website is broken" type issues
* Can you attach a screenshot/screen recording of what you mean?
* Can you paste the crash IDs from chrome://crashes?
* Can you get a sample of the misbehaving process with Activity Monitor?
* Can you upload a trace from chrome://tracing?
* Can you paste the contents of chrome://gpu?
* Can you paste the contents of chrome://version?

## Second-phase triage

Second-phase triage is the step which either moves a bug to another team's
triage queue, or assigns a priority, component, and (possibly) owner to a bug.
This phase operates on [label:Mac status:Untriaged][untriaged-m] and [untagged
status:Untriaged][untriaged-c] bugs. The first part of this phase is deciding
whether a bug should be worked on by the Mac team. If so, the bug moves to one
of:

* Pri=2,3 in label:Mac, Assigned with an owner if one is obvious, Available
  otherwise
* Pri=0,1 in label:Mac, Assigned with an owner

Otherwise, the bug loses label:Mac and moves to one of:

* Untriaged in a different component
* Assigned with an owner
* WontFix
* Duplicate

Since our component filter is not comprehensive, sometimes an Untriaged bug
that is being investigated by the appropriate team remains in the Mac queue.
In those cases, labeling the bug `Hotlist-MacTriageBypass` will remove it from
the queue.

Here are some rules of thumb for how to move bugs from label:Mac
status:Untriaged to another component:

* Is the bug Mac-only, or does it affect other platforms? If it affects other
  platforms as well, it's probably out of scope for us and should go into
  another component.
* Is the bug probably in Blink? If so, it should be handled by the Blink
  team's Mac folks; move to component `Blink`.
* Is the bug localized to a specific feature, like the omnibox or the autofill
  system? If so, it should be handled by that team; tag it with their component
  for triage.
* Is the bug a Views bug, even if it's Mac-specific? If so, it should be handled
  by the Views team; mark it as `Internals>Views`.

If the bug is Mac-specific and in scope for the Mac team, try to:

* Assign it to a sublabel of `Mac`
    * `Mac-Accessibility`
    * `Mac-Enterprise`
    * `Mac-Graphics`
    * `Mac-Infra`
    * `Mac-Performance`
    * `Mac-PlatformIntegration`
    * `Mac-Polish`
    * `Mac-TechDebt`
    * `Mac-UI`
* Assign it a priority:
    * Pri=0 means "this is an emergency, work on it immediately"
    * Pri=1 means "we should not ship a stable release with this bug if we can
      help it"
    * Pri=2 means "we should probably fix this" - this is the default bug
      priority
    * Pri=3 means "it would be nice if we fixed this some day"
* Maybe assign it an owner if needed - Pri=0 or 1 need one, Pri=2 or 3 can have
  one if the owner is obvious but don't need one

These are the other components we put bugs into that we assume have their own
triage processes:
* Admin
* Blink
* Infra
* Internals>Cast, Headless, Network, Plugins, Printing, Skia, Views
* IO>Bluetooth, USB
* Platform
* Services>Chromoting
* Test>Telemetry
* UI>Accessibility
* UI>Browser>WebAppInstalls
* UI>Browser>WebUI

[unconfirmed]: https://bugs.chromium.org/p/chromium/issues/list?q=OS%3DMac%20status%3AUnconfirmed%20-component%3ABlink%2CEnterprise%2CInternals%3ENetwork%2CPlatform%3EDevtools%2CServices%3ESyncs%2CUI%3EBrowser%3EPasswords%20-label%3AMac-TriageBypass%20-label%3AHotlist-MacTriageBypass&can=2
[untriaged-m]: https://bugs.chromium.org/p/chromium/issues/list?q=has%3AMac%20status%3AUntriaged%20-label%3AMac-TriageBypass%20-label%3AHotlist-MacTriageBypass&can=2
[untriaged-c]: https://bugs.chromium.org/p/chromium/issues/list?q=OS%3DMac%20-OS%3DWindows%2CLinux%2CChrome%2CAndroid%2CiOS%20status%3AUntriaged%20-component%3AAdmin%2CBlink%2CInfra%2CInternals%3ECast%2CInternals%3EHeadless%2CInternals%3ENetwork%2CInternals%3EPlugins%3EPDF%2CInternals%3EPrinting%2CInternals%3ESkia%2CInternals%3EUpdater%2CInternals%3EViews%2CIO%3EBluetooth%2CIO%3EUSB%2CPlatform%2CServices%3EChromoting%2CTest%3ETelemetry%2CUI%3EBrowser%3EWebAppInstalls%2CPlatform%3EWebAppProvider%2CUI%3EBrowser%3EWebUI%2CInternals%3EGPU%2CEnterprise%2CUI%3EBrowser%3ESharing%2CUI>Accessibility%20-label%3AMac-TriageBypass%20-label%3AHotlist-MacTriageBypass&can=2
[available]: https://bugs.chromium.org/p/chromium/issues/list?q=has%3AMac%20status%3AAvailable&can=2
[assigned]: https://bugs.chromium.org/p/chromium/issues/list?q=has%3AMac%20status%3AAssigned&can=2
