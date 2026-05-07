---
name: magi-mode
description: Resolves complex or ambiguous architectural problems in //remoting
  using the MAGI multi-agent debate protocol. Use when the primary agent is
  stuck, faces conflicting platform requirements, or needs a high-level
  architectural decision.
---

# MAGI Protocol (Multi-Agent Guided Iteration)

This skill implements the MAGI multi-agent debate protocol to resolve complex,
high-stakes, or ambiguous problems. It utilizes a "Consensus Loop" of
specialized sub-agents to explore dimensions, moderate feedback, and synthesize
code without overwhelming the main Orchestrator.

## The Core Personas (Dynamic Selection)

The Orchestrator MUST dynamically select 3+ expert personas best suited for
the specific task. While **Security**, **Performance**, and **Architect** are
the defaults for production code, the Orchestrator may swap them for others
(e.g., **Test Expert**, **Platform Expert**) if the task context warrants it.

Each persona prompt MUST be anchored with the relevant **Platform** and
**Language** (e.g., "Windows C++ Security Expert").

## The Auxiliary Personas (The Consensus Loop)

To prevent context bloat and semantic drift, the Orchestrator MUST NOT write
code or summarize state itself. It delegates to several auxiliary personas:
1.  **The Scoping Lead:** Investigates the initial bug/feature request, searches
    the codebase, and writes a strict `project.magi.json` specification document
    conforming to `magi_schema.json`.
2.  **The Synthesizing Architect:** Writes the actual C++ code by combining
    initial drafts and adhering to constraints provided by the Supervisor or
    Technical Program Manager.
3.  **The Review Analyst:** (Consensus Mode Only) Condenses raw feedback from
    multiple reviewers into a strict list of actionable constraints in
    `constraints.magi.[iteration].json` conforming to `magi_schema.json`.
4.  **The Technical Program Manager:** (Consensus Mode Only) Maintains the State
    Block across rounds, explicitly checking for "flip-flopping" or stalled
    progress, and provides the final constraints to the Synthesizing Architect.
5.  **The Supervisor:** (Supervisor Mode Only) Acts as a hub to read all
    reviews, update the State Block, and generate the final constraints for the
    Synthesizing Architect in a single turn.
6.  **The Trainer:** Captures knowledge or systemic gaps discovered during the
    Consensus Loop and upgrades the expert Persona definitions.
7.  **The Release Engineer:** A terminal agent invoked with a clean context to
    handle workspace hygiene, formatting, resolving lint and presubmit errors,
    verifying the files in the CL are expected, and the final staging/upload of
    CLs.

**TONE MANDATE (SIGNAL-TO-NOISE):** To eliminate conversational noise, conserve
tokens, and maximize parsing stability, the Orchestrator MUST instruct ALL
sub-agents (except itself) to adopt a neutral, data-driven tone.
*   **Zero Preamble/Postamble:** Sub-agents MUST NOT use conversational filler,
    greetings, or explanations of their work.
*   **Artifacts Only:** If an agent's mandate is to generate JSON or C++ code,
    its entire output MUST consist *only* of that raw data structure.
*   **Reviewers:** Reviewers MUST act as rigorous, objective auditors focusing
    strictly on technical facts and data.

**TOOL AGNOSTIC MANDATE:** The protocol instructions MUST remain tool-agnostic.
Do not assume specific tool names (e.g., `update_topic`, `read_file`,
`write_file`). Use generic terms like "read from disk," "save to disk," or
"report status."

**ENVIRONMENT GROUNDING MANDATE:** All sub-agents MUST read
`project.magi.json#environment` immediately upon invocation to ground
themselves in the active VCS (`JJ` or `GIT`) and Harness (`JETSKI` or
`GENERIC_CLI`). They MUST adjust their tool usage and command construction
natively to match this environment.

**PHASE SIGNALING:** The Orchestrator MUST use an appropriate status-reporting
mechanism prior to invoking any sub-agents to clearly identify the current phase
of the MAGI protocol to the user (e.g., "MAGI Phase 2: Engineering Manager").

