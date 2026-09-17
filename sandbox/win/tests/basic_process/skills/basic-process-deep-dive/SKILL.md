---
name: basic-process-deep-dive
description: >-
  Create focused Chromium Basic Process Windows API deep-dive reports from
  existing audit data or a fresh test run, with symbolized call stacks, fuzzy
  owner attribution, and prioritized remediation.
---

# Basic Process deep-dive reports

Use this skill for focused reports derived from the Basic Process Windows API
audit data.

## Select the input data

Choose one of two workflows:

- **Analyze existing data:** use the raw logs or symbolized CSV explicitly
  selected by the user. Do not rebuild or rerun the workload unnecessarily.
- **Collect fresh data:** select the workload and configured build directory,
  then capture a new run using the recipe below or another supported workload.

If the run, workload, or output location is ambiguous, ask before proceeding.
Do not silently select a historical path or whichever log is newest. EchoService
is one capture recipe, not a restriction on the data this skill can analyze.

Treat the selected `api_callstack_report.csv` (or equivalent full-depth CSV) as
the authoritative stack source. Preserve every captured frame, module + RVA,
symbol, source location when available, source stack ID, observation count, and
loader/resolver detail. Keep the raw logs alongside derived artifacts.

Do not mix evidence from unrelated runs. For comparisons, name the baseline and
candidate explicitly, keep their artifacts and totals separate, and state
differences in revision, configuration, workload, and process role. Use source
code to interpret evidence, not to invent runtime observations.

## Run provenance

Record the following with each data set and summarize it in the report:

- Source revision, relevant local changes (including staged and untracked
  sources), and CL/patch set when known.
- Build directory, `args.gn` and relevant effective GN values, target
  architecture, OS version, and tool versions.
- Exact build/test commands, test filter, runtime switches, measured process
  roles/PIDs, capture time, and build/test exit status.
- Original and rewritten executable identities, shim identity, and matching
  PDB identities. Record hashes and PE/PDB matching information, not just names.
- Paths to build/test logs, launcher results, raw audit logs, symbolized CSV,
  and any parsing or symbolization diagnostics.

For supplied artifacts, record what is known and mark missing provenance
explicitly. Do not substitute today's checkout or binaries for the recorded
run. Preserve matching binaries and symbols, or an immutable way to retrieve
them, before a later build overwrites them.

## Optional fresh capture: EchoService

This recipe exercises startup, a Mojo EchoService round trip, and shutdown.
It audits an unsandboxed utility child; it does not demonstrate enforcement of
an OS Basic Process sandbox or cover renderer workloads.

### Configure and build

From the Chromium source root, use the fixture instructions in
`sandbox\win\tests\basic_process\README.md`. Confirm the selected revision has
the fixture, and inspect the build arguments rather than overwriting an
existing configuration. Required settings are Windows, a non-component build,
`is_asan = false`, and `is_basic_api_shim = true`. A typical release setup is:

```gn
is_debug = false
is_component_build = false
is_asan = false
is_basic_api_shim = true
```

Keep symbols enabled for the code being analyzed. Higher symbol levels may be
needed for useful source/inline attribution. Use the enlistment's configured
toolchain and an appropriate concurrency limit; do not change global settings.

Example PowerShell commands, with the build directory adjusted as needed:

```powershell
$out = "out\basic_process"
$stamp = Get-Date -Format "yyyyMMdd_HHmmss_fff"
$run = Join-Path $PWD.Path "out\api_reports\echo_service_$stamp"
New-Item -ItemType Directory -Path $run -ErrorAction Stop | Out-Null

gn gen $out *> "$run\generate.log"
if ($LASTEXITCODE -ne 0) { throw "GN generation failed; see $run\generate.log" }
$shim = gn args $out --list=is_basic_api_shim --short
if ($LASTEXITCODE -ne 0) { throw "Could not inspect is_basic_api_shim" }
if ($shim -notcontains "is_basic_api_shim = true") {
    throw "Enable is_basic_api_shim in the selected build directory"
}

$targets = @(
    "content_browsertests"
    "content/test:basic_process_echo_service_fixture"
)
autoninja -C $out @targets *> "$run\build.log"
if ($LASTEXITCODE -ne 0) { throw "Build failed; see $run\build.log" }
```

