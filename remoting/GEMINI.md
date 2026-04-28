# Chrome Remote Desktop Instructions

Instructions that are relevant when working with the Chrome Remote Desktop (aka.
CRD and chromoting) host binaries, or code located in //remoting.

While code for the Chrome Remote Desktop host lives in the Chromium repo (this
repo), it is mostly an independent product, separate from Chrome, except for
ChromeOS, where the IT2ME native messaging host is part of the Chrome binary.

## Relevant Documentation

When working on specific domains, **you MUST use the `read_file` tool to load
these documents:**
*   **Testing:** See `docs/testing.md` if you are writing, modifying, or
    debugging tests.

*   **Architecture & IPC:** See `docs/architecture.md` if you are modifying host
    architecture, IPC, or adding/removing processes.

*   **Native Client:** See `client/GEMINI.md` if you are working on the native
    client code (ChromeOS/Boca).

*   **D-Bus Interface Generation:** See `host/linux/dbus_interfaces/GEMINI.md` if
    you need to generate missing D-Bus headers for Linux.

*   **Linux Host Implementation Details:** See `host/linux/GEMINI.md` for
    information about Linux-specific libraries like GVariant and D-Bus
    wrappers.

## Security Context

CRD host processes often run with high privileges (e.g., `SYSTEM` on Windows,
`root` on Linux) while receiving input from the network. You **MUST** be
extremely careful with memory safety, bounds checking, and input sanitization.
Correctness is critical to prevent security vulnerabilities.

## Platform Context

Code in `//remoting` is built for many platforms. The file suffix often
indicates conditional compilation:
* `_chromeos.cc`: ChromeOS specific.
* `_mac.cc` / `_mac.mm`: MacOS specific
* `_posix.cc`: Linux and MacOS.
* `_win.cc`: Windows specific.
* `_wayland.cc`: Linux/Wayland specific.
* `_x11.cc`: Linux/X11 specific.

*   **Cross-Platform Testing Limits:** Always be aware of the host OS you are
    currently running on (e.g., Linux). Do **NOT** attempt to build or run tests
    for a different platform locally (e.g., do not try to build Mac-only code or
    run ChromeOS-specific tests on a Linux workstation).
*   **Try-Bots (`git cl try`):** If you modify code for a platform that cannot
    be tested locally, stop and inform the user. Suggest using `git cl try` to
    verify the changes on the appropriate CQ bots. Common try-bots include:
    `linux-rel`, `mac-rel`, `win-rel`, and `chromeos-rel`. Example command: `git
    cl try -B luci.chromium.try -b mac-rel`. After triggering a try-bot, pause
    and wait for the human to provide the failure logs. Do not attempt to poll
    `git cl status` or download massive raw logs via `bb` unless explicitly
    asked.

## Coding Standards

*   **Finch & UMA:** Chrome Remote Desktop does **NOT** use Finch
    (`base::FeatureList`) or UMA (`base::UmaHistogram*`) for non-ChromeOS
    platforms. Do not introduce these into the codebase unless you are
    specifically modifying ChromeOS-only code.
*   **Headers:** Always check headers to add missing ones and remove obsolete
    headers.
*   **Containers:** Consult `base/containers/README.md` when choosing a
    container (list, map, etc.).
*   **std::optional:** Prefer `has_value()` for boolean testing and `*` for
    dereferencing.
*   **Logical Chunks:** Break work into logical chunks. Perform a build and test
    cycle after each chunk to prevent compounding failures.

## Code Review and CL Structure

Our team strongly prefers small, easily reviewable CLs (Changelists).
*   **Separation of Concerns:** Do not mix refactoring or reformatting with new
    feature development in the same CL.
*   **Pre-work CLs:** If a task requires refactoring/reformatting existing code
    to support a new feature, do this *first* in a dedicated "pre-work" CL. Give
    it a clear CL description explaining *why* the refactoring is being done
    (e.g., "Prep work for feature X").
*   **Proactive Communication:** If you (the agent) realize a request will
    involve a large diff, **pause and ask the human upfront**. Ask if they want
    one large CL or if they prefer you to manage a chain of smaller, focused CLs
    to make the work easier to review later.

## Automated Sub-Agent Review

Before finalizing any task or uploading a CL in `//remoting`, you **MUST** trigger
an automated self-review to catch common CRD-specific errors.
*   **Pre-checks:** Before invoking the generalist sub-agent, you MUST run the following
    commands and gather their output to include in the prompt below:
    1.  `git cl presubmit -u --force` (Linting and presubmit checks).
    2.  `gn check {OUT_DIR} //remoting/*` (Headers and dependencies).
