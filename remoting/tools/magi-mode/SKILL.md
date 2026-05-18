# MAGI Protocol (Modular Automated Guided Iteration)

This skill implements the "Lean MAGI" protocol, a high-efficiency multi-agent
framework designed to resolve complex, high-stakes, or ambiguous software
engineering problems. It utilizes a **"Verification Loop"** of specialized
technical modules to enforce invariants, detect conflicts, and synthesize code
without human management overhead.

## The Two-Path Model

The Orchestrator MUST select an execution path based on the task's complexity
and ambiguity:

1. **FAST_PATH (Efficiency):** Used for low-complexity, low-ambiguity tasks
   (e.g., surgical bug fixes). *Workflow:* Scoping (Spec) -> Synthesis (Code) ->
   Single Auditor (Architect).
2. **RIGOR_PATH (Correctness):** Used for high-complexity, high-ambiguity, or
   security-sensitive tasks. *Workflow:* Scoping (Spec + Scaffold) -> Synthesis
   (Code) -> Multiple Tier 1 Scanners (Security, Perf, Architect).

## The Core Modules (Execution)

The Orchestrator MUST dynamically select specialized modules best suited for the
specific task.

- **Scoping:** Investigates the initial request, searches the codebase, and
  writes the `project.magi.json` specification.
- **Synthesis:** Writes the actual C++ code by combining technical requirements
  and adhering to constraints.
- **The Scanners (Auditors):** Specialized technical mandates (Security,
  Performance, Architect, etc.) that perform rigorous, boolean-checklist-based
  audits of the generated code.

## The Auxiliary Modules

To maintain focus and avoid context dilution, specialized tasks are delegated:

1. **Consolidation:** (Rigor Path Only) Condenses raw feedback from multiple
   Scanners into a strict list of actionable constraints in
   `constraints.magi.[iteration].json`.

2. **Training:** Captures knowledge or systemic gaps discovered during the
   process and upgrades the Scanner rulesets.

3. **Release:** A terminal module invoked with a clean context to handle
   workspace hygiene, formatting, and final staging/upload of CLs.

**TONE MANDATE (SIGNAL-TO-NOISE):** To eliminate conversational noise, conserve
tokens, and maximize parsing stability, the Orchestrator MUST instruct ALL
sub-agents (except itself) to adopt a neutral, data-driven tone.

- **Zero Preamble/Postamble:** Sub-agents MUST NOT use conversational filler,
  greetings, or explanations of their work.
- **Artifacts Only:** If an agent's mandate is to generate JSON or C++ code, its
  entire output MUST consist *only* of that raw data structure.
- **Scanners:** Scanners MUST act as rigorous, objective auditors focusing
  strictly on technical facts and data.

**TOOL AGNOSTIC MANDATE:** The protocol instructions MUST remain tool-agnostic.
Do not assume specific tool names (e.g., `update_topic`, `read_file`,
`write_file`). Use generic terms like "read from disk," "save to disk," or
"report status."

**ENVIRONMENT GROUNDING MANDATE:** All sub-agents MUST read
`project.magi.json#environment` immediately upon invocation to ground themselves
in the active VCS (`JJ` or `GIT`) and Harness (`JETSKI` or `GENERIC_CLI`). They
MUST adjust their tool usage and command construction natively to match this
environment. They MUST also ensure that any interim files generated during
execution (e.g., drafts, reviews, logs) are placed in the dedicated subfolder
`remoting/tools/magi-mode/.temp/` to minimize permission prompts and maintain
workspace hygiene.

**THE CHECKLIST LIFECYCLE STATE MACHINE:** The session's verification integrity
is governed by a deterministic boolean checklist state machine. The automated
checklist state itself only transitions or is modified during three key steps
across the stages:

- **Activation (Stage 2, Step 3):** The Orchestrator reads all selected rulesets
  in `personas/**/*.json`, takes the **Union Set** of all keys, and initializes
  the active `checklist` in `state_block.magi.json` with all values set to
  `false`.