Save the provenance above and confirm the checkout did not change during the
build before running the test.

### Run and retain the raw evidence

Always use a fresh artifact directory: APIFW appends to its log, so reusing a
path mixes runs. Use a new directory for each repeat or baseline measurement.

```powershell
$test = "BasicProcessEchoServiceBrowserTest.AuditsEchoServiceApiCalls"
$testArgs = @(
    "--gtest_filter=$test"
    "--test-launcher-jobs=1"
    "--test-launcher-retry-limit=0"
    "--test-launcher-summary-output=$run\results.json"
    "--basic-process-api-test-log-file=$run\api_calls.log"
)
& "$out\content_browsertests.exe" @testArgs *> "$run\test.log"
$testExit = $LASTEXITCODE
if ($testExit -ne 0) {
    throw "Test failed ($testExit); retain $run as partial evidence"
}
if (!(Test-Path "$run\api_calls.log") -or
    (Get-Item "$run\api_calls.log").Length -eq 0) {
    throw "No audit data was captured"
}
```

Confirm the launcher JSON names the requested test and records a passing run;
zero matching tests or a crash after a passing test body is not success.
Keep failed runs, but label their evidence as partial rather than silently
reporting them as successful.

The fixture supplies `--basic-process-api-test-audit` and
`--basic-process-api-test-log-call-sites` to the measured child. Passing the
log-file switch to the test retains its otherwise temporary audit log. Verify
these behaviors in the selected revision if the fixture has changed.

### Parse and symbolize

The test produces raw logs, not the final report. Use a parser/symbolizer that
matches the selected log format, and retain its script and invocation with the
run. Do not rely on unbundled helpers in an old report directory without
inspecting and validating them.

- Preserve original records and account for malformed, truncated, interleaved,
  or unparsed data. Do not silently drop frames or infer successful recovery.
- Inspect the selected logger's buffer limit. `BoundedLine<1024>` can truncate
  a record at 1,023 characters, including its newline, so the next `[pid] stub`
  or `[pid] stub-site` prefix may be concatenated onto it. Preserve the raw
  fragment and mark the record partial. Recover only demonstrably complete
  frames; a truncated RVA can still look like valid hexadecimal. A line-based
  test summary can miss the joined records, so explain any count difference
  rather than forcing recovered-record totals to match it.
- Reconcile record types, PIDs, APIs, and stack counts with the capture output.
  Verify aliases against the shim's exports before normalizing API names;
  retain the original names for traceability.
- Use the exact executable and matching PDB from the run. The rewritten
  EchoService image normally retains `content_shell.exe.pdb` as its PDB
  reference; verify the match rather than relying on that filename.
- Log addresses are module-relative RVAs. If a symbolizer expects absolute
  addresses, add the matched image's preferred image base, or use its explicit
  relative-address mode. Never hardcode the image base or symbolize an old log
  against a freshly rebuilt binary.
- Check symbolizer errors and output/address correspondence. Retain unresolved
  frames with module + RVA and report their count; never invent symbols.
  Prefer structured output, such as `llvm-symbolizer --relative-address
  --output-style=JSON`, and verify each response address matches its input.
  Preserve inline frames without shifting the original captured-frame indices.

Write the full-depth CSV before trimming or grouping stacks. Then follow the
report, ownership, deduplication, and rendering instructions below.

## Evidence and counting limits

- Directly forwarded allowlisted APIs are not logged. Describe the result as
  observed shim activity, not every Windows API invocation or proof that all
  unobserved APIs are unused.
- `stub` records are not universally invocation counts: some loader thunks
  deduplicate repeated arguments, and modified thunks may log conditionally.