*   **Action:** Invoke the `generalist` sub-agent using the `invoke_agent` tool.
*   **Prompt:** Use the following exact prompt template:
    > "You are a senior Chrome Remote Desktop reviewer. Review the following
    > changes [include diff or describe changes]. I have also run the presubmit
    > and gn check commands. Here is their output: [include output].
    > Your MOST IMPORTANT task is to identify any unintended changes, logical
    > mistakes, or typos. After verifying the core correctness, specifically check for:
    > 1. Memory safety in high-privilege processes, particularly preventing Use-After-Free
    >    (UAF) errors.
    > 2. Platform-specific logic bugs or missing `#if BUILDFLAG(...)` guards.
    > 3. Adherence to the 'Small CLs' mandate. Suggest a reasonable split point if the CL is
    >    large and a split point is apparent.
    > 4. Consider all directives in remoting/GEMINI.md and ensure the CL adheres to them.
    >    [include contents of remoting/GEMINI.md]
    >
    > IMPORTANT: You are the reviewing sub-agent. Do NOT trigger any further sub-agent reviews."
*   **Feedback Loop:** If the sub-agent identifies issues, you must address them
    and re-run the relevant validation steps before considering the task complete.

## Workflow Efficiency (Optimizing for Wall-Clock Time)

When executing tasks, optimize for **total human wall-clock time** rather than
just the fastest individual command execution. Every shell command requires
human approval via the CLI, which causes expensive context switches for the
developer.

*   **MAGI Protocol:** For highly complex, high-stakes, or ambiguous architectural
    problems, suggest invoking the `magi-mode` skill. This triggers a
    multi-agent debate to explore security, performance, and maintainability
    trade-offs before implementation. It features an iterative "Rumination Cycle"
    with dynamic topology routing (Star to Roundtable) and human escalation for
    deadlocks.
*   **Batch Commands:** Combine related shell commands using `&&` whenever
    logical. For example, instead of running a build and then asking to run a
    test, combine them:
    `autoninja -C {OUT_DIR} {TARGET} && tools/autotest.py...`.
*   **Parallel Execution:** Use the `wait_for_previous: false` parameter in tool
    calls to perform independent reads or searches in parallel within a single
    turn.
*   **Minimize Interruptions:** Gather all necessary information (e.g., listing
    directories and reading related headers) in a single turn. Avoid asking the
    human for permission at every micro-step; only pause for intervention when a
    step is destructive, risky, or requires a strategic decision.

IMPORTANT: When making suggestions for CRD code changes, make sure they are
actually relevant to CRD. For example, code changes outside of //remoting
usually aren't relevant.

## Code Health & Modernization

`//remoting` contains legacy code (10+ years old) that may not align with modern
Chromium standards or may duplicate functionality now available in `//base`.

*   **Proactive Observation:** If you notice code that is significantly out of
    date, or a `/remoting/base` utility that could be replaced by a standard
    `//base` equivalent, **flag it to the human in the chat**.
*   **TODOs:** Do NOT add `TODO` comments for code health improvements unless
    explicitly asked by the human.
*   **Judgment:** Prioritize the task at hand. Only suggest modernization if it
    directly relates to the area you are already modifying or if it
    significantly improves the safety/readability of your new changes.

## Knowledge Preservation & Documentation

To maintain context efficiency, **this root `GEMINI.md` MUST remain lean.** It
should only contain critical safety rules, high-level workflow mandates, and
routing pointers.

If you discover a complex architectural pattern, a non-obvious dependency, or a
critical threading constraint (especially one that differs from standard
Chromium conventions):
1. **Verify your discovery:** Ensure your understanding is correct by reading
   the relevant source code and headers.
2. **Broad/Global Knowledge:** Do **NOT** add deep technical references,
   architectural explanations, or domain-specific guides directly to this root
   file. Instead, create or update a specific Markdown file in the `docs/`
   directory (e.g., `docs/audio.md`, `docs/style.md`).
   * When creating a new doc, add a single bullet point to the
     `## Relevant Documentation` section at the top of this file, instructing
     future agents on *when* to load it.
3. **Local Knowledge:** If the discovery applies only to a specific
   sub-directory of `//remoting`, create a new `GEMINI.md` file in that
   sub-directory or update the existing one. Focus on "Why" and "Architecture,"
   not "What."
4. **Be concise:** All system instructions and documentation should be
   high-signal and low-noise to save tokens for future agents.

# Validation & Pre-Commit

Whenever you make any code or build changes in `//remoting`, you **MUST** run
the following:

1.  **Code Formatting:** Run `git cl format` to ensure all changes follow
    Chromium style.
2.  **Dependency Check:** Run `gn check {OUT_DIR} //remoting/*` to ensure no
    circular or missing dependencies were introduced.
3.  **Unit Tests:** Run `remoting_unittests` to verify your changes. You can use
    `tools/autotest.py --run-all -C {OUT_DIR} remoting/` to run all tests in the
    remoting directory.
4.  **Static Analysis:** Run `git cl presubmit -u --force` after you have
    finished all other validation steps.

### Common Build Targets

*   **Debug binaries** are preferred by default.
*   `remoting_all`: Build everything related to remoting.
*   `remoting_unittests`: Run remoting unit tests.
*   `remoting_dev_me2me_host`: Build a set of targets for running the host
    locally.
*   `remoting_me2me_host`: The Me2Me host binary.
*   `remoting_native_messaging_host`: Native Messaging host for Me2Me.
*   `remote_assistance_host`: Native Messaging host for It2Me.