- **Assertion (Stage 3, Step 1 & 2):** Scanners toggle their domain-specific
  keys in their `ReviewFeedback` checklist. The Orchestrator (or Consolidation
  in RIGOR_PATH) performs a **Logical AND** consolidation across all reviews. A
  key in the consolidated `state_block.magi.json#checklist` only becomes `true`
  if **ALL** scanners evaluating that key asserted `true`. Any `false` keys or
  `unlisted_issues_found` are translated into strict constraints in
  `constraints.magi.[iteration].json`.
- **Upgrades (Stage 3, Step 3):** Once consensus is reached (all checklist items
  are `true`), the Training agent uses `unlisted_issues_found` history to append
  new keys to the appropriate `personas/**/*.json` checklists.

**STAGE SIGNALING:** The Orchestrator MUST use an appropriate status-reporting
mechanism prior to invoking any sub-agents to clearly identify the current stage
of the MAGI protocol to the user (e.g., "MAGI Stage 2: Generate").

**DECENTRALIZED HANDOFFS:** To reduce Orchestrator overhead, agents SHOULD
include a `next_stage` field in their JSON output to signal the intended
successor.

- **Scoping:** `SCAFFOLDING` (or `PREPARATION` if `task_type` is `REVIEW`)
- **Architect / Test Expert:** `PREPARATION`
- **Scanners:** `ANALYSIS`
- **Consolidation:** `SYNTHESIS` (if iteration needed) or `TRAINING`
- **Synthesis:** `TEST_FILLING` (if implementation) or `ANALYSIS` (if review)
- **Training:** `VALIDATION`
- **Validation:** `DEPLOYMENT`

## Workflow

### Stage 1: Specify & Investigate

#### Step 1: Define Goal (Scoping)

1. **The Investigation:** When a bug or feature is requested, the Orchestrator
   MUST NOT read the raw logs or attempt to hold the requirements in its own
   context window. Instead, invoke a "Scoping" sub-agent.
2. **Session Resumption:** Before starting investigation, Scoping MUST check if
   the `remoting/tools/magi-mode/.temp/` directory contains a half-finished
   session (e.g., existing state files). If it does, the agent MUST ask the user
   whether they want to resume the previous session or start a new task.

#### Step 2: Investigate Codebase (Scoping)

1. Scoping investigates the codebase (`grep_search`, `read_file`) to understand
   context, dependencies, and existing patterns.
2. **Environment Discovery:** Before writing the file, Scoping MUST discover the
   environment:
   - **VCS:** Check for a `.jj/` directory or run `jj status`. If successful,
     set `vcs` to `"JJ"`. Otherwise, default to `"GIT"`.
   - **Harness:** Check if Jetski tools (e.g., `code_search`, `view_file`) are
     available. If yes, set `harness` to `"JETSKI"`. Otherwise, set to
     `"GENERIC_CLI"`.

#### Step 3: Define Scope (Scoping)

1. Scoping writes a strict specification to `project.magi.json` conforming to
   `magi_schema.json`.
2. **Path & Complexity Determination:** Scoping MUST determine the
   `execution_path` and `complexity_level`:
   - `complexity_level`: `LOW` (minor bug fixes, small nits), `MEDIUM` (standard
     feature work), or `HIGH` (architectural changes, security-sensitive code).
   - `execution_path`: `FAST_PATH` if complexity is `LOW` and ambiguity is
     `LOW`. Otherwise, default to `RIGOR_PATH`.
3. **Task Type Determination:** Scoping MUST determine the `task_type` based on
   the request:
   - `IMPLEMENTATION`: Default. For creating new features or fixing bugs. Sets
     `next_stage` to `SCAFFOLDING`.
   - `REVIEW`: For reviewing existing changes or a CL. Sets `next_stage` to
     `PREPARATION`.
   - `AUDIT`: For analyzing existing code for modernization or flaws. Sets
     `next_stage` to `PREPARATION`.