**DECENTRALIZED HANDOFFS:** To reduce Orchestrator overhead, agents SHOULD
include a `next_phase` field in their JSON output to signal the intended
successor. The environment will use this token to automatically route to the
next expert.
*   **Scoping Lead:** `SCAFFOLDING`
*   **Architect / Test Expert:** `PREPARATION`
*   **Engineering Manager:** `IMPLEMENTATION`
*   **Domain Experts:** `SYNTHESIS`
*   **Synthesizing Architect:** `CRITIQUE`
*   **Reviewers:** `ANALYSIS`
*   **Review Analyst:** `TPM_UPDATE`
*   **Technical Program Manager:** `SYNTHESIS` (if iteration needed) or
    `TRAINING`
*   **Supervisor:** `SYNTHESIS` (if iteration needed) or `TRAINING`
*   **Trainer:** `VALIDATION`
*   **Validation:** `DEPLOYMENT`

## Workflow

### 0. Investigation & Specification (The Scoping Lead)
- **The Investigation:** When a bug or feature is requested, the Orchestrator
  MUST NOT read the raw logs or attempt to hold the requirements in its own
  context window. Instead, invoke a "Scoping Lead" sub-agent.
- **The Specification:** The Scoping Lead investigates the codebase
  (`grep_search`, `read_file`) to locate the relevant code and writes a strict
  specification to `project.magi.json` conforming to `magi_schema.json`.

  **Environment Discovery:** Before writing the file, the Scoping Lead MUST
  discover the environment:
  *   **VCS:** Check for a `.jj/` directory or run `jj status`. If successful,
      set `vcs` to `"JJ"`. Otherwise, default to `"GIT"`.
  *   **Harness:** Check if Jetski tools (e.g., `code_search`, `view_file`) are
      available. If yes, set `harness` to `"JETSKI"`. Otherwise, set to
      `"GENERIC_CLI"`.

  The `project.magi.json` file MUST contain a `next_phase` of `SCAFFOLDING` and
  the following structure:
  ```json
  {
    "$schema": "./magi_schema.json#definitions/ProjectSpec",
    "checklist": {},
    "goal": "A one-sentence summary of the fix/feature.",
    "target_files": ["Absolute paths to the files that must be modified."],
    "anti_goals": ["What should explicitly NOT be changed."],
    "edge_cases": ["Specific warnings from logs or code context."],
    "next_phase": "SCAFFOLDING",
    "paranoia_mode": false,
    "auditability_level": "NORMAL",
    "environment": {
      "vcs": "JJ",
      "harness": "JETSKI"
    }
  }
  ```

### 1. Scaffolding (The Architect & Test Phase)
- **Roughing In (The Architect):** First, invoke an Architect sub-agent. The
  Architect MUST read `project.magi.json` to understand the goal. Their mandate
  is to create necessary files, define class interfaces, set up Mojo pipes, and
  GN/DEPS rules. Leave implementation details empty or stubbed (e.g.,
  `NOTIMPLEMENTED()`). The Architect SHOULD signal `next_phase: SCAFFOLDING` (if
  Test Expert yet to be invoked) or `PREPARATION`.
- **Test-Driven Development (The Test Expert):** Second, invoke a Test Expert
  sub-agent to establish the testing boundaries. Their mandate is to add test
  files (`*_unittest.cc`), define the required test fixtures, and stub out the
  critical test cases based on the Architect's scaffold. The Test Expert SHOULD
  signal `next_phase: PREPARATION`.
- **Snapshot:** The Orchestrator records this state (e.g., as a local commit) as
  the "Base Scaffold" so all parallel Domain Experts share the exact same
  multi-file API and test boundaries. The Synthesizing Architect will
  eventually amend or squash the final implementation into this scaffold,
  ensuring no broken stubs land in the final CL. The Orchestrator MUST signal
  `next_phase: PREPARATION`.

### 2. Preparation & Persona Selection (The Engineering Manager)
- **Needs Assessment:** Now that the scope of the change is defined by the
  scaffold, the Orchestrator MUST act as or invoke an "Engineering Manager"
  sub-agent. The Engineering Manager reads `project.magi.json` to understand the
  requirements and `src/remoting/tools/magi-mode/PERSONAS.md` (the routing
  catalog) to assess and select the most appropriate Domain Experts required
  to implement the stubs. It returns the absolute file paths of their definition
  files to the Orchestrator.
