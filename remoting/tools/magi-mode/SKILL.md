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
    the codebase, and writes a strict `project.magi.md` specification document.
2.  **The Synthesizing Architect:** Writes the actual C++ code by combining
    initial drafts and adhering to constraints provided by the Technical Program
    Manager.
3.  **The Review Analyst:** Condenses raw feedback from multiple reviewers into
    a strict list of actionable constraints.
4.  **The Technical Program Manager:** Maintains the State Block across rounds,
    explicitly checking for "flip-flopping" or stalled progress, and provides
    the final constraints to the Synthesizing Architect.
5.  **The Trainer:** Captures knowledge or systemic gaps discovered during the
    Consensus Loop and upgrades the expert Persona definitions.
6.  **The Release Engineer:** A terminal agent invoked with a clean context to
    handle workspace hygiene, formatting, resolving lint and presubmit errors,
    verifying the files in the CL are expected, and the final staging/upload of
    CLs.

**TOOL AGNOSTIC MANDATE:** The protocol instructions MUST remain tool-agnostic.
Do not assume specific tool names (e.g., `update_topic`, `read_file`,
`write_file`). Use generic terms like "read from disk," "save to disk," or
"report status."

**PHASE SIGNALING:** The Orchestrator MUST use an appropriate status-reporting
mechanism prior to invoking any sub-agents to clearly identify the current phase
of the MAGI protocol to the user (e.g., "MAGI Phase 2: Engineering Manager").

## Workflow

### 0. Investigation & Specification (The Scoping Lead)
- **The Investigation:** When a bug or feature is requested, the Orchestrator
  MUST NOT read the raw logs or attempt to hold the requirements in its own
  context window. Instead, invoke a "Scoping Lead" sub-agent.
- **The Specification:** The Scoping Lead investigates the codebase
  (`grep_search`, `read_file`) to locate the relevant code and writes a strict
  specification to `project.magi.md`. This file MUST contain:
  *   **Goal:** A one-sentence summary of the fix/feature.
  *   **Target Files:** Absolute paths to the files that must be modified.
  *   **Out of Scope / Anti-Goals:** What should explicitly NOT be changed.
  *   **Known Edge Cases / Gotchas:** Specific warnings from logs or code
      context.

### 1. Scaffolding (The Architect & Test Phase)
- **Roughing In (The Architect):** First, invoke an Architect sub-agent. The
  Architect MUST read `project.magi.md` to understand the goal. Their mandate is
  to create necessary files, define class interfaces, set up Mojo pipes, and
  GN/DEPS rules. Leave implementation details empty or stubbed (e.g.,
  `NOTIMPLEMENTED()`).
- **Test-Driven Development (The Test Expert):** Second, invoke a Test Expert
  sub-agent to establish the testing boundaries. Their mandate is to add test
  files (`*_unittest.cc`), define the required test fixtures, and stub out the
  critical test cases based on the Architect's scaffold.
- **Snapshot:** The Orchestrator records this state (e.g., as a local commit) as
  the "Base Scaffold" so all parallel Domain Experts share the exact same
  multi-file API and test boundaries. The Synthesizing Architect will eventually
  amend or squash the final implementation into this scaffold, ensuring no
  broken stubs land in the final CL.

### 2. Preparation & Persona Selection (The Engineering Manager)
- **Needs Assessment:** Now that the scope of the change is defined by the
  scaffold, the Orchestrator MUST act as or invoke an "Engineering Manager"
  sub-agent. The Engineering Manager reads `project.magi.md` to understand the
  requirements and `src/remoting/tools/magi-mode/PERSONAS.md` (the routing
  catalog) to assess and select the most appropriate Domain Experts required
  to implement the stubs. It returns the absolute file paths of their definition
  files to the Orchestrator.
- **The Recruiter (Talent Acquisition):** If the Engineering Manager determines
  that a required expertise is lacking in the current catalog, they MUST invoke
  a "Recruiter" sub-agent. The Recruiter is responsible for dynamically
  generating the missing persona markdown file in
  `src/remoting/tools/magi-mode/personas/` and updating the `PERSONAS.md`
  catalog.
  *CRITICAL:* These MAGI system changes MUST NOT be entangled with the main
  work CL (see VCS Isolation rule below).
- **Transparency:** The Orchestrator MUST output the Engineering Manager's
  persona selection logic to the human. Ensure the workspace is clean.
- **Opaque Passing:** The Orchestrator passes the *file paths* of the selected
  personas to the sub-agents. The sub-agents read the file from disk to load
  their mandate, keeping the Orchestrator's context window lean.