4. **Context Resolution (The Stop-and-Verify Gate):** Scoping MUST verify that
   all external context (e.g., Buganizer links, documentation URLs) has been
   successfully retrieved and parsed. If any link returned a login prompt,
   redirect, or error, Scoping MUST halt, report the failure to the human, and
   request the raw text of the missing context. It MUST NOT proceed with a
   hallucinated or "fallback" scope.
5. **Approach Confirmation (The Dynamic Gate):** Scoping MUST assess the
   ambiguity of the request.
   - **Low Ambiguity:** If the human provided prescriptive instructions (e.g.,
     "Add feature X to file Y"), Scoping sets `ambiguity_level: "LOW"`.
   - **High Ambiguity:** If the request is exploratory, implies multiple viable
     paths, or is underspecified, Scoping sets `ambiguity_level: "HIGH"`. **The
     Gate:** If `ambiguity_level == "HIGH"` OR `context_resolved == false`, the
     Orchestrator MUST pause for human intervention. If
     `ambiguity_level == "LOW"` AND `context_resolved == true`, the Orchestrator
     MAY auto-proceed to the next stage.
6. **JSON Contract (`project.magi.json`):** See [EXAMPLES.md](./EXAMPLES.md) for
   a full example. *Tooling Selection:* The combination of `repo_type`, `vcs`,
   and `harness` in the `environment` block determines the exact build, test,
   and upload commands used by the agents.

### Stage 2: Generate

*This stage is ONLY executed if `task_type` is `IMPLEMENTATION`.*

#### Step 1: Scaffold (The Architect)

1. **Roughing In:** Invoke an Architect sub-agent. The Architect MUST read
   `project.magi.json` to understand the goal. Their mandate is to create
   necessary files, define class interfaces, set up Mojo pipes, and GN/DEPS
   rules. Leave implementation details empty or stubbed (e.g.,
   `NOTIMPLEMENTED()`). The Architect MUST signal `next_stage: SCAFFOLDING`.

#### Step 2: TDD Boundary (The Test Expert)

1. **Test-Driven Development:** After the Architect completes the scaffold,
   invoke a Test Expert sub-agent to establish the testing boundaries. Their
   mandate is to add test files (`*_unittest.cc`), define the required test
   fixtures, and stub out the critical test cases based on the Architect's
   scaffold. To ensure failure in Chromium's GTest framework (confirming TDD
   behavior), the Test Expert MUST insert `ADD_FAILURE() << "NOT IMPLEMENTED"`
   into the stubbed test cases. The Test Expert SHOULD signal
   `next_stage: PREPARATION`.
2. **Scaffold Verification:** Before proceeding to Step 3, the Orchestrator MUST
   attempt to build the scaffolded targets. If `build_targets` are defined in
   `project.magi.json`, the Orchestrator MUST verify that the scaffold compiles
   and that all newly added tests fail (confirming TDD behavior).
3. **Snapshot:** The Orchestrator records this state (e.g., as a local commit)
   as the "Base Scaffold" so all parallel Implementation Modules share the exact
   same multi-file API and test boundaries.

#### Step 3: Select Modules (The Orchestrator)

1. **Needs Assessment:** The Orchestrator reads `project.magi.json` and
   `src/remoting/tools/magi-mode/ROUTING.md` (the routing catalog) to select the
   appropriate Scanners (Auditors) based on the execution path:
   - **FAST_PATH:** Select a single auditor (typically the Architect).
   - **RIGOR_PATH:** Select the "Big Three" (Security, Performance, Architect)
     plus any relevant domain modules.
2. **State Initialization:** The Orchestrator writes the initial State Block to
   `state_block.magi.json`. The `checklist` field is initialized with the
   **Union Set** of all checklist keys from every selected ruleset, set to
   `false`.
3. **State Transport Selection:** The Orchestrator selects `state_transport`
   based on the risk score (Scanner Count * Target Files):
   - **FILE_IO:** Use if risk score > 15.
   - **EPHEMERAL_WITH_LOGS:** Default for standard tasks.
