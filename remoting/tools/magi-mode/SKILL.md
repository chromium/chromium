# MAGI Protocol (Multi-Agent Guided Iteration)

This skill implements the MAGI multi-agent debate protocol to resolve complex,
high-stakes, or ambiguous problems. It utilizes a "Consensus Loop" of
specialized sub-agents to explore dimensions, moderate feedback, and synthesize
code without overwhelming the main Orchestrator.

For more information about the protocol, see the [README.md](./README.md).

## The Core Personas (Dynamic Selection)

The Orchestrator MUST dynamically select 3+ expert personas best suited for the
specific task. While **Security**, **Performance**, and **Architect** are the
defaults for production code, the Orchestrator may swap them for others (e.g.,
**Test Expert**, **Platform Expert**) if the task context warrants it.

Each persona prompt MUST be anchored with the relevant **Platform** and
**Language** (e.g., "Windows C++ Security Expert").

## The Auxiliary Personas (The Consensus Loop)

To prevent context bloat and semantic drift, the Orchestrator MUST NOT write
code or summarize state itself (except for initializing the State Block). It
delegates to several auxiliary personas:

1. **The Scoping Lead:** Investigates the initial bug/feature request, searches
   the codebase, and writes a strict `project.magi.json` specification document
   conforming to `magi_schema.json`.
2. **The Synthesizing Architect:** Writes the actual C++ code by combining
   initial drafts and adhering to constraints provided by the Supervisor or
   Technical Program Manager.
3. **The Review Analyst:** (Consensus Mode Only) Condenses raw feedback from
   multiple reviewers into a strict list of actionable constraints in
   `constraints.magi.[iteration].json` conforming to `magi_schema.json`.
4. **The Technical Program Manager:** (Consensus Mode Only) Maintains the State
   Block across rounds, explicitly checking for "flip-flopping" or stalled
   progress, and provides the final constraints to the Synthesizing Architect.
5. **The Supervisor:** (Supervisor Mode Only) Acts as a hub to read all reviews,
   update the State Block, and generate the final constraints for the
   Synthesizing Architect in a single turn.
6. **The Trainer:** Captures knowledge or systemic gaps discovered during the
   Consensus Loop and upgrades the expert Persona definitions.
7. **The Release Engineer:** A terminal agent invoked with a clean context to
   handle workspace hygiene, formatting, resolving lint and presubmit errors,
   verifying the files in the CL are expected, and the final staging/upload of
   CLs.

**TONE MANDATE (SIGNAL-TO-NOISE):** To eliminate conversational noise, conserve
tokens, and maximize parsing stability, the Orchestrator MUST instruct ALL
sub-agents (except itself) to adopt a neutral, data-driven tone.

- **Zero Preamble/Postamble:** Sub-agents MUST NOT use conversational filler,
  greetings, or explanations of their work.
- **Artifacts Only:** If an agent's mandate is to generate JSON or C++ code, its
  entire output MUST consist *only* of that raw data structure.
- **Reviewers:** Reviewers MUST act as rigorous, objective auditors focusing
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

- **Activation (Stage 2, Step 3):** The Orchestrator reads all selected
  `personas/**/*.json` checklists, takes the **Union Set** of all keys, and
  initializes the active `checklist` in `state_block.magi.json` with all values
  set to `false`.
- **Assertion (Stage 3, Step 1 & 2):** Reviewers toggle their domain-specific
  keys in their `ReviewFeedback` checklist. The Supervisor/Review Analyst
  performs a **Logical AND** consolidation across all reviews. A key in the
  consolidated `state_block.magi.json#checklist` only becomes `true` if **ALL**
  reviewers evaluating that key asserted `true`. Any `false` keys or
  `unlisted_issues_found` are translated into strict constraints in
  `constraints.magi.[iteration].json`.
- **Upgrades (Stage 3, Step 4):** Once consensus is reached (all checklist items
  are `true`), the Trainer uses `unlisted_issues_found` history to append new
  keys to the appropriate `personas/**/*.json` checklists.