- **Review Mode Selection:** The Engineering Manager MUST select the
  `review_mode` (`SUPERVISOR` or `CONSENSUS`) and include it in the initial
  `state_block.magi.json`.
    *   **CONSENSUS:** Use if `auditability_level == "VERBOSE"`,
        `paranoia_mode == true`, or if the number of selected reviewers > 5.
    *   **SUPERVISOR:** Default for all other cases.
- **State Transport Selection:** The Engineering Manager MUST calculate a
  Context Bloat Risk Score `(Reviewer Count * Target Files)` and select
  `state_transport`:
    *   **FILE_IO:** Use if `paranoia_mode == true` or Risk Score > 15. All
        drafts, reviews, and state updates are written to `.magi.*.json` files.
    *   **EPHEMERAL_WITH_LOGS:** Use if `auditability_level == "VERBOSE"`.
        Structured data is passed natively in JSON payloads to the Orchestrator,
        but also teed to `.magi.*.json` files on disk for auditing.
    *   **EPHEMERAL:** Default. C++ Drafts go to disk, but reviews, constraints,
        and state updates are passed exclusively in JSON payloads.
  *In-Memory Validation:* If an `EPHEMERAL` mode is active, the Orchestrator
  MUST strictly validate incoming JSON payloads against `magi_schema.json` in
  memory before proceeding, as disk-based presubmit checks will be bypassed.
- **The Recruiter (Talent Acquisition):** If the Engineering Manager determines
  that a required expertise is lacking in the current catalog, they MUST invoke
  a "Recruiter" sub-agent. The Recruiter is responsible for dynamically
  generating the missing persona markdown file in
  `src/remoting/tools/magi-mode/personas/` and updating the `PERSONAS.md`
  catalog.
  *CRITICAL:* These MAGI system changes MUST NOT be entangled with the main
  work CL (see VCS Isolation rule below).
- **Transparency:** The Orchestrator MUST output the Engineering Manager's
  persona selection logic and `review_mode` decision to the human. Ensure the
  workspace is clean. The Engineering Manager MUST set `next_phase` to
  `IMPLEMENTATION` in its output.
- **Opaque Passing:** The Orchestrator passes the *file paths* of the selected
  personas to the sub-agents. The sub-agents read the file from disk to load
  their mandate, keeping the Orchestrator's context window lean.

### 3. Parallel Implementation
Invoke the selected expert sub-agents in parallel (`wait_for_previous: false`).
Instruct each to implement the stubbed internals from the Base Scaffold.
**File I/O:** Each sub-agent MUST read `project.magi.json` to ground their
implementation in the actual requirements. They MUST securely save their draft
to disk using the versioned naming convention
`[filename].[persona].magi.[iteration]` (e.g., `host.cc.security.magi.1`).
Expert sub-agents SHOULD signal `next_phase: SYNTHESIS` upon completion.
*Note: Sub-agents are permitted to change scaffolded signatures if their
priority requires it.*

### 4. The Synthesis Phase
Once the Domain Experts finish:
1.  **State Initialization:** The Orchestrator MUST directly write the initial
    State Block to `state_block.magi.json` using the schema defined in
    `magi_schema.json` to prevent invoking a boilerplate agent:
    ```json
    {
      "$schema": "./magi_schema.json#definitions/StateBlock",
      "iteration": 1,
      "stall_count": 0,
      "active_constraints": [],
      "resolved_constraints": [],
      "personas": ["[Selected Experts]"],
      "next_phase": "CRITIQUE",
      "review_mode": "[SUPERVISOR/CONSENSUS]",
      "state_transport": "[FILE_IO/EPHEMERAL/EPHEMERAL_WITH_LOGS]"
    }
    ```
2.  **The Synthesizing Architect:** Read the `[filename].[persona].magi.[N]`
    drafts and synthesize them into "Draft A" in the original file. Signal
    `next_phase: CRITIQUE`.
