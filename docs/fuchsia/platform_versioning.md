# Fuchsia platform versioning

When building for Fuchsia, the binaries are built against a target API level,
which is set via `fuchsia_target_api_level` in the [`.gn` file](../../.gn). This
information becomes embedded in the compiled component as a target ABI
revision, which indicates the semantics the component expects from the
platform.

Updates to the `fuchsia_target_api_level` are currently done manually. As a
general rule, we want to be using the latest API level supported by the Fuchsia
SDK checked out. An older API level might be needed if one of the following are
true:

1. There are unresolved compatibility issues.
2. The binaries are intended to ship on an older release of Fuchsia.

For instance, if M97 is shipped on a Fuchsia release that only supports API
level up to 5, the target API level cannot be updated in Chromium until M97
branch is cut.