- `stub-site` records are deduplicated by thunk, captured stack, and detail in
  the current implementation. They do not provide per-site invocation counts.
  If each retained record has weight 1, label it as a captured call-site
  observation, not as one API call. Preserve actual observation weights when
  an existing data set supplies them.
- Define `Observations` for the selected data set. Keep logged-record totals
  separate from stack totals; do not distribute an API's record count across
  its stacks or represent an unknown count as zero.
- Report tripwire hits and failed runs explicitly. No tripwire hits during
  audit forwarding does not establish compatibility with the restricted API
  surface.
- Separate fixture/startup artifacts from proposed production requirements.
  Differences between runs suggest an effect; comparable before/after runs
  are needed to attribute it to a change.

## Required report structure

Every report must contain:

1. A clear focus area and title.
2. The inclusion rule used to select APIs and stacks from the data.
3. A complete list of APIs seen within that focus.
4. A fuzzy next-step ownership assessment for every API.
5. Every matching reviewer-focused call pattern, deduplicated and grouped by
   API, with traceability back to all source stacks.
6. Selected run provenance, workload scope, and evidence/counting limitations.

Use an API assessment table with these columns:

| API | Observations | Raw stacks | Deduplicated patterns | Next-step owner file | Preferred approach | Confidence | Rationale |
|-----|-------------:|-----------:|----------------------:|----------------------|--------------------|------------|-----------|

The next-step owner is a Chromium source file, not merely a team or subsystem.
Use `External/CRT/system caller` only when no Chromium-owned caller is present,
and identify the closest Chromium integration owner when the evidence supports
one.

## Ownership heuristic

Choose the file that most plausibly owns the next investigation or change. It
need not be the innermost frame.

- Walk outward from the API caller until the stack reaches actionable Chromium
  code.
- Do not assign ownership to generic task, threading, allocator, or callback
  machinery when a more specific caller is visible.
- Prefer the component that chose the operation or data flow over a generic
  wrapper that issued the final Win32 call.
- Record confidence as High, Medium, or Low and explain ambiguous attribution.
- If no solution is apparent, assign the immediate meaningful caller's owner so
  that owner can propose one.

## Solution preference

Evaluate recommendations in this order:

1. **Migrate the caller:** prefer an equivalent API or Chromium abstraction
   already supported by Basic Process.
2. **Broker the operation:** consider a browser-to-child broker, especially for
   stable state or configuration that can be supplied once rather than through
   a hot per-call IPC path.
3. **Opt out narrowly:** consider excluding the specific caller, feature, or
   process role from Basic Process when migration or brokering is unsuitable.
4. **Escalate to the caller owner:** when no solution is obvious, name the file
   owning the immediate meaningful caller and ask that owner to determine the
   solution.

Do not present logging, forwarding, or allowlisting an API as the preferred
remediation when one of the higher-priority approaches is viable.

## Call-stack evidence

Optimize stack evidence for reviewer understanding rather than reproducing raw
debugger output. Start from every matching source stack, retain only the frames
needed to explain the call pattern and identify a plausible fix, and then
deduplicate equivalent retained patterns.

### Frame selection

Retain, in recorded innermost-first order:

- The immediate API caller and enough adjacent implementation frames to explain
  why the API is called.
- The actionable Chromium, third-party integration, CRT, or system caller that
  owns the operation.
- A meaningful feature, startup, or entry boundary when it helps explain the
  call pattern.
- Loader/resolver detail only when it is non-empty and helps explain the call
  pattern.

Normally omit:

- `ntdll`, `kernel32`, and other loader/runtime frames below a clear entry
  boundary.
- Generic task, callback, bind, allocator, thread-start, exception-unwind, and
  message-loop machinery after a specific caller is already visible.
- Repeated helper or inline frames that do not change ownership or explain the
  operation.
- Unresolved tail frames that add no attribution value.