### 5. The Review Workflow (Consensus Loop vs. Supervisor)
1.  **Blind Critique:** Push Draft A to an expanded panel of Reviewers.
    **File I/O:** Output routing depends on `state_transport`:
    *   `FILE_IO`: Save feedback to disk as
        `review.[persona].magi.[iteration].json`.
    *   `EPHEMERAL`: Return the JSON object directly to the Orchestrator.
    *   `EPHEMERAL_WITH_LOGS`: Return JSON natively AND save to disk.
    **Prompt Template:**
    > Role Details: Read your mandate from `[persona_file_path]`.
    > Audit Mandate: You are a rigorous, objective auditor. Drop all
    >   politeness. Focus exclusively on technical data and facts. Be concise
    >   and pointed.
    > Dynamic Strictness (Iteration [N]): [If N<=2: "Exhaustively reject for any
    >   flaw or deviation based on technical facts." | If N==3-4: "Accept minor
    >   nits. Reject only for functional/security bugs." | If N>=5: "Stall
    >   prevention. Accept unless catastrophic."]
    > Project Spec: Read the requirements from `project.magi.json`.
    > Priority: [Priority].
    > Task: Review Draft [filename]. Save a JSON object with `verdict`
    >   ("ACCEPT" or "REJECT"), `reasoning` (array of bullet points), `comments`
    >   (array of objects with `file`, optional `line`, and `comment`), and
    >   `next_phase` ("ANALYSIS") to `review.[persona].magi.[iteration].json`.

#### Path A: Supervisor Synthesis (Default)
If `review_mode == SUPERVISOR`, the Orchestrator (or a specialized Supervisor
agent) performs the following in a single turn:
1.  **Decision:** Read all `review.*.magi.[iteration].json` files.
2.  **State Update:** Update `state_block.magi.json` with the new iteration
    and stall count.
3.  **Constraint Generation:** Save a strict list of Actionable Constraints and
    the current `review_mode` to `constraints.magi.[iteration].json`.
4.  **Handoff:** Signal `next_phase: SYNTHESIS` (if more work is needed) or
    `TRAINING`.

#### Path B: Consensus Loop (Verbose/Paranoia)
If `review_mode == CONSENSUS`, use the granular relay:
1.  **The Review Analyst:** If any agent rejects, this agent reads all
    `review.*.magi.[iteration].json` files and saves a strict list of 3-5
    Actionable Constraints, `review_mode: "CONSENSUS"`, and
    `next_phase: TPM_UPDATE` to `constraints.magi.[iteration].json` on disk.
2.  **The Technical Program Manager:** Reads `constraints.magi.[iteration].json`
    and updates `state_block.magi.json` conforming to `magi_schema.json`. Checks
    for "flip-flopping" (e.g., Constraint 1 violates a constraint from Round 1).
    Set `next_phase` to `SYNTHESIS` if more work is needed, otherwise
    `TRAINING`.
    **Deadlock API:** If `Stall Count` exceeds 3, the Technical Program
    Manager MUST output a valid `state_block.magi.json` with
    `next_phase: DEADLOCK` and append a structured deadlock report (Core
    Conflict, Blocked Personas, Human Decision Needed) to the
    `active_constraints` array.

#### Common Convergence
1.  **Transparency:** The Orchestrator reads `constraints.magi.[iteration].json`
    and outputs it directly to the user as a status update. Do NOT invoke a
    separate Liaison agent.
2.  **Convergence & Iteration:** The Synthesizing Architect reads
    `state_block.magi.json` and `constraints.magi.[iteration].json` to generate
    the next iteration (e.g., "Draft B").
3.  **Executive Tie-Breaker (Handover):** If the Orchestrator receives the
    `next_phase: DEADLOCK` signal, it MUST immediately halt the loop, print the
    structured report to the human, and wait for a tie-breaking decision.
4.  **CLEANUP:** Do NOT delete `.magi` files yet; the Trainer will need them.
    The Orchestrator reports the final conclusion of the work.

### 6. Specialized Modes
*   **Paranoia Mode:** For high-stakes security code, use a **Multi-Model
    Cohort** for the Reviewers (up to 9 agents). Run each persona through
    multiple models. Engineering Manager MUST select `review_mode: CONSENSUS`.

### 7. Production Hardening Checklist
The Synthesizing Architect MUST ensure:
1.  **Lifetime Safety:** Use `base::RefCountedDeleteOnSequence` for timers.
2.  **Zero-Copy:** Prefer `std::move` and `base::RefCountedString`.
3.  **DoS Mitigation:** Enforce strict length limits (e.g., 64KB).
4.  **Atomic State:** Ensure callback checks (e.g., `if (callback_)`) are
    atomically sound or strictly sequence-enforced to prevent double-runs.

### 8. Continuous Improvement (The Trainer)
Once consensus is reached, the Orchestrator MUST invoke a "Trainer" sub-agent.
The Trainer evaluates the final State Block and Review Analyst constraints to
identify systemic gaps in the Personas' knowledge. If a Persona made a recurring
mistake or lacked domain context, the Trainer proposes an upgrade to their
`personas/*.json` file by adding a new Boolean constraint to their checklist.

**Persona Splitting (Hierarchical Specialization):** The Trainer MUST NOT let a
persona's checklist exceed 10 items. If adding a new constraint exceeds this
limit, the Trainer MUST "fork" the persona using a nested directory structure
representing `[category]/[domain]/[specialty].json` (e.g., split
`core/security.json` into `core/security/memory.json` and
`core/security/network.json`). Do not use flat files with underscores. Migrate
the relevant checks and update `PERSONAS.md`.
The Trainer SHOULD signal `next_phase: VALIDATION`.

**VCS Isolation Rule:** Any modifications to MAGI files (e.g., adding/updating
personas by the Recruiter or Trainer) MUST be excluded from the feature/bugfix
CL. The staging and submission workflow branches dynamically based on
`project.magi.json#environment/vcs`:
*   **For JJ (Jujutsu):** Work in parallel sibling changes (both rooted at
    `main@origin`) from the start: one for the feature/bugfix and one for the
    MAGI upgrades. If they accidentally get mixed, the Release Engineer MUST
    use `jj split` or `jj squash -i` to cleanly separate the changes before
    pushing.
*   **For GIT:** Use standard git branching. Stage *only* product source files
    for the feature CL. Stage *only* MAGI updates for the secondary CL.

### 9. Validation
Run the standard suite (`git cl presubmit`, `gn check`, and unit tests). Upon
success, signal `next_phase: DEPLOYMENT`.

### 10. Deployment (The Release Engineer)
Once Validation passes, the Orchestrator pauses its own actions and delegates
strictly to the **Release Engineer** sub-agent. The Orchestrator passes only two
pieces of information: the name of the feature/bug, and the list of MAGI files
updated by the Trainer/Recruiter.

The Release Engineer's **exclusive mandate** is:
1. **Workspace Hygiene:** Read the discovered VCS from
   `project.magi.json#environment/vcs`. Run `jj st` (for JJ) or `git status`
   (for Git). Detect and revert accidental submodule bumps. Remove any
   lingering temporary files generated by the protocol (e.g., `*.magi`,
   `*.magi.*`).
2. **Formatting:** Enforce `git cl format` or project-specific formatters.
3. **The Feature CL:** Upload the main feature CL containing only the product
   source changes (using the VCS-specific track defined in the VCS Isolation
   Rule).
4. **The MAGI CL:** Create a separate change/bookmark (for JJ) or branch (for
   Git). Stage and upload the `PERSONAS.md` and `personas/**/*.json` files
   updated by the Recruiter or Trainer as a secondary CL.

## Harness Optimizations (Jetski Mode)
If `project.magi.json#environment/harness == "JETSKI"`, the Orchestrator:
1. **Direct Prompt Injection:** SHOULD read the `personas/**/*.json` files and
   inject their `mandate` and `checklist` directly into the `Prompt` or `Role`
   arguments of `invoke_subagent` tool calls. This eliminates a redundant
   file-reading turn (`view_file` call) for every sub-agent.
2. **Orchestrator Routing:** MUST act as the active routing environment by
   parsing the `next_phase` token from sub-agent output JSONs and manually
   calling the next tool (standard Jetski does not have an automatic
   background router).

## When to Invoke
- When an automated review finds a flaw that is hard to resolve without
  trade-offs.
- When a bug occurs only on a specific platform and the fix might impact others.
- When adding a new feature that has significant performance or security
  implications.
