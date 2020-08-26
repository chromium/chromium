# Guidelines for Considering Branch Date in Project Planning

*   Avoid changes to persisted data (e.g. data serialization, database schemas,
    etc) just before branch due to the risk of reverts. If you modify data that
    will be synced, this change should be forward compatible for up to four
    milestones (as stable and canary are at times four milestones apart and it's
    reasonable to assume that somebody runs stable on chromeos and canary on
    mac, both syncing to the same account).
*   Two weeks prior to the branch point, avoid committing big and risky changes
    or enabling non-trivial features.
*   Pay more attention to complexity and structure of CLs around branch points,
    in case fixes need to be merged back after branch is created. Break up a CL
    into smaller pieces to facilitate potential merges.
*   As the branch point gets closer, consider if there is data to gather (like
    adding use counters, or other histograms that we want) and prioritize that
    work. Metrics changes on branch don't meet the bar.
*   Prioritize stability and "bake time", as well as other ways to reduce the
    release risk in your timeline discussions.
*   Consider API adoption timelines: code may not be exercised until developers
    adopt API, so feature freeze and branch could have minimal impact on
    stability.

