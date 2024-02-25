# CQ Fault Attribution

The Failure Analysis section in Gerrit provides an easily discoverable view of
CQ test variant (and soon, build) failures, and categorizes them by comparing
the failures to the same test variant in snapshot builds. The failures are
categorized as one of the following:

- New failure (not currently present in tip-of-tree \[ToT\])
- Pre-existing with a different failure reason (test variant failure present in
  ToT, but for a different failure reason)
- Pre-existing with the same failure reason (test variant failure present in
  ToT for the same failure reason)

Additionally, any test variant failures that have not been able to be
classified due to insufficient test history in the snapshot / postsubmit builds
used in ToT will be indicated as such.

Within the Gerrit checks tab, any fault attributed test row will contain a tag
with the corresponding fault attribution category. For any tests pre-existing
failures, a link to the snapshot where the failure was found will be present as
well.

To provide any feedback on fault attribution (either positive or constructive
feedback), please feel free to create a buganizer issue [here](https://buganizer.corp.google.com/issues/new?component=1315651&template=1849636).
