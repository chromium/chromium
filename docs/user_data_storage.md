# User Data Storage

This document explains the Chromium policies for files in the `User Data`
directory.

[TOC]

## Backward Compatibility

Due to the nature of frequent updates, Chromium must always support loading data
from files written by previous versions. A good rule of thumb is to leave
migration code in place for *at least* one year (approximately 9 milestones with
the current 6-week release cadence). It is not uncommon for clients to update
from very old versions, so use good judgement for deciding when to remove
migration code -- if the complexity is low, keep it indefinitely.

## Version Downgrade Processing

In cases where Chromium is run against a `User Data` directory written by a
newer version, the browser may run to the extent possible with the following
behaviors:

*   Versioned files that are apparently readable by the old version may be used
    as-is and modified as needed. For example, a SQLite file containing a table
    with a compatible version number no higher than that supported by the old
    version.
*   Versioned files that cannot be read by the old version and contain user
    configuration or user generated data are left on-disk unmodified. This
    allows the data to be used again once the browser is updated. Furthermore,
    the user should be notified via the [profile error
    dialog](../chrome/browser/ui/profile_error_dialog.h) that their experience
    may be degraded. For example, such a browsing session may not accumulate new
    history database entries.
*   Versioned files that cannot be read by the old version and contain computed
    or cached data may be either left on-disk unmodified or deleted and
    replaced.

## Post-branch Compatibility

Breaking changes in data storage are forbidden once a branch has been created
for a release. This guarantees that data written by a later build on a release
branch can be read by previous versions on that same release branch.

## See also

*   [User Data Directory](user_data_dir.md)