**STAGE SIGNALING:** The Orchestrator MUST use an appropriate status-reporting
mechanism prior to invoking any sub-agents to clearly identify the current stage
of the MAGI protocol to the user (e.g., "MAGI Stage 2: Generate").

**DECENTRALIZED HANDOFFS:** To reduce Orchestrator overhead, agents SHOULD
include a `next_stage` field in their JSON output to signal the intended
successor. The environment will use this token to automatically route to the
next expert.

- **Scoping Lead:** `SCAFFOLDING`
- **Architect / Test Expert:** `PREPARATION`
- **Engineering Manager:** `IMPLEMENTATION` (or `CRITIQUE` if `task_type` is
  `REVIEW` or `AUDIT`)
- **Domain Experts:** `SYNTHESIS`
- **Synthesizing Architect:** `TEST_FILLING`
- **Test Expert:** `CRITIQUE`
- **Reviewers:** `ANALYSIS`
- **Review Analyst:** `TPM_UPDATE`
- **Technical Program Manager:** `SYNTHESIS` (if iteration needed) or `TRAINING`
- **Supervisor:** `SYNTHESIS` (if iteration needed) or `TRAINING`
- **Trainer:** `VALIDATION`
- **Validation:** `DEPLOYMENT`

## Workflow

### Stage 1: Specify & Investigate

#### Step 1: Define Goal (The Scoping Lead)

1. **The Investigation:** When a bug or feature is requested, the Orchestrator
   MUST NOT read the raw logs or attempt to hold the requirements in its own
   context window. Instead, invoke a "Scoping Lead" sub-agent.
2. **Session Resumption:** Before starting investigation, the Scoping Lead MUST
   check if the `remoting/tools/magi-mode/.temp/` directory contains a
   half-finished session (e.g., existing state files). If it does, the agent
   MUST ask the user whether they want to resume the previous session or start a
   new task.

#### Step 2: Investigate Codebase (The Scoping Lead)

1. The Scoping Lead investigates the codebase (`grep_search`, `read_file`) to
   understand context, dependencies, and existing patterns.
2. **Environment Discovery:** Before writing the file, the Scoping Lead MUST
   discover the environment:
   - **VCS:** Check for a `.jj/` directory or run `jj status`. If successful,
     set `vcs` to `"JJ"`. Otherwise, default to `"GIT"`.
   - **Harness:** Check if Jetski tools (e.g., `code_search`, `view_file`) are
     available. If yes, set `harness` to `"JETSKI"`. Otherwise, set to
     `"GENERIC_CLI"`.

#### Step 3: Define Scope (The Scoping Lead)

1. The Scoping Lead writes a strict specification to `project.magi.json`
   conforming to `magi_schema.json`.
2. **Task Type Determination:** The Scoping Lead MUST determine the `task_type`
   based on the request:
   - `IMPLEMENTATION`: Default. For creating new features or fixing bugs. Sets
     `next_stage` to `SCAFFOLDING`.
   - `REVIEW`: For reviewing existing changes or a CL. Sets `next_stage` to
     `PREPARATION`.
   - `AUDIT`: For analyzing existing code for modernization or flaws. Sets
     `next_stage` to `PREPARATION`.
3. **Context Resolution (The Stop-and-Verify Gate):** The Scoping Lead MUST
   verify that all external context (e.g., Buganizer links, documentation URLs)
   has been successfully retrieved and parsed. If any link returned a login
   prompt, redirect, or error, the Scoping Lead MUST halt, report the failure to
   the human, and request the raw text of the missing context. It MUST NOT
   proceed with a hallucinated or "fallback" scope.
4. **Approach Confirmation (The Dynamic Gate):** The Scoping Lead MUST assess
   the ambiguity of the request.
   - **Low Ambiguity:** If the human provided prescriptive instructions (e.g.,
     "Add feature X to file Y"), the Scoping Lead sets `ambiguity_level: "LOW"`.
   - **High Ambiguity:** If the request is exploratory, implies multiple viable
     paths, or is underspecified, the Scoping Lead sets
     `ambiguity_level: "HIGH"`. **The Gate:** If `ambiguity_level == "HIGH"` OR
     `context_resolved == false`, the Orchestrator MUST pause for human
     intervention. If `ambiguity_level == "LOW"` AND `context_resolved == true`,
     the Orchestrator MAY auto-proceed to the next stage.
