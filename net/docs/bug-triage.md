# Chrome Network Bug Triage

The Chrome network team uses a two day bug triage rotation.  The main goals are
to identify and label new network bugs, and investigate network bugs when no
label seems suitable.

## Management

Owners for the network bug triage rotation can find instructions on
generating and modifying shifts
[here (internal-only)](https://goto.google.com/pflvb).

## Responsibilities

### Required, in rough order of priority:
* Identify new network bugs on the tracker.
* Investigate recent `Internals>Network` issues with no subcomponent.
* Follow up on `Needs-Feedback` issues for all network components.
* Identify and file bugs for significant new crashers.

### Best effort, also in rough priority order:
* Investigate unowned and owned-but-forgotten net/ crashers.
* Investigate old bugs.
* Close obsolete bugs.

All of the above is to be done on each rotation.  These responsibilities should
be tracked, and anything left undone at the end of a rotation should be handed
off to the next triager.  The downside to passing along bug investigations like
this is each new triager has to get back up to speed on bugs the previous
triager was investigating.  The upside is that triagers don't get stuck
investigating issues after their time after their rotation, and it results in a
uniform, predictable two day commitment for all triagers.

## Details

### Required:

* Identify new network bugs on the bug tracker, looking at [this issue tracker
  query](https://bugs.chromium.org/p/chromium/issues/list?q=status%3Aunconfirmed+-commentby=425761728072-pa1bs18esuhp2cp2qfa1u9vb6p1v6kfu@developer.gserviceaccount.com&sort=-id&num=1000).

  * All Unconfirmed issues filed during your triage rotation should be scanned
    for suspected network bugs, a network component assigned and a
    chrome://net-export/ log requested.  Suggested text: "Please collect and
    attach a chrome://net-export log. Instructions can be found here:
    https://chromium.org/for-testers/providing-network-details".
    A link to the instructions appears on net-export, for easy reference.
    When asking for a log or more details, attach the Needs-Feedback label.

  * A triager is responsible for looking at bugs reported from noon PST /
    3:00 pm EST of the last day of the previous triager's rotation until the
    same time on the last day of their rotation.

* Investigate [Unconfirmed / Untriaged Internals>Network issues that don't belong to a more specific network component](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3DInternals%3ENetwork+status%3AUnconfirmed,Untriaged+-label:Needs-Feedback&sort=-modified),
  prioritizing the most recent issues, ones with the most responsive reporters,
  and major crashers.  This will generally take up the majority of your time as
  triager. Continue digging until you can do one of the following:

    * Mark it as `WontFix` (working as intended, obsolete issue) or a
      duplicate.

    * Mark it as a feature request.

    * Mark it as `Needs-Feedback`.

    * Remove the `Internals>Network` component, replacing it with at least one
      more specific network component or non-network component. Replacing the
      `Internals>Network` component gets it off the next triager's radar, and
      in front of someone more familiar with the relevant code.  Note that
      due to the way the bug report wizard works, a lot of bugs incorrectly end
      up with the network component.

    * The issue is assigned to an appropriate owner.  Make sure to mark it as
      "assigned" so the next triager doesn't run into it.

    * If there is no more specific component for a bug, it should be
      investigated by the triager until we have a good understanding of the
      cause of the problem, and some idea how it should be fixed, at which point
      its status should be set to Available.  Future triagers should ignore bugs
      with this status, unless investigating stale bugs.

* Follow up on [Needs-Feedback issues for all components owned by the network stack team](https://bugs.chromium.org/p/chromium/issues/list?q=component%3AInternals%3ENetwork+-component%3AInternals%3ENetwork%3EDataProxy+-component%3AInternals%3ENetwork%3EDataUse+-component%3AInternals%3ENetwork%3EVPN+Needs%3DFeedback&sort=-modified).

    * Remove label once feedback is provided.  Continue to investigate, if
      the previous section applies.

    * If the `Needs-Feedback` label has been present for one week, ping the
      reporter.

    * Archive after two weeks with no feedback, telling users to file a new
      bug if they still have the issue, with the requested information, unless
      the reporter indicates they'll provide data when they can.  In that case,
      use your own judgment for further pings or archiving.

* Identify significant new crashes. See [internal documentation](https://goto.google.com/network_triage_internal#looking-for-new-crashers).

### Best Effort (As you have time):

* Investigate old bugs, and bugs associated with `Internals>Network`
  subcomponents.

* Investigate unowned and owned but forgotten net/ crashers that are still
  occurring (As indicated by
  [go/chromenetcrash](https://goto.google.com/chromenetcrash)), prioritizing
  frequent and long standing crashers.

* Close obsolete bugs.

See [bug-triage-suggested-workflow.md](bug-triage-suggested-workflow.md) for
suggested workflows.

See [bug-triage-labels.md](bug-triage-labels.md) for labeling tips for network
and non-network bugs.

See [crash-course-in-net-internals.md](crash-course-in-net-internals.md) for
some help on getting started with chrome://net-internals debugging.
