# Chromium Issue Priorities & SLOs

[TOC]


## Priority definition

Five priority levels from P0-P4.

-   **P0 (emergency)**
    -   Regressions which are substantially impacting existing users, partners,
        or developers.
    -   High-risk security issues affecting the stable channel.
-   **P1 (priority engineering work)**
    -   Major Regressions.
    -   Work requiring prompt resolution.
    -   Work that has to get done before the targeted release.
-   **P2 (active engineering work)**
    -   Non-urgent issues.
    -   Important issues that are worked on as best effort, without a milestone.
    -   Polish or bug fixing work in areas where the team has decided we want to
        invest
-   **P3 (later, want to do)**
    -   Something we want to do, but not right now.
    -   Legitimate issues that we will work on when we have the cycles to do so.
-   **P4 (later, future)**
    -   List of things we are considering doing in the future

## SLOs

The following SLOs only apply to bugs of type "Bug", "Vulnerability" (Security),
"Customer Issue" or "Privacy Issue".

<table>
  <tr>
   <td>&nbsp;</td>
   <td><strong>Assigned</strong></td>
   <td><strong>Updated</strong></td>
   <td><strong>Fixed</strong></td>
  </tr>
  <tr>
   <td><strong>P0</strong></td>
   <td>1 business day</td>
   <td>Every business day</td>
   <td>Within 1 week</td>
  </tr>
  <tr>
   <td><strong>P1</strong></td>
   <td>1 week</td>
   <td>Weekly</td>
   <td>Within 4 weeks</td>
  </tr>
</table>

Priorities 2-4 are out of scope for SLO.

Additionally, "Vulnerability" (Security) type issues carry the following closure
SLOs based on the noted severity.

<table>
  <tr>
   <td><strong>S</strong></td>
   <td><strong>Closure SLO</strong></td>
  </tr>
  <tr>
   <td><strong>S0</strong></td>
   <td>1 week</td>
  </tr>
  <tr>
   <td><strong>S1</strong></td>
   <td>4 weeks</td>
  </tr>
  <tr>
   <td><strong>S2</strong></td>
   <td>12 weeks</td>
  </tr>
  <tr>
   <td><strong>S3+</strong></td>
   <td>No SLO</td>
  </tr>
</table>

### Release Blocking Issues

These are SLOs for issues that are severe enough to block a release shipping to
users  (see [Release Blockers](release_blockers.md)). They apply to bug types in the same way as
the above SLOs.

<table>
  <tr>
   <td>&nbsp;</td>
   <td><strong>Assigned</strong></td>
   <td><strong>Updated</strong></td>
   <td><strong>Fixed</strong></td>
  </tr>
  <tr>
   <td><strong>Urgent</strong></td>
   <td>1 day</td>
   <td>Every day</td>
   <td>2 days*</td>
  </tr>
  <tr>
   <td><strong>Standard</strong></td>
   <td>2 days</td>
   <td>Every 2 days</td>
   <td>Within 5 days*</td>
  </tr>
  <tr>
  <td colspan="4">
  <em><small>* Fixed includes CLs landed to all relevant release branches.</small></em>
  </td>
  </tr>
</table>