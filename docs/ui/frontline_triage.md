# Chromium UI Frontline Triage Procedures and Policy

## Filing New Issues

### Steps

0. Read and follow the [Chromium Code of Conduct](https://chromium.googlesource.com/chromium/src/+/main/CODE_OF_CONDUCT.md).
1. File your issue on http://crbug.com/.
   * Required: Repro Steps
   * Preferred: Screenshots
   * Optional: Video Repro (Please keep brief and to the point)
2. Add the label Hotlist-DesktopUIConsider.
3. Find an appropriate component.

### Consideration Deadline
Issues must be marked Hotlist-DesktopUIConsider by Wednesday 5:00p PT for
consideration for that week.

## Weekly Triage Process
1. After the consideration deadline, query for all Hotlist-DesktopUIConsider
   bugs.
2. For each bug, perform the following:
   1. Assess Priority
      * P0: Emergency, must be fixed now or before the next Canary. Mark with a
            ReleaseBlock label for the nearest build that does not have the
            issue.
      * P1: Bug most users will hit. Mark with a ReleaseBlock label.
        * Mark ReleaseBlock-Dev if the issue impacts a core scenario (like
          scrolling).
        * Mark ReleaseBlock-Beta if the issue impacts a secondary scenario.
        * Mark ReleaseBlock-Stable if Chrome should never ship with this issue.
      * P2 - Bug that many users may hit and/or has a reasonable workaround.
      * P3 - Bug that few users will hit.
   2. Schedule a milestone.
   3. Assign to relevant area owner:
      * Omnibox: jdonneley@
      * Top Chrome: dfried@
      * UI Platform: robliao@
      * WebUI: dpapad@
3. Once all bugs are triaged, remove the Hotlist-DesktopUIConsider label and add
   Hotlist-DesktopUITriaged.

## Periodic Review at Milestone Branch
For all Hotlist-DesktopUITriaged bugs, reassess priority and milestone using the
above standards.
