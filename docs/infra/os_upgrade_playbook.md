# OS Upgrade Playbook for Chromium Builders and Fleets

[TOC]

## Overview & Planning

Upgrading the operating system (OS) of a Chromium builder or an entire machine
fleet (e.g., Ubuntu 20.04 -> 24.04, macOS 14 -> macOS 15, Windows 10 -> Windows
11) is a critical infrastructure operation. It ensures the build infrastructure
remains secure, performant, and supported by upstream vendors.

To prevent production outages, tree closures, or Commit Queue (CQ) regressions,
OS upgrades follow a phased rollout strategy.

### Pre-Flight Checklist
Before initiating an OS upgrade:
1. **File a Parent Tracking Bug:** Create a tracking bug on Buganizer under
   `Infra>Client>Chrome` to track the overall OS upgrade lifecycle.
2. **Review Infrastructure Terminology:** If you are new to Chromium build
   infrastructure, consult [`glossary.md`](glossary.md) for definitions of LUCI,
   Swarming, Milo, Starlark, CQ, and CI. For general builder setup, refer to
   [`new_builder.md`](new_builder.md).
3. **Notify Tree Sheriffs & Gardeners:** Inform current Chromium Sheriffs /
   Gardeners of the upcoming migration window.
4. **Determine Hardware Lead Times:** For physical hardware pools (e.g. Macs,
   mobile devices), coordinate early with Fleet Operations via
   [`go/ineedhw`](https://goto.google.com/ineedhw) (*Google internal*).

---

## Phase 1: Set Up a Non-Blocking FYI Builder

Before migrating a production CQ (try) or CI builder to a new OS version,
engineers first create an **FYI builder**.

### Purpose of an FYI Builder
- **Identical Workload:** Runs the exact same set of compile targets and tests
  as the production builder to be migrated.
- **Isolated Target OS:** Configured with swarming dimensions targeting the
  **new** OS version.
- **Non-Blocking Signal:**
  - Does **NOT** block developer CLs or CQ runs.
  - Does **NOT** close the waterfall tree upon test failure.
  - Does **NOT** trigger alerts for sheriff rotations or gardeners.

This provides the team with a safe environment to validate compile integrity,
identify test flakiness or regressions on the new OS, fix missing dependencies,
and adjust test parameters prior to fleet rollout.

### Step-by-Step: Creating an FYI Builder

1. **Locate the Builder Group Starlark File**
   FYI builder definitions live under
   [`//infra/config/subprojects/chromium/ci/`](../../infra/config/subprojects/chromium/ci/).
   Choose or create the relevant OS-specific FYI Starlark file (e.g.,
   `chromium.fyi.star`, `chromium.linux.star`, or `chromium.mac.fyi.star`).

2. **Define the FYI Builder in Starlark**
   For general guidelines on creating builders, see
   [`new_builder.md`](new_builder.md). Copy the existing production builder
   declaration and adjust the following properties:
   - **`name`**: Choose a descriptive name reflecting the target OS version
     (e.g., `"linux-rel-2404-fyi"` or `"mac-15-rel-fyi"`).
   - **`builder_group`**: Set to an FYI group such as `"chromium.fyi"`,
     `"chromium.mac.fyi"`, or `"chromium.win.fyi"`.
   - **`os`**: Specify the new OS swarming dimension using the Starlark helper
     (e.g., `os = os.LINUX_UBUNTU_24_04` or `os = os.MAC_15`).
   - **Mirror Configuration / Targets:** Mirror the original builder's GN args
     and test bundle targets.
   - **Tree Closing / Sheriffing:** Omit `tree_closing_notifiers` and
     `gardener_rotations` (or set them to `None`).

   ```starlark
   # Example: FYI builder for testing Ubuntu 24.04 OS upgrade
   ci.builder(
       name = "linux-rel-2404-fyi",
       builder_group = "chromium.fyi",
       os = os.LINUX_UBUNTU_24_04,
       cores = 32,
       # Match GN args and target specs of the production "linux-rel" builder
       gn_args = gn_args.config(
           # ... matching config ...
       ),
       targets = targets.bundle(
           # ... matching targets ...
       ),
       # Ensure no sheriff alerts or tree closing notifications
       gardener_rotations = None,
       tree_closing_notifiers = None,
   )
   ```

3. **Commit the Starlark Config Changes**
   Submit a CL with the new FYI builder declaration and verify the builder
   appears on Milo to validate build and test health on the target OS.

### Monitoring Performance & Capacity Planning

Once the FYI builder is running, use it to gather performance metrics and test
health signals before proceeding to fleet-wide migration.

#### 1. Measure Build & Test Speed Regressions
- **Runtime Metrics:** Compare build completion times and test suite execution
  durations against the production builder running on the current OS version.
- **Failures & Flakiness:** Identify individual test suites, binary target
  failures, or flakiness specific to the new OS version so fixes can be landed
  early.

#### 2. Capacity Planning & Hardware Requests (`go/ineedhw` - Google internal)
If build or test execution on the new OS version is slower and the performance
regression is unavoidable (e.g., due to updated OS security controls, kernel
changes, or toolchain overhead), you must allocate additional hardware capacity
to preserve CQ cycle times and CI throughput.

- **Estimate Required Capacity:** Use the calculator at
  [`go/estimating-bot-capacity`](https://goto.google.com/estimating-bot-capacity)
  (*Google internal*) to determine the target fleet size.
  - **Formula / Example:** If OS version N requires X bots to handle workload
    demand, and OS version N+1 is 20% slower, you will need an extra 20%
    capacity (1.2 * X bots) to maintain identical throughput.
- **File a Hardware Request:** Request the additional bot capacity via
  [`go/ineedhw`](https://goto.google.com/ineedhw) (*Google internal*). State the
  business justification clearly (e.g., *"OS migration for Chromium builder
  `<builder_name>` from OS N to OS N+1 requires 20% additional bot capacity due
  to observed runtime regressions"*).

---

## Phase 2: Test Greening & Handling OS-Specific Failures

When running the FYI builder on the new OS version, it is common for certain
test suites or individual tests to fail due to OS API changes, updated system
dependencies, font differences, or updated security policies.

Before migrating production CI/CQ builders to the new OS version, the FYI
builder must reach a **"green" (passing) state**.

### Workflow for Handling Test Failures

If tests or test suites fail on the new OS version, follow this decision
process:

```
                  ┌──────────────────────────────┐
                  │  Test Fails on New OS Version│
                  └──────────────┬───────────────┘
                                 │
                   Is the fix obvious or easy?
                                 │
                   ┌─────────────┴─────────────┐
                   │                           │
                [ YES ]                     [ NO ]
                   │                           │
        ┌──────────┴──────────┐     ┌──────────┴──────────┐
        │ Implement & Submit  │     │ Disable Test ONLY   │
        │ Code/Fix directly   │     │ on New OS Version   │
        └─────────────────────┘     └──────────┴──────────┘
                                               │
                                    ┌──────────┴──────────┐
                                    │ File Tracking Bug   │
                                    │ against Test Owner  │
                                    └─────────────────────┘
```

#### 1. Fix Easy / Obvious Failures Directly
If the root cause is straightforward (e.g., a missing OS package dependency,
minor flag adjustment, or updated baseline path), the migrating engineer should
land a fix directly in the codebase.

#### 2. Selective Disabling on the New OS Version
If the failure is complex, non-obvious, or outside the migrating engineer's area
of expertise, **do not let it block the OS upgrade**.
Instead, **selectively disable** the failing test or test suite **only on the
new OS version**:

- **Individual Tests (e.g., Web Tests / Browser Tests / GTest Expectation Files):**
  Use OS-specific expectation tags in the relevant expectation or filter file
  (e.g. `TestExpectations`, `gpu_test_expectations.txt`, or `filter` files) so the
  test continues running and passing on older OS versions while being skipped on
  the new OS version.

  *Example (Web Tests / GPU Expectations):*
  ```text
  # crbug.com/1234567 [ Ubuntu-24.04 ] fast/canvas/draw-image.html [ Failure ]
  # crbug.com/1234568 [ Win11 ] GlTests.SimpleRendering/0 [ Skip ]
  ```

- **Entire Test Suites in Starlark Target Specifications:**
  If an entire binary test suite fails on the new OS version and cannot run,
  temporarily exclude or filter the test suite for the new OS mixin/variant in
  [`//infra/config/targets/`](../../infra/config/targets/):
  ```starlark
  # Example: Exclude a failing suite specifically on the new OS variant/mixin
  targets.matrix_compound_suite(
      name = "linux_chromeos_gtests",
      matrix = {
          "browser_tests": {
              "mixins": [
                  # ...
              ],
              "remove_mixins": [
                  "ubuntu_24_04",  # Skip on Ubuntu 24.04 pending crbug.com/1234569
              ],
          },
      },
  )
  ```

#### 3. File Tracking Bugs against Test Owners
For **every** test or test suite disabled on the new OS version:
1. **File a Bug:** File a tracking bug under the appropriate component for the
   team owning the test/feature.
2. **Include Context:**
   - State the specific OS version upgrade (e.g., *"Test `FooBrowserTest.Bar`
     fails on Ubuntu 24.04 / macOS 15 / Win 11"*).
   - Attach build logs, stack traces, and links to failing FYI builder runs on
     Milo.
3. **Reference Bug in Code:** Annotate the disabled test expectation or Starlark
   comment with the tracking bug link (`crbug.com/XXXXXX`).
4. **Assign / CC Owners:** Assign or CC the component owners so they can
   investigate, fix, and re-enable the test once resolved.

---

## Phase 3: Dual OS Dimension Acceptance & Fleet Transition

Once tests run cleanly ("green") on the new OS version and capacity adjustments
are planned, you can begin transitioning the target production builder and
machine fleet to the new OS version.

### Dual OS Dimension Strategy
Rather than performing a hard cutover from the old OS version to the new OS
version overnight, the standard strategy is to update the builder's accepted
swarming dimensions in Starlark to **temporarily accept both OS versions**.

#### Benefits
- **Gradual Fleet Migration:** Lab operations and infrastructure teams can
  progressively re-image or add new machines to the bot pool without disrupting
  running builds.
- **Continuous Scheduling:** Swarming automatically assigns tasks to available
  bots running *either* the current OS version or the new OS version
  (`"OS_Old|OS_New"`).
- **Low Risk:** If an issue occurs during machine re-imaging, tasks seamlessly
  fall back to remaining bots on the existing OS version.

### Real-World Starlark Configuration

In Chromium's infrastructure configuration, builders specify abstract OS
constants (e.g., `os = os.MAC_DEFAULT`), while the central LUCI builder
configuration in `main.star` and target mixin definitions map these constants to
Swarming dimension **OR expressions** (`"Mac-14|Mac-15"`).

#### 1. Builder Definition (`//infra/config/subprojects/...`)
Builders reference abstract OS constants rather than hardcoded OS versions:

```starlark
# Example: Builder referencing os.MAC_DEFAULT
ci.builder(
    name = "Mac Builder Siso FYI",
    gn_args = "ci/Mac Builder",
    os = os.MAC_DEFAULT,
    cpu = cpu.ARM64,
)
```

#### 2. Central OS Dimension Override (`//infra/config/main.star`)
During an OS upgrade (e.g., macOS 14 to macOS 15), update
`os_dimension_overrides` in `main.star` to map `os.MAC_DEFAULT` to a dual OS
dimension expression using `|`:

```starlark
chromium_luci.configure_builders(
    enable_alerts_configuration = True,
    os_dimension_overrides = {
        # Temporarily accept both Mac-14 and Mac-15 bots during OS migration
        os.MAC_DEFAULT: "Mac-14|Mac-15",
        os.MAC_BETA: "Mac-15",
        os.WINDOWS_DEFAULT: os.WINDOWS_10,
    },
)
```

#### 3. Target Mixins (`//infra/config/targets/mixins.star`)
Similarly, update relevant test mixins to accept tasks on either OS version:

```starlark
targets.mixin(
    name = "mac_arm64",
    swarming = targets.swarming(
        dimensions = {
            "cpu": "arm64",
            "os": "Mac-14|Mac-15",
        },
    ),
)
```

---

### Incremental In-Place Fleet Migration

In practice, teams rarely have spare capacity to stand up a parallel fleet of
new OS bots to replace 100% of an existing builder's demand upfront. Instead, OS
upgrades are conducted as an **in-place migration**.

To avoid capacity bottlenecks, build queuing, or CQ cycle time spikes, bots
assigned to the builder (or shared testing pool) are removed and upgraded in
**small increments of 5% or less** of the total fleet size at any given time.

```
┌────────────────────────────────────────────────────────────────────────┐
│                      Fleet Bot Pool (100% Total)                       │
└────────────────────────────────────────────────────────────────────────┘
  │                                                                    │
  ├── 95%+ Active on Current OS (Serving CQ/CI Workloads)               │
  │                                                                    │
  └── <= 5% Quarantined / Re-imaged to New OS (Incremental In-Place)   │
```

#### A. Physical Hardware Fleet (e.g., Macs, iOS, Android Devices)
For bare-metal physical hosts and connected test devices (such as Mac mini
pools, iOS test beds, or Android device labs):

1. **Update Configs to Dual OS Acceptance:** Land the Starlark CL setting
   `os_dimension_overrides` to `"Mac-14|Mac-15"` (as seen in real-world CLs like
   [`chrome-internal-review CL 9313101`](https://chrome-internal-review.googlesource.com/c/chrome/src-internal/+/9313101)
   - *Google internal*).
2. **File a Fleet Operations Ticket:** File a ticket with the **Fleet Systems /
   Lab Operations** team requesting an OS upgrade for the target physical device
   pool.
3. **Quarantine & Re-image in 5% Batches:** Lab operators temporarily place a
   small batch (≤ 5% of the fleet) into quarantine, wipe and re-image the hosts
   with the new OS version (e.g. macOS 15), and reconnect them to Swarming.
4. **Verify & Repeat:** Once the initial batch passes health checks and
   successfully processes jobs on the new OS version, repeat the process for
   subsequent 5% batches until 100% of the physical hardware fleet is upgraded.

#### B. Virtual Machine Fleet (GCE Bots)
For Google Compute Engine (GCE) cloud-based virtual bot pools, bot allocation
and auto-scaling limits are managed via internal infrastructure configuration
files in [`infradata/config`](../../infra/config) (*Google internal*).

1. **Locate GCE Pool Configuration:** Open the relevant bot pool definition file
   (such as
   `data/config/configs/chromium-swarm/starlark/bots/chromium/tests.star` in
   `infradata/config`).
2. **Real-World Config Example (e.g., Windows 10 -> Windows 11 Migration):**
   The bot pool (e.g., `chromium.tests`) defines separate GCE bot declarations
   for each OS version (such as `chrome.gce_win10_22h2` and
   `chrome.gce_win11_24h2`).

   ```starlark
   # Win10 test fleet
   chrome.gce_win10_22h2(
       prefix = "chrome-windows-10-main",
       amount = chrome.robocrop(
           min_amount = 100,
           max_amount = 3700,
       ),
       resource_group = "chrome-desktop-robocrop",
       machine_type = "e2-standard-8",
   ),
   # Win11 test fleet
   chrome.gce_win11_24h2(
       prefix = "chrome-windows-11",
       amount = chrome.robocrop(
           min_amount = 30,
           max_amount = 150,
       ),
       machine_type = "e2-standard-8",
   ),
   ```

3. **Gradually Shift Robocrop Allocation Amounts:**
   To perform an in-place OS migration for builders using this pool, gradually
   **decrease** the bot capacity (`min_amount` and `max_amount` in
   `chrome.robocrop`) assigned to the older OS bot group (Windows 10) by a small
   percentage (≤ 5%), and **increase** the allocation for the new OS bot group
   (Windows 11) by the exact same amount.

4. **Land Config & Monitor:** Land the `infradata/config` CL. The GCE provider /
   Robocrop autoscaler will automatically drain instances running the old OS
   image and spin up replacement instances running the new OS image. Monitor
   Swarming queue times and task success rates before performing the next
   allocation shift.

---

## Phase 4: Updating Try / CQ Builder Mirroring Specifications

Try (Commit Queue) builders pick up their test configurations and compile
settings by **mirroring** one or more CI builders.

### Scenarios for OS Upgrades:

1. **Generic CI Builder Migration (No Change Needed):**
   If the mirrored CI builder itself is migrated to a new OS version (e.g., via
   central `os_dimension_overrides` in `main.star`), no changes are needed in the
   Try builder's declaration.

2. **OS-Versioned CI Builder Migration:**
   Some Try builders specifically mirror CI builders that explicitly target a
   specific OS version.
   *Example:* The `android-x64-rel` Try/CQ builder specifically mirrors the
   `ci/android-15-x64-rel` CI builder. When upgrading the default CQ builder from
   Android 15 to Android 16, the Try builder declaration must be updated to mirror
   `ci/android-16-x64-rel` instead.

### Starlark Configuration Example

In [`../../infra/config/subprojects/chromium/try/`](../../infra/config/subprojects/chromium/try/)
(e.g., `try.star` or `try/subproject.star`), update the `mirrors` list and
`gn_args` config references:

```starlark
# Example: Updating try builder android-x64-rel to mirror Android 16 CI builder
try_.orchestrator_builder(
    name = "android-x64-rel",
    mirrors = [
        "ci/android-12l-x64-rel-cq",
        # Updated from ci/android-15-x64-rel to ci/android-16-x64-rel
        "ci/android-16-x64-rel",
        "ci/android-webview-13-x64-hostside-rel",
    ],
    gn_args = gn_args.config(
        configs = [
            # Updated to match new Android 16 CI builder config
            "ci/android-16-x64-rel",
            "release_try_builder",
        ],
    ),
)
```

---

## Phase 5: Safe Production Cutover & Disabling Gardening Alerts

When applying OS configuration changes to production CI builders, engineers can
temporarily place the builder in **experimental mode** or **disable
gardening/sheriff alerts** until the migration change is verified to be safe on
live infrastructure.

### 1. Temporarily Disabling Gardening Alerts & Tree Closing
To prevent accidental sheriff alerts or waterfall tree closures while verifying
a production builder on a new OS version:

```starlark
ci.builder(
    name = "linux-rel",
    # Temporarily set gardener rotations and tree closing to None during OS cutover
    gardener_rotations = None,
    tree_closing_notifiers = None,
    # ...
)
```

### 2. Experimental Mode
Alternatively, set `is_experimental = True` (or `experimental = True`) on the
builder declaration. Experimental builders continue executing builds and
publishing results to Milo, but their build status does not affect CQ or sheriff
rotations.

### 3. Restoring Sheriff Alerts
Once the builder successfully completes several build cycles on the new OS
version without infrastructure errors:
1. Re-enable `gardener_rotations` and `tree_closing_notifiers`.
2. Remove experimental flags.
3. Submit the Starlark configuration CL.

---

## Phase 6: Lock Builders to New OS Version & Post-Migration Cleanup

Once 100% of the bot fleet (physical or virtual) has been re-imaged and verified
on the new OS version, complete the migration with post-rollout cleanup:

1. **Lock Builder & Mixins to New OS:**
   Update `os_dimension_overrides` in `main.star` and mixin definitions in
   `mixins.star` to point exclusively to the new OS version (e.g., `os.MAC_DEFAULT:
   "Mac-15"`).

2. **Decommission Temporary FYI Builder:**
   Remove the temporary FYI builder created in Phase 1 (`linux-rel-2404-fyi`)
   from Starlark.

3. **Clean Up Temporary Test Expectations:**
   Audit and remove temporary OS expectation tags (e.g., `[ Ubuntu-24.04 ]` or
   `[ Win11 ]`) or mixin exclusions as test component owners land fixes for
   disabled tests.

4. **Land Final Cleanup CL:**
   Land the final Starlark configuration CL to complete the OS upgrade
   lifecycle.
