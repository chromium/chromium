# CQ Usage

This directory exists to surface any changes to the CQ config that would change
what/when the CQ blocks on builders. The OWNERS for this directory is
intentionally limited because it is costly to trigger builders on the CQ and
this needs to be weighed against the expected benefit. Additionally, if the
resources for a builder are not correctly configured, then it could result in
excessively long CQ times, preventing developers from testing and/or submitting
their code in a reasonable time frame.

Changes to the CQ config that add or modify optional or experimental builders
(excluding changes that move an optional or experimental builder to being a
blocking builder or vice versa) should not result in changes to files in this
directory. If such changes do cause modifications in this directory, please file
a bug against component Infra>Client>Chrome.