5. **JSON Contract (`project.magi.json`):** See [EXAMPLES.md](./EXAMPLES.md) for
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
   as the "Base Scaffold" so all parallel Domain Experts share the exact same
   multi-file API and test boundaries.

#### Step 3: Select Experts (The Engineering Manager)

1. **Needs Assessment:** Now that the scope of the change is defined by the
   scaffold, the Orchestrator MUST act as or invoke an "Engineering Manager"
   sub-agent. The Engineering Manager reads `project.magi.json` to understand
   the requirements and `src/remoting/tools/magi-mode/PERSONAS.md` (the routing
   catalog) to assess and select both the **Implementors** and the
   **Reviewers**.
   - **Implementors**: Defaults to the "Big Three" (Security, Performance,
     Architect). The Engineering Manager MAY select additional Domain Experts if
     the task requires specialized work. If `task_type` is `REVIEW` or `AUDIT`,
     the Engineering Manager MUST skip selecting Implementors.
   - **Reviewers**: Includes all selected Implementors, plus the Language
     Expert, and any relevant Domain Experts. If the change includes new or
     modified tests, the Test Expert MUST also be included. The Engineering
     Manager returns the absolute file paths of these selected personas
     (separated into `implementors` and `reviewers`) to the Orchestrator. The
     Engineering Manager MUST also read its own persona file from
     `src/remoting/tools/magi-mode/personas/core/engineering_manager.json` and
     evaluate the selection checklist to ensure all technical dimensions are
     covered for both roles. It MUST include the checklist evaluation in its
     JSON output. The Engineering Manager MUST set `next_stage` to
     `IMPLEMENTATION` for `IMPLEMENTATION` tasks, or `CRITIQUE` for `REVIEW` and
     `AUDIT` tasks.
2. **State Initialization:** The Orchestrator MUST directly write the initial
   State Block to `state_block.magi.json` using the schema defined in
   `magi_schema.json` to prevent invoking a boilerplate agent. The `checklist`
   field is initialized with the **Union Set** of all checklist keys from every
   selected persona, set to `false`.
3. **Review Mode Selection:** The Engineering Manager MUST select the
   `review_mode` (`SUPERVISOR` or `CONSENSUS`) and include it in the initial
   `state_block.magi.json`.
   - **CONSENSUS:** Use if `auditability_level == "VERBOSE"`,
     `paranoia_mode == true`, or if the number of selected reviewers > 3. For
     critical or P1 tasks, the Engineering Manager SHOULD mandate a minimum of 3
     specialized reviewers to ensure broad coverage.
   - **SUPERVISOR:** Default for all other cases.
4. **State Transport Selection:** The Engineering Manager MUST calculate a
   Context Bloat Risk Score `(Reviewer Count * Target Files)` and select
   `state_transport`:
   - **FILE_IO:** Use if `paranoia_mode == true` or Risk Score > 15. All drafts,
     reviews, and state updates are written to `.magi.*.json` files.
   - **EPHEMERAL_WITH_LOGS:** Default for low-risk tasks. Structured data is
     passed natively in JSON payloads to the Orchestrator, but also teed to
     `.magi.*.json` files on disk for auditing.
   - **EPHEMERAL:** Use only when minimal disk I/O is required and no auditing
     is needed.
5. **In-Memory Validation:** If an `EPHEMERAL` mode is active, the Orchestrator
   MUST strictly validate incoming JSON payloads against `magi_schema.json` in
   memory before proceeding.
6. **The Recruiter (Talent Acquisition):** If the Engineering Manager determines
   that a required expertise is lacking in the current catalog, they MUST invoke
   a "Recruiter" sub-agent to dynamically generate the missing persona file.
