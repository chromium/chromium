# Perf Regression Sheriffing (go/perfregression-sheriff)

The perf regression sheriff tracks performance regressions in Chrome's
continuous integration tests. Note that a [new rotation](perf_bot_sheriffing.md)
has been created to ensure the builds and tests stay green, so the perf
regression sheriff role is now entirely focused on performance.

**[Rotation calendar](https://calendar.google.com/calendar/embed?src=google.com_2fpmo740pd1unrui9d7cgpbg2k%40group.calendar.google.com)**

## Key Responsibilities

 * [Triage Regressions on the Perf Dashboard](#Triage-Regressions-on-the-Perf-Dashboard)
 * [Follow up on Performance Regressions](#Follow-up-on-Performance-Regressions)
 * [Give Feedback on our Infrastructure](#Give-Feedback-on-our-Infrastructure)

## Triage Regressions on the Perf Dashboard

Open the perf dashboard [alerts page](https://chromeperf.appspot.com/alerts).

In the upper right corner, **sign in with your Chromium account**. Signing in is
important in order to be able to kick off bisect jobs, and see data from
internal waterfalls.

Pick up **Chromium Perf Sheriff** from "Select an item ▼" drop down menu.
table of "Performance Alerts" should be shown. If there are no currently pending
alerts, then the table won't be shown.

The list can be sorted by clicking on the column header. When you click on the
checkbox next to an alert, all the other alerts that occurred in the same
revision range will be highlighted.

Check the boxes next to the alerts you want to take a look at, and click the
"Graph" button. You'll be taken to a page with a table at the top listing all
the alerts that have an overlapping revision range with the one you chose, and
below it the dashboard shows graphs of all the alerts checked in that table.

1. **For alerts related to `resource_sizes`:**
    * Refer to [apk_size_regressions.md](apk_size_regressions.md).
2. **Look at the graph**.
    * If the alert appears to be **within the noise**, click on the red
      exclamation point icon for it in the graph and hit the "Report Invalid
      Alert" button.
    * If the alert appears to be **reverting a recent improvement**, click on
      the red exclamation point icon for it in the graph and hit the "Ignore
      Valid Alert" button.
    * If the alert is **visibly to the left or the right of the
      actual regression**, click on it and use the "nudge" menu to move it into
      place.
    * If there is a line labeled "ref" on the graph, that is the reference build.
      It's an older version of Chrome, used to help us sort out whether a change
      to the bot or test might have caused the graph to jump, rather than a real
      performance regression. If **the ref build moved at the same time as the
      alert**, click on the alert and hit the "Report Invalid Alert" button.
3. **Look at the other alerts** in the table to see if any should be grouped together.
   Note that the bisect will automatically dupe bugs if it finds they have the
   same culprit, so you don't need to be too aggressive about grouping alerts
   that might not be related. Some signs alerts should be grouped together:
    * If they're all in the same test suite
    * If they all regressed the same metric (a lot of commonality in the Test
      column)
4. **Triage the group of alerts**. Check all the alerts you believe are related,
  and press the triage button.
    * If one of the alerts already has a bug id, click "existing bug" and use
      that bug id.
    * Otherwise click "new bug".
    * Only add a description if you have additional context. Otherwise a default
      description will be automatically added when left blank.
5. **Look at the revision range** for the regression. You can see it in the
   tooltip on the graph. If you see any likely culprits, cc the authors on the
   bug.
6. **Optionally, kick off more bisects**. The perf dashboard will automatically
   kick off a bisect for each bug you file. But if you think the regression is
   much clearer on one platform, or a specific page of a page set, or you want
   to see a broader revision range feel free to click on the alert on that graph
   and kick off a bisect for it. There should be capacity to kick off as many
   bisects as you feel are necessary to investigate; [give feedback](#feedback)
   below if you feel that is not the case.

### Dashboard UI Tips

* Grouping is done client side today. If you click "Show more" at the bottom
until you can see all the alerts, the alerts will be grouped together more.
* You can shift click on the check boxes to select multiple alerts quickly.

## Follow up on Performance Regressions

During your shift, you should try to follow up on each of the bugs you filed.
Once you've triaged all the alerts, check to see if the bisects have come back,
or if they failed. If the results came back, and a culprit was found, follow up
with the CL author. If the bisects failed to update the bug with results, please
file a bug on it (see [feedback](#feedback) links below).

Also during your shift, please spend any spare time driving down bugs from the
[regression backlog](http://go/triage-backlog). Treat these bugs as you would
your own -- investigate the regressions, find out what the next step should be,
and then move the bug along. Some possible next steps and questions to answer
are:

*   Should the bug be closed?
*   Are there questions that need to be answered?
*   Are there people that should be added to the CC list?
*   Is the correct owner assigned?

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
* **Perf Bisect/Trybots**: If a bisect is identifying the wrong CL as culprit
  or missing a clear culprit, or not reproducing what appears to be a clear
  regression, please link the comment the bisect bot posted on the bug at
  [go/bad-bisects](https://docs.google.com/spreadsheets/d/13PYIlRGE8eZzsrSocA3SR2LEHdzc8n9ORUoOE2vtO6I/edit#gid=0).
  The team triages these regularly. If you spot a really clear bug (bisect
  job red, bugs not being updated with bisect results) please file it in
  crbug with component `Speed>Bisection`. If a bisect problem is blocking a
  perf regression bug triage, **please file a new bug with component
  `Speed>Bisection` and block the regression bug on the bisect bug**. This
  makes it much easier for the team to triage, dupe, and close bugs on the
  infrastructure without affecting the state of the perf regression bugs.
* **Noisy Tests**: Please file a bug in crbug with component `Speed>Benchmarks`
  and [cc the owner](http://go/perf-owners).
