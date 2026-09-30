# Chromium GPU Gardening & Flake Root-Cause Strategy

This guide outlines the operational workflow for the **GPU Gardener (Pixel Wrangler)** shift, paired with a deep-dive playbook for **root-causing and resolving flakes** rather than only suppressing them. It is designed to be used both by human wranglers and by automated/AI gardening agents.

[TOC]

---

## 1. Waterfalls, Shift Overview & Decision Flow

### Monitored Waterfalls & Dashboards
* **[Sheriff-O-Matic (`chromium.gpu`)](https://sheriff-o-matic.appspot.com/chromium.gpu)**: Primary alert view aggregating failures across the GPU waterfalls.
* **[`chromium.gpu` Console](https://ci.chromium.org/p/chromium/g/chromium.gpu/console)**: Core GPU waterfall running on physical GPU hardware (Windows, Mac, Linux, Android, ChromeOS).
* **[`chromium.gpu.fyi` Console](https://ci.chromium.org/p/chromium/g/chromium.gpu.fyi/console)**: Extended hardware/driver matrix and **ToT ANGLE** (`got_angle_revision` / `parent_got_angle_revision` rather than `DEPS` ANGLE). Coordinate with the [ANGLE Wrangler](https://chromium.googlesource.com/angle/angle/+/main/infra/ANGLEWrangling.md) for ANGLE-only regressions.
* **[`chromium.swangle` Console](https://ci.chromium.org/p/chromium/g/chromium.swangle/console)**: Secondary waterfall running ANGLE's GLES implementation on SwiftShader Vulkan. Regressions are mostly handled by the [ANGLE Wrangler](https://chromium.googlesource.com/angle/angle/+/main/infra/ANGLEWrangling.md), except for Chromium/WebGL-side regressions.
* **Checking Bot Liveness (Offline / Gray Columns)**: Periodically inspect the console views directly. If a builder's Swarming pool goes offline for an extended period, the top summary bubble on the console may remain green while the builder's column turns **gray**. If bots stop processing jobs or fail with deep infrastructure errors (source checkout or isolate failures), file a P1 bug under `Infra>Labs`, `Infra>Troopers`, and `Internals>GPU>Testing` (see [Contacting Troopers](https://chromium.googlesource.com/infra/infra/+doc/HEAD/doc/users/contacting_troopers.md) / [g.co/bugatrooper](https://g.co/bugatrooper)).

### End-to-End Triage & Decision Flow

```
Alert on Sheriff-O-Matic (chromium.gpu)
  |
  +--> Compile / Deterministic Regression
  |      +--> Check Blamelist & Trim via trim_culprit_cls.py
  |      +--> Revert Culprit CL & File P1 Bug (Hotlist 5853370)
  |
  +--> Skia Gold Pixel Diff
  |      +--> Intentional Rendering Change? -> Approve Positive Image in Gold UI
  |      +--> Corrupted / Unexpected Render? -> Mark Negative in Gold + Investigate / Revert
  |
  +--> Purple Bot / Device Unreachable / Offline Column
  |      +--> Run find_bad_machines.py / Check Swarming Bot
  |      +--> File Infra>Labs Trooper Bug (Hotlist 5853370)
  |
  +--> Intermittent / Flaky Failure
         +--> Run Phase 2 Flake Root-Cause Pipeline
                +--> Root Cause Found Quickly -> Land Fix or Revert + Close Bug
                +--> Needs Deeper Investigation -> Land Narrowest Expectation
                     (Slow / RetryOnFailure) + Attach Diagnostic Dossier to Bug
```

> **Note on Transient Flakes**: Sheriff-O-Matic automatically clears an alert as soon as a builder's latest build passes. To proactively scan a builder's recent history (e.g., the last 200 builds) for recurring test failures or `INFRA_FAILURE`s that have since turned green:
> ```bash
> prpc call cr-buildbucket.appspot.com buildbucket.v2.Builds.SearchBuilds <<EOF
> {
>   "predicate": {
>     "builder": {
>       "project": "chromium",
>       "bucket": "ci",
>       "builder": "<BUILDER_NAME>"
>     }
>   },
>   "pageSize": 200,
>   "mask": {
>     "fields": "id,number,status,summary_markdown,create_time,start_time,end_time"
>   }
> }
> EOF
> ```

### CLI Quick-Reference for Alert & Log Inspection
When triaging from the command line or via an automated agent:
1. **Fetch active Sheriff-O-Matic alerts as JSON** (since the web UI is a client-side SPA):
   ```bash
   prpc call sheriff-o-matic.appspot.com infra.appengine.som.v1.Alerts.ListAlerts <<EOF
   {"parent": "trees/chromium.gpu"}
   EOF
   ```
2. **Inspect a failing Buildbucket build's steps, Swarming task links, and ANGLE/Chromium revisions**:
   ```bash
   bb get -A -json <build_id>
   ```
3. **Query unexpected test failures and full crash logs (`typ_stderr`) from ResultDB**:
   * Note that Swarming task links end in `0` (e.g., `7b0c0568c721dc10`), whereas the corresponding ResultDB invocation ID replaces the trailing `0` with `1` (`task-chromium-swarm.appspot.com-7b0c0568c721dc11`):
   ```bash
   # List unexpected test results in the Swarming shard:
   rdb query -json -u -n 50 task-chromium-swarm.appspot.com-<task_id_ending_in_1>

   # Get a signed URL to download the full browser log and symbolized Crashpad minidumps:
   rdb rpc luci.resultdb.v1.ResultDB GetArtifact <<EOF
   {"name": "<testResult.name>/artifacts/typ_stderr"}
   EOF
   ```

---

## 2. Phase 1: Rapid Triage & Keeping the Tree Green

### A. Deterministic Breakages & Blamelist Narrowing
1. **Check `chromium.gpu` vs. `chromium.gpu.fyi`**:
   * Remember that `chromium.gpu.fyi` tracks **ToT ANGLE** (`got_angle_revision` on builders / `parent_got_angle_revision` on testers), whereas `chromium.gpu` uses DEPS ANGLE. If only FYI bots fail, compare `parent_got_angle_revision` between the last green and first red build (`git log <old>..<new>` in `third_party/angle`) and coordinate with the ANGLE Wrangler.
2. **Narrow & Trim Large Blamelists**:
   * **Intersect across builders**: If the same test starts failing across multiple `chromium.gpu` or `chromium.gpu.fyi` bots, intersect their regression ranges (`max(start_rev) .. min(end_rev)`) to narrow the candidate commits before inspecting individual CLs.
   * **Trim via CQ trybot history**: When a failure occurs on a configuration covered by an optional GPU trybot (e.g., `win_optional_gpu_tests_rel`, `linux_optional_gpu_tests_rel`, `mac_optional_gpu_tests_rel`), run [`//content/test/gpu/trim_culprit_cls.py`](/content/test/gpu/trim_culprit_cls.py) to eliminate CLs in the blamelist that already passed that trybot on CQ:
     ```bash
     vpython3 content/test/gpu/trim_culprit_cls.py \
       --start-revision <oldest_rev> \
       --end-revision <newest_rev> \
       --trybot win_optional_gpu_tests_rel \
       --project chrome-unexpected-pass-data
     ```
3. **Revert Policy**:
   * Revert culprits immediately on `chromium.gpu`, and on `chromium.gpu.fyi` when confident in the culprit CL. Include build links and **copy-pasted log excerpts** in the revert description and P1 bug (since raw Swarming/bot logs expire after a few days).
   * Use `rubber-stamper@appspot.gserviceaccount.com` for clean reverts (if you are not yet a Chromium committer, ask a GPU team committer to help land the revert or review).

### B. Skia Gold Pixel Failures
For failing `pixel_skia_gold_*` steps (see [`gpu_pixel_testing_with_gold.md`](/docs/gpu/gpu_pixel_testing_with_gold.md) for full details):
1. **Locate the Gold Triage Link** in the failing build step:
   * **Fewer than 10 untriaged images**: Follow the individual `gold_triage_link for <test name>` links (or `triage_link_for_entire_cl for <test name>` on trybots).
   * **10 or more untriaged images**: Follow the single `Too many artifacts produced to link individually, click for links` artifact link (or search `https://chrome-gold.skia.org/search?query=name%3D<test_name>`).
2. **Triage in Gold UI (Human-in-the-Loop Only — sign in with `@google.com` or `@chromium.org`)**:
   * **Important for AI / Automated Agents**: Agents must **never** autonomously approve or mark images positive/negative in Skia Gold (via the Gold UI, API, or `goldctl`), as Gold baseline changes take effect immediately without a CL review. Agents should only report the `gold_triage_link`, summarize the failure context, and leave image triage to a human wrangler.
   * **Intentional rendering / baseline change**: Approve the new positive image in Gold.
   * **Corrupted or unexpected rendering**: Mark the image as **negative** in Gold, file a bug on `crbug.com` (`Internals>GPU>Testing` + domain component such as `Blink>WebGL` or `Blink>Canvas`), and add a temporary expectation in [`//content/test/gpu/gpu_tests/test_expectations/pixel_expectations.txt`](/content/test/gpu/gpu_tests/test_expectations/pixel_expectations.txt) (`[ RetryOnFailure ]` for flakes or `[ Failure ]` for consistent failures) so the builder turns green while the bug is investigated.
   * **Subtle Noise vs. Real Bug**: If a test produces minor 1–2 pixel anti-aliasing jitter across runs, use [`//content/test/gpu/determine_gold_inexact_parameters.py`](/content/test/gpu/determine_gold_inexact_parameters.py) to configure fuzzy/Sobel matching in [`//content/test/gpu/gpu_tests/pixel_test_pages.py`](/content/test/gpu/gpu_tests/pixel_test_pages.py) rather than approving endless goldens or suppressing the test.

---

## 3. Phase 2: Deep-Dive Flake Analysis (Moving Beyond Blind Suppressions)

For flaky tests, the blamelist on the single failed build shown in Sheriff-O-Matic is usually misleading—the underlying bug was introduced in an earlier build, and the test happened to pass for several builds before failing. To **actually resolve flakes**, use a 5-step diagnostic pipeline before (or right after) landing a temporary suppression.

### Step 1: Find the True Temporal Onset via BigQuery (ResultDB)
Instead of trusting the build where the flake surfaced on SoM, query `chrome-luci-data.chromium.gpu_ci_test_results` (the same table used by [`//content/test/gpu/flake_suppressor/gpu_queries.py`](/content/test/gpu/flake_suppressor/gpu_queries.py)) to find the **exact first timestamp, Swarming bots, and build** where the test started flaking across *all* CI builders:

```bash
bq query --project_id=chrome-unexpected-pass-data --use_legacy_sql=false '
SELECT
  DATE(partition_time) AS day,
  (SELECT value FROM tr.variant WHERE key = "builder") AS builder,
  status,
  COUNT(*) AS runs,
  MIN(partition_time) AS first_seen,
  MAX(partition_time) AS last_seen,
  STRING_AGG(DISTINCT (SELECT value FROM tr.tags WHERE key = "bot_id" LIMIT 1), ", ") AS bots,
  ANY_VALUE(failure_reason.primary_error_message) AS sample_error
FROM `chrome-luci-data.chromium.gpu_ci_test_results` tr
WHERE test_id LIKE "%<TEST_NAME>%"
  AND partition_time > TIMESTAMP_SUB(CURRENT_TIMESTAMP(), INTERVAL 14 DAY)
  AND status != "PASS"
  AND status != "SKIP"
  AND NOT expected
GROUP BY day, builder, status
ORDER BY first_seen ASC'
```
* **Why this works**: If a test had 0 failures for 30 days and suddenly started flaking at 5% across 3 builders on Tuesday afternoon, the **earliest failing build across all builders** gives you a tight, actionable blamelist—turning an "un-bisectable flake" into a standard regression range.

### Step 2: Isolate Bad Hardware vs. Code Flakes
Because GPU tests run on physical hardware in `chromium.tests.gpu`, a single degraded GPU, thermal throttling, KVM/EDID glitches, or wedged driver state on one Swarming bot looks identical to a 10% code flake on a builder with 10 machines in its pool.
* **Check Swarming Bot Distribution**: Run [`//content/test/gpu/find_bad_machines.py`](/content/test/gpu/find_bad_machines.py) for the affected mixin (e.g., `win10_nvidia_gtx_1660_stable`, `gpu_pixel_6_stable`, `mac_arm64_apple_m2_retina_gpu_stable`):
  ```bash
  vpython3 content/test/gpu/find_bad_machines.py \
    --mixin <mixin_name> \
    --sample-period 7
  ```
* If failures cluster on 1–2 `bot_id`s (flagged by `DetectViaStdDevOutlier` / `DetectViaRandomChance`), **do not suppress the test**—file an `Infra>Labs` bug with the specific Swarming bot IDs (`https://chromium-swarm.appspot.com/bot?id=<bot_id>`) to quarantine/repair the machines.

### Step 3: Check Shard Position & Cross-Test State Pollution
Inspect the failing Swarming shard log (`shard #N`) for **execution order effects**:
* **First-test-in-shard flake**: Browser/GPU process cold-start race, shader disk cache initialization, or DevTools websocket connection timeout on startup.
* **Always fails after Test X**: GPU context state leak, uncleaned shared image/mailbox, or VRAM exhaustion from the preceding test in the same shard.
* **Parallel `--jobs=N` contention**: In suites running with `--jobs=4` (`webgl1_conformance`, `webgl2_conformance`), multiple browser instances compete for the physical GPU.

### Step 4: Match Against the 5 Common GPU Flake Archetypes

| Failure Signature / Symptom | Likely Root Cause Archetype | How to Confirm & Fix |
| :--- | :--- | :--- |
| `Timed out waiting for websocket message` or 300s global timeout (especially on `Debug` / `ASAN`) | **Heartbeat / Heavy Test Timeout** (not a functional bug) | First apply `[ Slow ]` in expectations (multiplies heartbeat timeout in `webgl*_conformance` and `webgpu_cts`). If a single test does massive allocations in Debug, split the test. |
| Black/old frame captured in `pixel_tests` or `trace_test` (`expected_colors` mismatch) | **Missing Swap Promise / Premature Screenshot** | Test HTML triggers `chrome.gpuBenchmarking` or completes before `requestAnimationFrame` / swap promise / compositor frame submission settles. Fix in `content/test/data/gpu/*.html` by waiting for frame presentation. |
| `SyncToken` DCHECK, missing texture tile, or intermittent `GL_INVALID_OPERATION` | **Cross-Process SyncToken / Ordering Race** (see [`sync_token_internals.md`](/docs/gpu/sync_token_internals.md)) | Unverified sync token sent across Mojo pipes, or missing `WaitSyncTokenCHROMIUM` before Skia/Display Compositor reads a SharedImage. Enable `--enable-gpu-service-logging` to trace command order. |
| Flaky `ContextLost_*` or `Pixel_Video_Context_Loss_*` timeouts | **Context Recovery / IPC Race** | Browser/Renderer loses track of GPU channel re-establishment or media decoder reset callback drops during context loss. Check `gpu_channel_manager` / `ContextProvider` loss listeners. |
| Media / WebCodecs / Hardware Decode sporadic failures on Mac or Android | **Platform Decoder / ColorSpace / Buffer Pool Edge Case** | Inspect media log in Swarming output; often caused by hardware decoder session resets or surface texture exhaustion when tests run back-to-back. |

### Step 5: High-Iteration Targeted Reproduction (Swarming & Local)
When you have a hypothesis or want to add `DLOG(ERROR)` / `--enable-gpu-client-logging` / `--enable-gpu-service-logging` to catch a flake in the act:

1. **Option A — Trigger Swarming directly from local build via `mb.py`** (see [`gpu_testing.md`](/docs/gpu/gpu_testing.md)):
   Grab the `gpu`, `os`, and `pool` dimensions from the failing Swarming task and run `--repeat=20` (or `--gtest_repeat=20`):
   ```bash
   tools/mb/mb.py run -s --no-default-dimensions \
     -d gpu <pci_id_and_driver> -d os <os_dim> -d pool chromium.tests.gpu \
     out/Release telemetry_gpu_integration_test -- \
     --isolated-script-test-output '${ISOLATED_OUTDIR}/output.json' \
     <suite_name> --show-stdout --browser=release --passthrough -v \
     --test-filter=<flaky_test_name> --repeat=20
   ```
2. **Option B — Repeat on Trybots via `test_suite_exceptions.pyl`**:
   If you can't cross-compile locally (e.g., Mac/Win from Linux), edit [`//testing/buildbot/test_suite_exceptions.pyl`](/testing/buildbot/test_suite_exceptions.pyl) to filter to just the flaky test with `--repeat=30`, run `testing/buildbot/generate_buildbot_json.py`, upload a draft CL with extra logging, and trigger the specific `gpu-fyi-try-*` bot.
3. **Option C — Reproduce Locally**:
   * **From a Swarming task**: Open the failed shard's Swarming task page and copy the command at the bottom of the left panel ("Reproducing the task locally") to download the isolated build and inputs directly.
   * **From a local Chromium build**: Run [`//content/test/gpu/run_gpu_integration_test.py`](/content/test/gpu/run_gpu_integration_test.py) with `--browser=exact`:
     ```bash
     vpython3 content/test/gpu/run_gpu_integration_test.py <suite_name> \
       --browser=exact --browser-executable=out/Release/chrome \
       --passthrough -v --test-filter=<flaky_test_name> --repeat=20
     ```

---

## 4. Phase 3: Expectation Hygiene & Buganizer Workflow

When a failure or flake cannot be fixed within the shift's immediate triage window, suppress it cleanly so the tree stays green while preserving maximum signal for root-causing.

### 1. Telemetry-Based Suites: Expectation Hierarchy (Least to Most Destructive)
Most GPU integration suites (`webgl_conformance`, `pixel`, `trace_test`, `context_lost`, `webcodecs`, `webgpu_cts`, `gpu_process`, etc.) use Telemetry expectations in [`//content/test/gpu/gpu_tests/test_expectations/`](/content/test/gpu/gpu_tests/test_expectations/) (see [`gpu_expectation_files.md`](/docs/gpu/gpu_expectation_files.md)):
1. **`[ Slow ]`**: Always try first for heartbeat websocket timeouts in `webgl_conformance`, `webgl2_conformance`, and `webgpu_cts`.
2. **`[ RetryOnFailure ]`**: Preferred for genuine low/moderate-rate flakes. Keeps running the test and passes if retry succeeds.
3. **`[ Failure ]`**: Use when failing deterministically or flaking >50% of builds so retries still fail the shard. Prefer `[ Failure ]` over `[ Skip ]` even for consistent failures because it still executes the test, allowing [`//content/test/gpu/unexpected_pass_finder.py`](/content/test/gpu/unexpected_pass_finder.py) to detect when the underlying issue is fixed.
4. **`[ Skip ]`**: **Last resort only** (enforced by presubmit)—use only when the test wedges the GPU/device, causes cascading shard timeouts, or is permanently unsupported on that platform.

**Important**:
* Specify the narrowest possible tags from the file header (e.g., `[ win10 nvidia-0x2184 release angle-d3d11 ]` rather than `[ win nvidia ]`). You can use [`//content/test/gpu/suppress_flakes.py`](/content/test/gpu/suppress_flakes.py) to auto-generate non-conflicting tag sets, and run [`//content/test/gpu/validate_tag_consistency.py`](/content/test/gpu/validate_tag_consistency.py) / `git cl presubmit` to verify no tag conflicts exist.
* A proper review is preferred, but for urgent expectation suppressions when no reviewer is available, you may add `rubber-stamper@appspot.gserviceaccount.com` alongside a regular GPU reviewer.

### 2. GTest-Based Suites & Filter Files
For C++ GTest suites (`gl_tests`, `gl_unittests`, `angle_unittests`, `dawn_end2end_tests`, `tab_capture_end2end_tests`, etc.):
* **Source-level suppression**: Prefix the test name with [`DISABLED_`](https://github.com/google/googletest/blob/main/docs/advanced.md#temporarily-disabling-tests) (or `MAYBE_` with `#ifdef` for platform-specific disabling).
* **Builder/Config filter files**: When a GTest suite uses a `.filter` file for a specific configuration, add `-SuiteName.TestName` to the relevant file in [`//testing/buildbot/filters/`](/testing/buildbot/filters/).

### 3. Filing High-Signal Bugs & Hotlist `5853370`
Every expectation **must** reference a `crbug.com/` ID added to the **Pixel-Wrangler Hotlist (`https://issues.chromium.org/hotlists/5853370`)**. To make flake bugs actionable for domain owners:
* **Component**: `Internals>GPU>Testing` + domain component (`Blink>WebGL`, `Blink>WebGPU`, `Internals>GPU>Video`, `Internals>Skia>Compositing`, etc.).
* **Diagnostic Dossier in Description**:
  1. Exact failing builder(s), step name, and Swarming task dimensions (`gpu`, `os`).
  2. **True onset date/revision range** from the Step 1 BigQuery query (even if wide).
  3. **Copy-pasted stack trace / assertion / log snippet** (symbolized if possible).
  4. Whether `find_bad_machines.py` ruled out a single bad Swarming host.