7. **Transparency:** The Orchestrator MUST output the Engineering Manager's
   persona selection logic and `review_mode` decision to the human.
8. **Opaque Passing:** The Orchestrator passes the *file paths* of the selected
   personas to the sub-agents. The sub-agents read the file from disk to load
   their mandate, keeping the Orchestrator's context window lean.
9. **JSON Contract (`state_block.magi.json`):** See [EXAMPLES.md](./EXAMPLES.md)
   for a full example.

#### Step 4: Implement (Domain Experts)

1. **Parallel Implementation:** Invoke the selected expert sub-agents in
   parallel (`wait_for_previous: false`). Instruct each to implement the stubbed
   internals from the Base Scaffold.
2. **Mandates for Domain Experts:**
   - **Production Code Focus:** Domain experts SHOULD focus primarily on
     implementing the production code logic.
   - **Production Hardening:** Domain experts MUST adhere to the **Production
     Hardening Checklist** (defined at the end of this document) during
     implementation.
   - **Domain Edge Cases:** If a domain expert identifies specific edge cases or
     scenarios that need verification, they MUST add a stubbed test case in the
     test file (with both `ADD_FAILURE() << "NOT IMPLEMENTED"` and a descriptive
     TODO comment) rather than fully implementing the test.
   - **Test Hooks & Accessors:** Domain experts MUST provide any necessary
     public accessors, test-only hooks, or `friend` declarations in the
     production code that the Test Expert will need to verify internal state.
3. **File I/O:** Each sub-agent MUST read `project.magi.json` to ground their
   implementation in the actual requirements. They MUST securely save their
   draft to disk using the versioned naming convention
   `[filename].[persona].magi.[iteration]` (e.g., `host.cc.security.magi.1`).
   Expert sub-agents SHOULD signal `next_stage: SYNTHESIS` upon completion.
   *Note: Sub-agents are permitted to change scaffolded signatures if their
   priority requires it.*

#### Step 5: Synthesize (The Synthesizing Architect)

1. **Conflict Resolution:** The Synthesizing Architect MUST use a surgical 3-way
   merge strategy (Base Scaffold + Draft A + Draft B) rather than full-file
   overwrites to resolve conflicts between domain experts.
2. **Hardening Audit:** The Synthesizing Architect MUST perform a final audit
   against the **Production Hardening Checklist** during synthesis to ensure
   merged code maintains architectural integrity.
3. **Synthesis Build:** If `build_targets` are defined in `project.magi.json`,
   the Synthesizing Architect MUST run the local build/test suite on "Draft A".
   The Orchestrator MUST verify that the scaffold compiles and that the tests
   still fail with "NOT IMPLEMENTED" (as Domain Experts only added stubbed
   tests). The Synthesizing Architect MUST attach the build logs to the
   synthesis report before signaling `next_stage: TEST_FILLING`.

#### Step 6: Implement Tests (The Test Expert)

1. Fill out the actual implementation of tests.

#### Step 7: Verification Build (The Test Expert)

1. Run tests to verify they PASS. If they fail due to implementation bugs, loop
   back to Step 5 or 4.

### Stage 3: Refine

#### Step 1: Review (Reviewers)

1. **Blind Critique:** Push the files to be reviewed to the expanded panel of
   Reviewers selected in Stage 2 Step 3.
   - For `IMPLEMENTATION` tasks, this is the synthesized "Draft A".
   - For `REVIEW` and `AUDIT` tasks, there is no synthesized draft, so the
     original target files specified in `project.magi.json` MUST be pushed.
     **File I/O:** Output routing depends on `state_transport`:
   - `FILE_IO`: Save feedback to disk as
     `review.[persona].magi.[iteration].json`.
   - `EPHEMERAL`: Return the JSON object directly to the Orchestrator.
   - `EPHEMERAL_WITH_LOGS`: Return JSON natively AND save to disk.