4. **JSON Contract (`state_block.magi.json`):** See [EXAMPLES.md](./EXAMPLES.md)
   for a full example.

#### Step 4: Implement (Implementation Modules)

1. **Parallel Implementation:** Invoke the selected sub-agents in parallel
   (`wait_for_previous: false`). Instruct each to implement the stubbed
   internals from the Base Scaffold.
2. **Mandates for Modules:**
   - **Production Code Focus:** Modules SHOULD focus primarily on implementing
     the production code logic.
   - **Production Hardening:** Modules MUST adhere to the **Production Hardening
     Checklist** (defined at the end of this document) during implementation.
   - **Domain Edge Cases:** If a module identifies specific edge cases or
     scenarios that need verification, they MUST add a stubbed test case in the
     test file (with both `ADD_FAILURE() << "NOT IMPLEMENTED"` and a descriptive
     TODO comment) rather than fully implementing the test.
   - **Test Hooks & Accessors:** Modules MUST provide any necessary public
     accessors, test-only hooks, or `friend` declarations in the production code
     that the Test Expert will need to verify internal state.
3. **File I/O:** Each sub-agent MUST read `project.magi.json` to ground their
   implementation in the actual requirements. They MUST securely save their
   draft to disk using the versioned naming convention
   `[filename].[persona].magi.[iteration]` (e.g., `host.cc.security.magi.1`).
   Sub-agents SHOULD signal `next_stage: SYNTHESIS` upon completion. *Note:
   Sub-agents are permitted to change scaffolded signatures if their priority
   requires it.*

#### Step 5: Synthesize (Synthesis)

1. **Conflict Resolution:** Synthesis MUST use a surgical 3-way merge strategy
   (Base Scaffold + Draft A + Draft B) rather than full-file overwrites to
   resolve conflicts between modules.
2. **Hardening Audit:** Synthesis MUST perform a final audit against the
   **Production Hardening Checklist** during synthesis to ensure merged code
   maintains architectural integrity.
3. **Synthesis Build (Empirical Gate):** If `build_targets` are defined in
   `project.magi.json`, Synthesis MUST run the local build/test suite on "Draft
   A".
   - **Failure:** If the code fails to compile, Synthesis MUST loop back to
     internal refinement and fix the syntax/link errors. It MUST NOT signal
     `next_stage: TEST_FILLING` or `ANALYSIS` until the build is green.
   - **Success:** Once the build is verified, Synthesis MUST attach the build
     logs to the synthesis report before signaling `next_stage: TEST_FILLING`.

#### Step 6: Implement Tests (The Test Expert)

1. Fill out the actual implementation of tests.

#### Step 7: Verification Build (The Test Expert)

1. Run tests to verify they PASS. If they fail due to implementation bugs, loop
   back to Step 5 or 4.

### Stage 3: Refine

#### Step 1: Review (The Scanners)

1. **Audit Mandate:** Invoke the selected Scanners (Auditors) to review the
   synthesized code against their specialized boolean checklists.
2. **Prompt Template:**
   > MANDATE: Perform technical audit of synthesized code. INPUT: [filename]
   > SPEC: project.magi.json RULESET: [persona_file_path] OUTPUT: JSON object
   > conforming to magi_schema.json#definitions/ReviewFeedback TARGET:
   > review.[persona].magi.[iteration].json NEXT_STAGE: ANALYSIS TONE: Zero
   > Preamble. Artifacts only.

#### Step 2: Consolidate (The Orchestrator / Consolidation)

1. **Path A: FAST_PATH:** The Orchestrator reads the single review and updates
   `state_block.magi.json` directly. If any checklist items are `false`, it
   generates `constraints.magi.[iteration].json` and loops back to synthesis.