### 3. Parallel Implementation
Invoke the selected expert sub-agents in parallel (`wait_for_previous: false`).
Instruct each to implement the stubbed internals from the Base Scaffold.
**File I/O:** Each sub-agent MUST read `project.magi.md` to ground their
implementation in the actual requirements. They MUST securely save their draft
to disk using the versioned naming convention
`[filename].[persona].magi.[iteration]` (e.g., `host.cc.security.magi.1`).
*Note: Sub-agents are permitted to change scaffolded signatures if their
priority requires it.*

### 4. The Synthesis Phase
Once the Domain Experts finish:
1.  **State Initialization:** The Orchestrator MUST directly write the initial
    State Block to `state_block.magi.md` to prevent invoking a boilerplate
    agent:
    *   **Iteration:** 1
    *   **Personas:** [Selected Experts]
    *   **Resolved:** [None]
    *   **Active Conflicts:** [None]
    *   **Stall Count:** 0
2.  **The Synthesizing Architect:** Read the `[filename].[persona].magi.[N]`
    drafts and synthesize them into "Draft A" in the original file.
### 5. The Consensus Loop (Expanded Review)
1.  **Blind Critique:** Push Draft A to an expanded panel of Reviewers.
    **File I/O:** Each reviewer MUST securely save their feedback to disk in a
    unique file: `review.[persona].magi.[iteration].md`. Do NOT return feedback
    as text to the Orchestrator.
    **Prompt Template:**
    > Role Details: Read your mandate from `[persona_file_path]`.
    > Project Spec: Read the requirements from `project.magi.md`.
    > Priority: [Priority].
    > Task: Review Draft [filename]. Save `Verdict: [ACCEPT/REJECT]` and
    > `Reasoning: [Bullet points]` to `review.[persona].magi.[iteration].md`.
2.  **The Review Analyst:** If any agent rejects, this agent reads all
    `review.*.magi.[iteration].md` files and saves a strict list of 3-5
    Actionable Constraints to `constraints.magi.[iteration].md` on disk.
3.  **The Technical Program Manager:** Reads `constraints.magi.[iteration].md`
    and updates `state_block.magi.md`. Checks for "flip-flopping" (e.g.,
    Constraint 1 violates a constraint from Round 1).
    **Deadlock API:** If `Stall Count` exceeds 3, the Technical Program
    Manager's ONLY output must be the exact string `STATUS: DEADLOCK` followed
    by a structured report (Core Conflict, Blocked Personas, Human Decision
    Needed).
4.  **Transparency:** The Orchestrator reads `constraints.magi.[iteration].md`
    and outputs it directly to the user as a status update. Do NOT invoke a
    separate Liaison agent.
5.  **Convergence & Iteration:** The Synthesizing Architect reads
    `state_block.magi.md` and `constraints.magi.[iteration].md` to generate
    the next iteration (e.g., "Draft B").
6.  **Executive Tie-Breaker (Handover):** If the Orchestrator receives the
    `STATUS: DEADLOCK` string, it MUST immediately halt the loop, print the
    structured report to the human, and wait for a tie-breaking decision.
7.  **CLEANUP:** Do NOT delete `.magi` files yet; the Trainer will need them.
    The Orchestrator reports the final conclusion of the work.

### 6. Specialized Modes
*   **Paranoia Mode:** For high-stakes security code, use a **Multi-Model
    Cohort** for the Reviewers (up to 9 agents). Run each persona through
    multiple models.

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
`personas/*.md` file and applies the improvements.

**VCS Isolation Rule:** Any modifications to MAGI files (e.g., adding/updating
personas by the Recruiter or Trainer) MUST be excluded from the feature/bugfix
CL. Ensure MAGI paths are not staged during upload. Once the main solution is
uploaded, create a completely separate, independent CL for the MAGI upgrades.

### 9. Validation
Run the standard suite (`git cl presubmit`, `gn check`, and unit tests).

### 10. Deployment (The Release Engineer)
Once Validation passes, the Orchestrator pauses its own actions and delegates
strictly to the **Release Engineer** sub-agent. The Orchestrator passes only two
pieces of information: the name of the feature/bug, and the list of MAGI files
updated by the Trainer/Recruiter.

The Release Engineer's **exclusive mandate** is:
1. **Workspace Hygiene:** Run `git status` / `jj st`. Detect and revert
   accidental submodule bumps. Remove any lingering temporary files generated by
   the protocol (e.g., `*.magi`, `*.magi.*`).
2. **Formatting:** Enforce `git cl format` or project-specific formatters.
3. **The Feature CL:** Stage *only* the product source files. Verify the commit
   message. Upload the main feature CL.
4. **The MAGI CL:** Create a new branch/bookmark. Stage the `PERSONAS.md` and
   `personas/*.md` files updated by the Recruiter/Trainer. Upload the secondary
   CL.

## When to Invoke
- When an automated review finds a flaw that is hard to resolve without
  trade-offs.
- When a bug occurs only on a specific platform and the fix might impact others.
- When adding a new feature that has significant performance or security
  implications.