When uncertain whether a frame affects ownership, behavior, or remediation,
retain it. It is acceptable to keep the full source stack when no defensible
trimming boundary exists.

For example, a UCRT DLL-startup pattern may retain
`__acrt_GetModuleFileNameA -> _configure_narrow_argv -> ... ->
dllmain_dispatch`, then omit the following `ntdll` loader tail.

### Deduplication

Deduplicate after frame selection, using the retained symbolic call pattern and
its ownership/remediation meaning rather than raw addresses.

- Collapse stacks whose retained function/source chain describes the same
  actionable pattern, even when RVAs or irrelevant low-level frames differ.
- Equivalent patterns may be collapsed across modules when the module
  distinction does not change ownership or remediation. List every represented
  module so the distinction is not lost.
- Feature-caller differences alone are not a reason to keep separate patterns
  when every caller reaches the same owner and the same fix covers all cases.
- Do not merge stacks when process role, loader target, operation mode, owner,
  behavior, or likely remediation differs materially.
- Aggregate observation counts across collapsed source stacks.

When an equivalent retained pattern appears in multiple Chromium modules,
choose the representative stack in this order:

1. `chrome.exe`
2. `chrome.dll`
3. `chrome_elf.dll`

If none of these modules is present, use the primary measured module recorded
in the run provenance.

Use the representative module's frames and RVAs for the displayed call stack.
List every represented module exactly once in the pattern's `**Modules:**`
metadata line. Do not emit a separate `Also seen in <...>` line because it
duplicates the module list.

### Solution-level second pass

After symbolic call-pattern deduplication, inspect the APIs with the most
remaining patterns and decide whether caller diversity actually changes the
solution.

For each high-pattern API, ask:

1. Is the actionable owner a shared backend outside the report's focus area?
2. Would one change in that owner migrate, broker, or opt out every caller?
3. Are the remaining pattern differences only feature callers, task paths, or
   startup contexts?

If so, collapse them into one solution-focused pattern. Show only enough stack
context to identify the immediate API caller, the shared actionable owner, and
one representative path from the focus area. Add a concise
`**Why one pattern is enough:**` explanation. For example, hundreds of Blink
logging callers that all reach `logging::LogMessage::Init` should become one
Base logging pattern, not hundreds of Blink patterns.

Keep separate solution patterns when the implementation work differs, such as:

- Imported-handle validation versus child-side handle creation or duplication.
- Different loader/resolver targets that reach different owners.
- Different libraries or abstractions that require independent migrations.
- Any distinction that changes the recommended owner or remediation.

If one representative stack cannot demonstrate every materially different
operation assigned to a pattern, split the pattern or include multiple short
representative sub-stacks.

The `Deduplicated patterns` assessment-table column must report the final
solution-focused pattern count after this second pass.

When an API has too many source stacks for an inline list to remain readable,
write an adjacent pattern-map CSV with columns for API, final pattern, source
stack ID, and observations. The report may then summarize the source-stack
count and link to that file instead of printing hundreds of IDs.

For each emitted pattern include:

- All source stack IDs and their individual observation counts, either inline
  or through the adjacent pattern-map CSV.
- The aggregate observation count.
- A short solution-cluster label.
- All represented modules.
- Useful, non-empty loader/resolver detail when present.
- The retained frame chain, with module + RVA and symbol/source location.
- A concise omission note when frames were trimmed, such as
  `Omitted from representative: 9 loader/runtime frames after
  dllmain_dispatch`.

### Required pattern formatting

Use the locale deep-dive layout for every API and retained pattern:

````markdown
### `AreFileApisANSI` - 82 observations, 2 raw stacks, 1 pattern

#### Pattern 1 - 82 observations from 2 raw stacks

**Source stacks:** `1` (41), `2` (41)

**Modules:** `chrome_elf.dll`, `chrome.dll`

