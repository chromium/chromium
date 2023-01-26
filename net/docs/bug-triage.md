# Chrome Network Bug Triage

The Chrome network team uses a two day bug triage rotation. The goal is to
review outstanding issues and keep things moving forward. The rotation is time
based rather than objective based. Sheriffs are expected to spend the majority
of their two days working on bug triage/investigation.

## 1. Review untriaged bugs

Look through [this list of untriaged
bugs](https://bugs.chromium.org/p/chromium/issues/list?sort=pri%20-stars%20-opened&q=status%3Aunconfirmed%2Cuntriaged%20-Needs%3DFeedback%20-Label%3ANetwork-Triaged%20-has%3ANextAction%20component%3DInternals%3ENetwork%3EReportingAndNEL%2CInternals%3ENetwork%3ECache%3ESimple%2CInternals%3ENetwork%2CInternals%3ENetwork%3ECache%2CInternals%3ENetwork%3ESSL%2CInternals%3ENetwork%3EQUIC%2CInternals%3ENetwork%3EAuth%2CInternals%3ENetwork%3EHTTP2%2CInternals%3ENetwork%3EProxy%2CInternals%3ENetwork%3ELogging%2CInternals%3ENetwork%3EConnectivity%2CInternals%3ENetwork%3EDomainSecurityPolicy%2CInternals%3ENetwork%3EFTP%2CInternals%3ENetwork%3EDNS).

The goal is for this query to be empty. Bugs can be removed from the triage queue
by doing any of the following:

* Changing the bug status - marking the bug Available, or closing it.
* Removing the `Internals>Network` component or subcomponent.
* Adding the label `Network-Triaged` (when there are multiple components).

For each bug try to:

* Remove the `Internals>Network` component or subcomponent if it belongs
  elsewhere
* Dupe it against an existing bug
* Close it as `WontFix`.
* Give the bug a priority. Refer to [this document for guidelines](https://docs.google.com/document/d/1JOtp1LS7suqTjMuv41jQFc7aCTR33zJKPoGjKpvVFCA)
* If the bug is a potential security issue (Allows for code execution from remote
  site, allows crossing security boundaries, unchecked array bounds, etc) mark
  it `Type-Bug-Security`.
* If the bug has privacy implications mark it with component `Privacy`.
* Set the type to `Task` or `Feature` when it is not a bug.
* Pay extra attention to possible regressions. Ask the reporter to narrow down using
  [bisect-builds-py](https://www.chromium.org/developers/bisect-builds-py). To
  view suspicious changelists in a regression window, you can use the Change Log
  form on [OmahaProxy](https://omahaproxy.appspot.com/)
* CC others who may be able to help
* Mark it as `Needs-Feedback` and request more information if needed.
* In cases where the bug has multiple components, but primary ownership falls
  outside of networking, further network triage may not be possible. In those
  cases, if possible remove the networking component. Otherwise, add the
  `Network-Triaged` label to the bug, and add a comment explaining which team
  should triage further. Adding the `Network-Triaged` serves to filter the
  bug from our untriaged bug list.
* Avoid spending time deep-diving into ambiguous issues when you suspect it is
  an out of scope server or network problem, and is not clearly high priority
  (for instance, it affects only 1 user and is not a regression).
  Instead:
  * Mark the bug as `Available` with Priority 3.
  * Add the `Needs-Feedback` label
  * Add a comment thanking the reporter, but explaining the issue is ambiguous
    and they need to do the debugging to demonstrate it is an actual Chrome bug.
    * Point them to `chrome://net-export` and the
      [NetLog Viewer](https://netlog-viewer.appspot.com/).
    * Ask them to confirm whether it is a Chromium regression. (Regressions are
      treated as high priority)
* Request a NetLog that captures the problem. You can paste this on the bug:
  ```
  Please collect and attach a chrome://net-export log.
  Instructions can be found here:
  https://chromium.org/for-testers/providing-network-details
  ```
* If a NetLog was provided, try to spend a bit of time reviewing it. See
  [crash-course-in-net-internals.md](crash-course-in-net-internals.md) for an
  introduction.
* Move to a subcomponent of `Internals>Network` if appropriate. See
  [bug-triage-labels.md](bug-triage-labels.md) for an overview of the components.
* If the bug is a crash, see [internal: Dealing with a crash
  ID](https://goto.google.com/network_triage_internal#dealing-with-a-crash-id)
and [internal: Investigating
crashers](https://goto.google.com/network_triage_internal#investigating-crashers)

## 2. Follow-up on issues with the Needs-Feedback label

Look through [this list of Needs=Feedback
bugs](https://bugs.chromium.org/p/chromium/issues/list?sort=-modified%20-modified&q=Needs%3DFeedback%20component%3DInternals%3ENetwork%3EReportingAndNEL%2CInternals%3ENetwork%3ECache%3ESimple%2CInternals%3ENetwork%2CInternals%3ENetwork%3ECache%2CInternals%3ENetwork%3ESSL%2CInternals%3ENetwork%3EQUIC%2CInternals%3ENetwork%3EAuth%2CInternals%3ENetwork%3EHTTP2%2CInternals%3ENetwork%3EProxy%2CInternals%3ENetwork%3ELogging%2CInternals%3ENetwork%3EConnectivity%2CInternals%3ENetwork%3EDomainSecurityPolicy%2CInternals%3ENetwork%3EFTP%2CInternals%3ENetwork%3EDNS).

* If the requested feedback was provided, review the new information and repeat
  the same steps as (1) to re-triage based on the new information.
* If the bug had the `Needs-Feedback` label for over 30 days, and the
  feedback needed to make progress was not yet provided, archive the bug.

## 3. Ensure P0 and P1 bugs have an owner

Look through [the list of unowned high priority
bugs](https://bugs.chromium.org/p/chromium/issues/list?sort=pri%20-stars%20-opened&q=Pri%3A0%2C1%20-has%3Aowner%20-label%3ANetwork-Triaged%20component%3DInternals%3ENetwork%3EReportingAndNEL%2CInternals%3ENetwork%3ECache%3ESimple%2CInternals%3ENetwork%2CInternals%3ENetwork%3ECache%2CInternals%3ENetwork%3ESSL%2CInternals%3ENetwork%3EQUIC%2CInternals%3ENetwork%3EAuth%2CInternals%3ENetwork%3EHTTP2%2CInternals%3ENetwork%3EProxy%2CInternals%3ENetwork%3ELogging%2CInternals%3ENetwork%3EConnectivity%2CInternals%3ENetwork%3EDomainSecurityPolicy%2CInternals%3ENetwork%3EFTP%2CInternals%3ENetwork%3EDNS).
These bugs should either have an owner, or be downgraded to a lower priority.

## 4. (Optional) Look through crash reports

Top crashes will already be entered into the bug system by a different process,
so will be handled by the triage steps above.

However if you have time to look through lower threshold crashes, see
[internal: Looking for new crashers](https://goto.google.com/network_triage_internal#looking-for-new-crashers)

## 5. Send out a sheriff report

On the final day of your rotation, send a brief summary to net-dev@chromium.org
detailing any interesting or concerning trends. Do not discuss any restricted
bugs on the public mailing list.

## Covered bug components

Not all of the subcomponents of `Interals>Network` are handled by this rotation.

The ones that are included are:

```
Internals>Network
Internals>Network>Auth
Internals>Network>Cache
Internals>Network>Cache>Simple
Internals>Network>DNS
Internals>Network>Connectivity
Internals>Network>DomainSecurityPolicy
Internals>Network>FTP
Internals>Network>HTTP2
Internals>Network>Logging
Internals>Network>Proxy
Internals>Network>QUIC
Internals>Network>ReportingAndNEL
Internals>Network>SSL
```

The rest of the `Internals>Network` subcomponents are out of scope,
and covered by separate rotations:

```
Internals>Network>Certificate
Internals>Network>CertTrans
Internals>Network>Cookies
Internals>Network>DataProxy
Internals>Network>DataUse
Internals>Network>DoH
Internals>Network>EV
Internals>Network>Library
Internals>Network>NetInfo
Internals>Network>NetworkQuality
Internals>Network>TrustTokens
Internals>Network>VPN
```

## Management

* Your rotation will appear in Google Calendar for three days, as a full day 
  event. However you should work on it during normal business hours only. 
  The bug triage work should be considered P1 during this period, but you 
  should feel free to work on project work once you triage outstanding issues.

* Google Calendar [c_c76b86b0356db19c1e06879b16d84a2fb7b9747f066b671c46875261c9e7f17a@group.calendar.google.com](https://calendar.google.com/calendar/embed?src=c_c76b86b0356db19c1e06879b16d84a2fb7b9747f066b671c46875261c9e7f17a%40group.calendar.google.com&ctz=Asia%2FTokyo)

* Owners for the network bug triage rotation can find instructions on
  generating and modifying shifts
[here (internal-only)](https://goto.google.com/net-triage-setup).

* An overview of bug trends can be seen on [Chromium
  Dashboard](https://chromiumdash.appspot.com/components/Internals/Network?project=Chromium)

* The issue tracker doesn't track any official mappings between components and
  OWNERS. This [internal document](https://goto.google.com/kojfj) enumerates
  the known owners for subcomponents. Owners information is dynamic, and the 
  document might become outdated, you can always go to the source, search for 
  the component in a DIR_METADATA file and look for an OWNERS file in the same, 
  or parent directory.

* [Web Platform Team SLOs](https://docs.google.com/document/d/18ylPve6jd43m8B7Dil6xmS4G9MHL2_DhQon72je-O9o/edit)

* [Web Platform bug triage guidelines](https://docs.google.com/document/d/1JOtp1LS7suqTjMuv41jQFc7aCTR33zJKPoGjKpvVFCA)