2. **Prompt Template:**
   > Role Details: Read your mandate from `[persona_file_path]`. Audit Mandate:
   > You are a rigorous, objective auditor. Drop all politeness. Focus
   > exclusively on technical data and facts. Be concise and pointed. Dynamic
   > Strictness (Iteration [N]): \[If N\<=2: "Exhaustively reject for any flaw
   > or deviation based on technical facts." | If N==3-4: "Accept minor nits.
   > Reject only for functional/security bugs." | If N>=5: "Stall prevention.
   > Accept unless catastrophic."\] Project Spec: Read the requirements from
   > `project.magi.json`. Priority: [Priority]. Task: Review Draft [filename].
   > Auditing against your persona checklist. Save a JSON object conforming to
   > `magi_schema.json#definitions/ReviewFeedback` to
   > `review.[persona].magi.[iteration].json`. You MUST set `next_stage` to
   > `"ANALYSIS"`.
3. **Overlapping Mandates:** For critical checklist items (e.g., security, data
   safety), the Engineering Manager SHOULD ensure that at least two independent
   reviewers evaluate the same item to achieve consensus.

#### Step 2: Consolidate (Supervisor/TPM)

1. **Path A: Supervisor Synthesis (Default):** If `review_mode == SUPERVISOR`,
   the Orchestrator (or a specialized Supervisor agent) performs the following
   in a single turn:
   - **Decision:** Read all `review.*.magi.[iteration].json` files.
   - **State Update:** Consolidate the checklists using **Logical AND** (a key
     only becomes `true` in `state_block.magi.json#checklist` if all reviewers
     evaluating it set it to `true`). Append any `unlisted_issues_found` to the
     historical logs. Update `state_block.magi.json` with the new iteration and
     stall count.
   - **Constraint Generation:** Save a strict list of Actionable Constraints
     (generated from all `false` checklist keys and `unlisted_issues_found`) and
     the current `review_mode` to `constraints.magi.[iteration].json`.
   - **Handoff:** Signal `next_stage: SYNTHESIS` (if more work is needed) or
     `TRAINING`.
2. **Path B: Consensus Loop (Verbose/Paranoia):** If `review_mode == CONSENSUS`,
   use the granular relay:
   - **The Review Analyst:** If any agent rejects, this agent reads all
     `review.*.magi.[iteration].json` files, performs the **Logical AND**
     consolidation on the checklists, and saves a strict list of 3-5 Actionable
     Constraints (derived from `false` checklist keys and
     `unlisted_issues_found`), `review_mode: "CONSENSUS"`, and
     `next_stage: TPM_UPDATE` to `constraints.magi.[iteration].json` on disk.
   - **The Technical Program Manager:** Reads
     `constraints.magi.[iteration].json` and updates `state_block.magi.json`
     conforming to `magi_schema.json`. The stall count MUST only be incremented
     if the iteration fails to resolve any checklist items or Actionable
     Constraints. Checks for "flip-flopping". Set `next_stage` to `SYNTHESIS` if
     more work is needed, otherwise `TRAINING`.
   - **Deadlock API:** If Stall Count reaches 3, the Technical Program Manager
     SHOULD attempt an automated fallback (e.g., reducing strictness or
     switching to `SUPERVISOR` mode) before declaring deadlock. If Stall Count
     exceeds 3, the Technical Program Manager MUST output a valid
     `state_block.magi.json` with `next_stage: DEADLOCK` and append a structured
     deadlock report to the `active_constraints` array.
3. **Common Convergence:**
   - **Transparency:** The Orchestrator reads
     `constraints.magi.[iteration].json` and outputs it directly to the user as
     a status update.
   - **Convergence & Iteration:** The Synthesizing Architect reads
     `state_block.magi.json` and `constraints.magi.[iteration].json` to generate
     the next iteration (e.g., "Draft B").
   - **Executive Tie-Breaker (Handover):** If the Orchestrator receives the
     `next_stage: DEADLOCK` signal, it MUST immediately halt the loop, print the
     structured report to the human, and wait for a tie-breaking decision.
   - **CLEANUP:** Do NOT delete `.magi` files yet; the Trainer will need them.
     The Orchestrator reports the final conclusion of the work.

#### Step 3: Train (The Trainer)

1. **Continuous Improvement:** Once consensus is reached, the Orchestrator MUST
   invoke a "Trainer" sub-agent. The Trainer evaluates the final State Block and
   Review Analyst constraints to identify systemic gaps in the Personas'
   knowledge. If a Persona made a recurring mistake or lacked domain context,
   the Trainer proposes an upgrade to their `personas/*.json` file by adding a
   new Boolean constraint to their checklist.
2. **Persona Splitting (Hierarchical Specialization):** The Trainer MUST NOT let
   a persona's checklist exceed 10 items. If adding a new constraint exceeds
   this limit, the Trainer MUST "fork" the persona using a nested directory
   structure representing `[category]/[domain]/[specialty].json` (e.g., split
   `core/security.json` into `core/security/memory.json` and
   `core/security/network.json`). Do not use flat files with underscores. The
   directory depth MUST NOT exceed 5 levels (counting from `/personas`). Migrate
   the relevant checks and update `PERSONAS.md`. The Trainer SHOULD signal
   `next_stage: VALIDATION`.

### Stage 4: Release

#### Step 1: Validate (Release Engineer)

1. Run `git cl presubmit` and full test suite.
2. **Failure Loop:** If validation fails and requires code change, loop back to
   **Stage 3: Step 1 (Review)**! (Trivial fixes like formatting/lint can be
   handled by Release Engineer directly if they pass presubmit).

#### Step 2: Deploy (Release Engineer)

1. **Handoff:** Once Validation passes, the Orchestrator pauses its own actions
   and delegates strictly to the **Release Engineer** sub-agent. The
   Orchestrator passes only two pieces of information: the name of the
   feature/bug, and the list of MAGI files updated by the Trainer/Recruiter.
2. **Exclusive Mandate:** The Release Engineer's exclusive mandate is:
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
     updated by the Recruiter or Trainer as a secondary CL.

### Specialized Modes

- **Paranoia Mode:** For high-stakes security code, use a **Multi-Model Cohort**
  for the Reviewers (up to 9 agents). Run each persona through multiple models.
  Engineering Manager MUST select `review_mode: CONSENSUS`.

### Production Hardening Checklist

The Synthesizing Architect MUST ensure:

1. **Lifetime Safety:** Use `base::RefCountedDeleteOnSequence` for timers.
2. **Zero-Copy:** Prefer `std::move` and `base::RefCountedString`.
3. **DoS Mitigation:** Enforce strict length limits (e.g., 64KB).
4. **Atomic State:** Ensure callback checks (e.g., `if (callback_)`) are
   atomically sound or strictly sequence-enforced to prevent double-runs.

**VCS Isolation Rule:** Any modifications to MAGI files (e.g., adding/updating
personas by the Recruiter or Trainer) MUST be excluded from the feature/bugfix
CL. The staging and submission workflow branches dynamically based on
`project.magi.json#environment/vcs`:

- **For JJ (Jujutsu):** Work in parallel sibling changes (both rooted at
  `main@origin`) from the start: one for the feature/bugfix and one for the MAGI
  upgrades. If they accidentally get mixed, the Release Engineer MUST use
  `jj split` or `jj squash -i` to cleanly separate the changes before pushing.
- **For GIT:** Use standard git branching. Stage *only* product source files for
  the feature CL. Stage *only* MAGI updates for the secondary CL.

#### Workspace Management

- **Interim File Isolation**: To minimize permission prompts for the user and
  maintain workspace hygiene, all interim files generated by the protocol (e.g.,
  drafts, reviews, logs) MUST be placed in a dedicated subfolder:
  `remoting/tools/magi-mode/.temp/`.
- **Cleanup**: The Release Engineer (or the agent in charge of cleanup) MUST
  delete the `.temp/` directory at the end of the run, rather than requiring the
  user to add it to `.gitignore`.

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
   a single space separator (`" ".join(array)`) to prevent token merging (e.g.,
   preventing "Your" and "goal" from becoming "Yourgoal"). *Note: This diverges
   from the "Opaque Passing" rule...*
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
