Managing third-party dependencies
===

Before reading documentation in this directory, please check the general
policies in
[Adding third_party Libraries](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md).
You MUST obtain the required approvals before adding a third-party dependency
and accept the responsibility as a dependency owner.

This directory contains how-to guides on managing your dependency with autoroll
tooling (aka. well-lit paths). Please refer to individual files to understand
their use cases and constraints. You should adopt **one well-lit path** that
suits your need (e.g. code organization, need for patching).

You can request to be exempted from the well-lit paths, please refer to
[Autoroll Exceptions](https://chromium.googlesource.com/chromium/src/+/main/docs/adding_to_third_party.md#Autoroll-Exceptions).

If you are not sure which path to take, please file a bug in
["Chromium > Third Party > Freshness" component](https://issues.chromium.org/issues/new?noWizard=true&component=1900398&template=2268619)
for consultation.

If you're a Googler, you can alternatively email
[chrome-ssci-team@google.com](mailto:chrome-ssci-team@google.com).