```text
[1] chrome.dll+0x1540bc46 | __acrt_GetModuleFileNameA @ path:33:0
[2] chrome.dll+0x153d65ea | _configure_narrow_argv @ path:398:0
[5] chrome.dll+0x153c221f | dllmain_dispatch @ path:276:0
```

Omitted from representative: 15 generic/runtime frames after
`dllmain_dispatch`.
````

Formatting requirements:

- Put API and pattern totals in their headings and use correct singular/plural
  wording.
- Render source stack IDs, observation counts, modules, and optional useful
  loader/resolver detail as bold metadata lines, separated by blank lines.
- Do not repeat aggregate observations in a separate metadata line when they
  already appear in the pattern heading.
- Put retained frames in a fenced `text` block.
- Format each frame as
  `[source frame index] module+RVA | symbolized function/source`.
- Preserve the original source frame index even when omitted frames create
  gaps.
- List represented modules only in the `**Modules:**` line; never duplicate
  them in an `Also seen in <...>` line.
- Put the omission note after the frame block.
- Do not render frames as a Markdown numbered list, add a `Retained frames:`
  label, or prefix metadata and omission notes with list bullets.

Omit the loader/resolver field entirely when it is empty or uninformative.
Never emit placeholder text such as
`Loader/resolver detail: none recorded`.

The authoritative CSV remains the source for full raw stacks. The report should
make that path explicit so a reviewer can recover omitted frames if needed.

## Required output formats

Write each report in both formats:

- `<report_name>.md`: the authoritative reviewer-focused Markdown report.
- `<report_name>.html`: a self-contained HTML rendering of the same content.

The HTML report must preserve headings, tables, inline code, fenced call
stacks, emphasis, and links. Include embedded CSS for readable screen and print
layouts; do not require network resources.

When a report directory contains multiple reports, also write `index.html`
linking the HTML reports, call-tree HTML, CSV evidence, pattern maps, and folded
stack files.

Generate HTML only after the Markdown report and completeness checks are
finished. HTML generation does not replace validation of the Markdown,
authoritative CSV, or pattern map.

From the Chromium source root, use the bundled
[`render_report_html.mjs`](render_report_html.mjs) helper:

```powershell
$skillDir = "sandbox\win\tests\basic_process\skills\basic-process-deep-dive"
$report = Join-Path $run "echo_service_api_deep_dive.md"
node "$skillDir\render_report_html.mjs" $report
if ($LASTEXITCODE -ne 0) { throw "HTML rendering failed" }
```

For existing data, set `$run` to the selected output directory and `$report` to
the chosen Markdown report. Adjust `$skillDir` if the skill is installed
elsewhere. Pass multiple Markdown paths to render several reports at once.

The helper uses Chromium's bundled `marked` package, writes an HTML counterpart
next to each Markdown input, and creates `index.html` when multiple inputs are
provided.

## Completeness checks

Before finishing a report:

- Verify run selection and provenance are explicit; identify missing metadata,
  failed captures, parsing losses, and symbolization gaps.
- For fresh captures, verify both the test process exit status and the launcher
  results, and confirm the expected measured process wrote the audit records.
- Verify every selected API appears in the assessment table.
- Verify every filtered source stack ID is assigned to exactly one emitted
  final solution pattern, including through a pattern-map CSV when used.
- Verify each pattern's aggregate observations equal the sum of its source
  stacks, and report-wide observations equal the filtered source total.
- Verify every report pattern has matching map rows and every map row resolves
  to an authoritative source stack when a pattern-map CSV is used.
- Verify no ownership- or remediation-relevant distinction was lost during
  trimming or deduplication.
- State the API, observation, raw-stack, and deduplicated-pattern totals.
- Clearly distinguish evidence from fuzzy ownership judgments.

Save Markdown, HTML, and pattern maps in the selected run's artifact directory
unless the user specifies another location. For existing data, default to the
directory containing the authoritative CSV. Keep comparison inputs separately
labeled and do not overwrite earlier reports without permission.
