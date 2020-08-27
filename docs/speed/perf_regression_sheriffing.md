# Perf Regression Sheriffing (go/perfregression-sheriff)

The perf regression sheriff tracks performance regressions in Chrome's
continuous integration tests. Note that a [different
rotation](perf_bot_sheriffing.md) has been created to ensure the builds and
tests stay green, so the perf regression sheriff role is now entirely focused
on performance.

**[Rotation calendar](https://calendar.google.com/calendar/embed?src=google.com_2fpmo740pd1unrui9d7cgpbg2k%40group.calendar.google.com)**

## Key Responsibilities

* [Address bugs needing attention](#Address-bugs-needing-attention)

* [Follow up on Performance Regressions](#Follow-up-on-Performance-Regressions)

* [Give Feedback on our Infrastructure](#Give-Feedback-on-our-Infrastructure)

## Address bugs needing attention

NOTE: Ensure that you're signed into Monorail.

Use [this Monorail query](https://bugs.chromium.org/p/chromium/issues/list?sort=modified&q=label%3AChromeperf-Sheriff-NeedsAttention%2CChromeperf-Auto-NeedsAttention%20-has%3Aowner&can=2)
to find automatically triaged issues which need attention.

NOTE: If the list of issues that need attention is empty, please jump ahead to
[Follow up on Performance Regressions](#Follow-up-on-Performance-Regressions).

Issues in the list will include automatically filed and bisected regressions
that are supported by the Chromium Perf Sheriff rotation. For each of the
issues:

1. Determine the cause of the failure:

   * If it's Pinpoint failing to find a culprit, consider re-running the
     failing Pinpoint job.

   * If it's the Chromeperf Dashboard failing to start a Pinpoint bisection,
     consider running a bisection from the grouped alerts. The issue
     description should have a link to the group of anomalies associated with
     the issue.

   * If this was a manual escalation (e.g. a suspected culprit author put the
     `Chromeperf-Sheriff-NeedsAttention` label to seek help) use the tools at
     your disposal, like:

     * Retry the most recent Pinpoint job, potentially changing the parameters.

     * Inspect the results of the Pinpoint job associated with the issues and
       decide that this could be noise.

   * In cases where it's unclear what next should be done, escalate the issue
     to the Chrome Speed Tooling team by adding the `Speed>Bisection` component
     and leaving the issue `Untriaged` or `Unconfirmed`.

2. Remove the `Chromeperf-Sheriff-NeedsAttention` or
   `Chromeperf-Auto-NeedsAttention` label once you've acted on an issue.

**For alerts related to `resource_sizes`:** Refer to
 [apk_size_regressions.md](apk_size_regressions.md).

## Follow up on Performance Regressions

Please spend any spare time driving down bugs from the [regression
backlog](http://go/triage-backlog). Treat these bugs as you would your own --
investigate the regressions, find out what the next step should be, and then
move the bug along. Some possible next steps and questions to answer are:

* Should the bug be closed?
* Are there questions that need to be answered?
* Are there people that should be added to the CC list?
* Is the correct owner assigned?

When a bug does need to be pinged, rather than adding a generic "ping", it's
much much more effective to include the username and action item.

You should aim to end your shift with an empty backlog, but it's important to
still advance each bug in a meaningful way.

After your shift, please try to follow up on the bugs you filed weekly. Kick off
new bisects if the previous ones failed, and if the bisect picks a likely
culprit follow up to ensure the CL author addresses the problem. If you are
certain that a specific CL caused a performance regression, and the author does
not have an immediate plan to address the problem, please revert the CL.

## Give Feedback on our Infrastructure

Perf regression sheriffs have their eyes on the perf dashboard and bisects
more than anyone else, and their feedback is invaluable for making sure these
tools are accurate and improving them. Please file bugs and feature requests
as you see them:

* **Perf Dashboard**: Please use the red "Report Issue" link in the navbar.
* **Pinpoint**: If Pinpoint is identifying the wrong CL as culprit
  or missing a clear culprit, or not reproducing what appears to be a clear
  regression, please file an issue in crbug with the `Speed>Bisection`
  component.
* **Noisy Tests**: Please file a bug in crbug with component `Speed>Benchmarks`
  and [cc the owner](http://go/perf-owners).
