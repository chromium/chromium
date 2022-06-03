# CQ usage owners

These files control the ownership for what gets run on the CQ for chromium.
Triggering builders on the CQ is costly due to the frequency that the CQ is
triggered. The cost to trigger a builder on the CQ needs to be weighed against
the expected benefit. Additionally, if the resources for a builder are not
correctly configured, then it could result in excessively long CQ times and/or a
broken CQ, preventing developers from testing and/or submitting their code in a
reasonable time frame. To prevent bad changes, the ownership of the CQ usage is
limited to those that are in a position to make such decisions.

## Secondary owners

Some classes of changes to the CQ usage are expected to be lower risk (e.g.
changing the path for a path-based builder). To avoid creating a review
bottleneck for such changes, there are additional owners that can grant the
necessary approval. These secondary owners should defer to the primary owners if
such a change might have a large impact on the CQ capacity.