2. **Path B: RIGOR_PATH:** The Orchestrator invokes the **Consolidation**
   sub-agent to consolidate multiple scanner reports. Consolidation performs a
   **Logical AND** across all checklists and generates a prioritized list of
   Actionable Constraints in `constraints.magi.[iteration].json`.
3. **Conflict Detection (Oscillation):** Consolidation MUST proactively detect
   mutually exclusive requirements.
   - **Oscillation:** If a checklist key toggles state (`True -> False -> True`)
     across iterations, or if the `active_constraints` list is identical across
     two iterations, Consolidation MUST signal `next_stage: ESCALATION`.
   - **Conflict Report:** In the event of an oscillation, Consolidation MUST
     produce a structured `conflict_report` in the State Block, identifying the
     specific modules and constraints that are in conflict.
4. **Common Convergence:**
   - **Convergence & Iteration:** Synthesis reads `state_block.magi.json` and
     `constraints.magi.[iteration].json` to generate the next iteration.
   - **Escalation Gate:** If `oscillation_detected == true`, the Orchestrator
     MUST halt and present the `conflict_report` to the human for a strategic
     decision.

#### Step 3: Train (Training)

1. **Continuous Improvement:** Once consensus is reached, the Orchestrator MUST
   invoke a "Training" sub-agent. Training evaluates the final State Block and
   Consolidation constraints to identify systemic gaps in the Scanners'
   knowledge. If a Scanner made a recurring mistake or lacked domain context,
   Training proposes an upgrade to the relevant `personas/*.json` ruleset by
   adding a new Boolean constraint to its checklist.
2. **Module Segmentation (Hierarchical Specialization):** Training MUST NOT let
   a ruleset's checklist exceed 10 items. If adding a new constraint exceeds
   this limit, Training MUST "segment" the module using a nested directory
   structure representing `[category]/[domain]/[specialty].json` (e.g., split
   `core/security.json` into `core/security/memory.json` and
   `core/security/network.json`). Do not use flat files with underscores. The
   directory depth MUST NOT exceed 5 levels (counting from `/personas`). Migrate
   the relevant checks and update `ROUTING.md`. Training MUST signal
   `next_stage: VALIDATION`.

### Stage 4: Release

#### Step 1: Validate (Release)

1. Run `git cl presubmit` and full test suite.
2. **Failure Loop:** If validation fails and requires code change, loop back to
   **Stage 3: Step 1 (Review)**! (Trivial fixes like formatting/lint can be
   handled by Release directly if they pass presubmit).

#### Step 2: Deploy (Release)

1. **Handoff:** Once Validation passes, the Orchestrator pauses its own actions
   and delegates strictly to the **Release** sub-agent. The Orchestrator passes
   only two pieces of information: the name of the feature/bug, and the list of
   MAGI files updated by Training/Recruiter.
2. **Exclusive Mandate:** Release's exclusive mandate is:
   - **Workspace Hygiene:** Read the discovered VCS from
     `project.magi.json#environment/vcs`. Run `jj st` (for JJ) or `git status`
     (for Git). Detect and revert accidental submodule bumps. Remove any
     lingering temporary files generated by the protocol (e.g., `*.magi`,
     `*.magi.*`) and delete the `remoting/tools/magi-mode/.temp/` directory.
   - **Formatting:** Enforce `git cl format` or project-specific formatters.
   - **The Feature CL:** Upload the main feature CL containing only the product
     source changes (using the VCS-specific track defined in the VCS Isolation
     Rule).
   - **The MAGI CL:** Create a separate change/bookmark (for JJ) or branch (for
     Git). Stage and upload the `PERSONAS.md` and `personas/**/*.json` files
     updated by the Recruiter or Training as a secondary CL.

### Specialized Modes

- **Manual Intervention:** If `ambiguity_level == "HIGH"` or
  `context_resolved == false`, the Orchestrator MUST pause for human
  verification before proceeding to execution.
- **Human-in-the-Loop Audit:** For critical security changes, the Orchestrator
  MAY present the final synthesized code and consolidated checklist to the human
  for a final "PASS/FAIL" before deployment.

### Production Hardening Checklist

Synthesis MUST ensure:

1. **Lifetime Safety:** Use `base::RefCountedDeleteOnSequence` for timers.
2. **Zero-Copy:** Prefer `std::move` and `base::RefCountedString`.
3. **DoS Mitigation:** Enforce strict length limits (e.g., 64KB).
4. **Atomic State:** Ensure callback checks (e.g., `if (callback_)`) are
   atomically sound or strictly sequence-enforced to prevent double-runs.

**VCS Isolation Rule:** Any modifications to MAGI files (e.g., adding/updating
personas by the Recruiter or Training) MUST be excluded from the feature/bugfix
CL. The staging and submission workflow branches dynamically based on
`project.magi.json#environment/vcs`:

- **For JJ (Jujutsu):** Work in parallel sibling changes (both rooted at
  `main@origin`) from the start: one for the feature/bugfix and one for the MAGI
  upgrades. If they accidentally get mixed, Release MUST use `jj split` or
  `jj squash -i` to cleanly separate the changes before pushing.
- **For GIT:** Use standard git branching. Stage *only* product source files for
  the feature CL. Stage *only* MAGI updates for the secondary CL.

#### Workspace Management

- **Interim File Isolation**: To minimize permission prompts for the user and
  maintain workspace hygiene, all interim files generated by the protocol (e.g.,
  drafts, reviews, logs) MUST be placed in a dedicated subfolder:
  `remoting/tools/magi-mode/.temp/`.
- **Cleanup**: Release (or the agent in charge of cleanup) MUST delete the
  `.temp/` directory at the end of the run, rather than requiring the user to
  add it to `.gitignore`.

### Infrastructure & Tooling Guidance

To ensure agents operate safely within the specific environment, specialized
tooling personas are available in `personas/infra/`:

- **`infra/jj_git.json`**: Expert in `jj` on Git workflow. Agents performing
  file operations or commit management in a `JJ` environment SHOULD consult this
  persona to avoid losing Gerrit `Change-Id`s or mishandling detached HEAD
  states.
- **`infra/chromium_build.json`**: Expert in Chromium build tools. Agents
  performing builds or adding new files SHOULD consult this persona to ensure
  correct target discovery and usage of `autoninja`.

## Harness Optimizations (Jetski Mode)

If `project.magi.json#environment/harness == "JETSKI"`, the Orchestrator:

1. **Direct Prompt Injection:** SHOULD read the `personas/**/*.json` files and
   inject their `mandate` and `checklist` directly into the `Prompt` or `Role`
   arguments of `invoke_subagent` tool calls. *Joining Rule:* If a mandate or
   checklist item is an array of strings, the Orchestrator MUST join them using
   direct concatenation (`"".join(array)`). To prevent token merging, each
   element in the array (except the last) MUST end with a trailing space or
   punctuation.

2. **Orchestrator Routing:** MUST act as the active routing environment by
   parsing the `next_stage` token from sub-agent output JSONs and manually
   calling the next tool (standard Jetski does not have an automatic background
   router). *Note: This exception effectively centralizes coordination in the
   Orchestrator for this environment, diverging from the default Decentralized
   Handoffs rule. This is an optimization for the Jetski environment and not a
   strict rule for all environments, to keep the protocol description general as
   mandated by the "TOOL AGNOSTIC MANDATE".*

## When to Invoke

- When an automated review finds a flaw that is hard to resolve without
  trade-offs.
- When a bug occurs only on a specific platform and the fix might impact others.
- When adding a new feature that has significant performance or security
  implications.

## Testing Protocol

To validate the MAGI protocol execution and prevent regressions in prompt
instructions and state machine transitions, consult the
[SKILL_TEST_PLAN.md](./SKILL_TEST_PLAN.md) for the overall strategy and
[SKILL_TEST.md](./SKILL_TEST.md) for the specific unit tests.
