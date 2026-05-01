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
code or summarize state itself. It delegates to four auxiliary personas:
1.  **The Synthesizing Architect:** Writes the actual C++ code by combining
    initial drafts and adhering to constraints provided by the Continuity Analyst.
2.  **The Review Analyst:** Condenses raw feedback from multiple reviewers into a
    strict list of actionable constraints.
3.  **The Continuity Analyst:** Maintains the State Block across rounds,
    explicitly checking for "flip-flopping" or stalled progress, and provides
    the final constraints to the Architect.
4.  **The Liaison:** Reports progress to the human via `update_topic` or chat
    messages at the end of each cycle and at the conclusion of the loop.

**MANDATE:** Every sub-agent invoked in the MAGI protocol MUST call the
`update_topic` tool as its first action to identify its role in the UI (e.g.,
`title="MAGI Persona: Engineering Manager"`).

## Workflow

### 1. Preparation & Persona Selection (The Engineering Manager)
- Identify the target file(s) and the core problem.
- **The Engineering Manager:** The Orchestrator MUST act as or invoke a
  sub-agent acting as the "Engineering Manager". The Engineering Manager reads
  `src/remoting/tools/magi-mode/PERSONAS.md` (the routing catalog) to assess
  and select the most appropriate experts. It returns the absolute file paths
  of their definition files to the Orchestrator.
- **The Recruiter (Talent Acquisition):** If the Engineering Manager determines
  that a required expertise is lacking in the current catalog, they MUST invoke
  a "Recruiter" sub-agent. The Recruiter is responsible for dynamically
  generating the missing persona markdown file in
  `src/remoting/tools/magi-mode/personas/`, updating the `PERSONAS.md` catalog,
  and committing these changes as a separate prerequisite Git/Jujutsu commit
  before the main architectural work begins.
- **Transparency:** The Orchestrator MUST output the Engineering Manager's
  persona selection logic to the human. Ensure the workspace is clean.
- **Opaque Passing:** The Orchestrator passes the *file paths* of the selected
  personas to the sub-agents. The sub-agents use `read_file` to load their
  mandate, keeping the Orchestrator's context window lean.

### 2. Scaffolding (The Pathfinder)
- **Roughing In:** Invoke a "Pathfinder" sub-agent (e.g., Chromium Expert) to
  create a base scaffold.
- **Mandate:** Create necessary files, define class interfaces, set up Mojo
  pipes, and GN/DEPS rules. Leave implementation details empty or stubbed (e.g.,
  `NOTIMPLEMENTED()`).
- **Commit:** The Orchestrator commits this state as the "Base Scaffold" so all
  parallel ideators share the exact same multi-file API boundaries.

### 3. Parallel Compute (Ideation)
Invoke the selected expert sub-agents in parallel (`wait_for_previous: false`).
Instruct each to implement the stubbed internals from the Base Scaffold.
*Note: Sub-agents are permitted to change scaffolded signatures if their
priority
requires it. Their first action must be to call `update_topic` to identify
their specific persona.*

### 4. The Synthesis Phase
Once the ideation agents finish:
1.  **The Continuity Analyst:** Initialize the State Block with the identified
    trade-offs to prevent context bloat:
    *   **Iteration:** [N]
    *   **Personas:** [Selected Experts]
    *   **Resolved:** [Addressed critiques]
    *   **Active Conflicts:** [Specific trade-offs, e.g., "Security vs.
        Performance"]
    *   **Stall Count:** [Count of non-productive iterations]
2.  **The Synthesizing Architect:** Pass the `.magi` drafts to this agent to
    synthesize into "Draft A" in the original file.

### 5. The Consensus Loop (Expanded Review)
1.  **Blind Critique:** Push Draft A to an expanded panel of Reviewers. The
    panel MUST include the core ideators PLUS 3-6 specialized reviewer personas
    (e.g., **Test Expert**, **Readability Expert**, **Cross-Platform Expert**).
    **Prompt Template:**
    > "**IMPORTANT:** Your very first action MUST be to call the `update_topic`
    > tool with the `title` set to your assigned MAGI Persona name (e.g.,
    > 'MAGI Persona: WebRTC Expert') and a `summary` of your immediate goal.
    >
    > Role Details: Read your mandate from `[persona_file_path]`.
    > Priority: [Priority].
    > Task: Review Draft [filename].
    > Output ONLY: `Verdict: [ACCEPT/REJECT]` and `Reasoning: [Bullet points]`."
2.  **The Review Analyst:** If any agent rejects, pass all feedback to the
    Review Analyst agent to condense into a strict list of 3-5 Actionable
    Constraints.
3.  **The Continuity Analyst:** Pass the Review Analyst's constraints to the
    Continuity Analyst. The Continuity Analyst updates the State Block and
    checks for "flip-flopping" (e.g., Constraint 1 violates a constraint from
    Round 1) and modifies the feedback provided to the Architect if necessary
    to ensure progress.
4.  **The Liaison:** Provide a brief status update to the human (e.g., via
    `update_topic` or chat) detailing what is being considered and the
    decision process for this cycle.
5.  **Convergence & Iteration:** Pass the updated State Block and Draft back to
    the Synthesizing Architect to generate Draft B.
6.  **Executive Tie-Breaker (Handover):** If the Continuity Analyst detects a
    deadlock, the Orchestrator MUST pause and escalate to the human with a
    structured **Deadlock Report**. Feed the human's decision back to break the
    tie.
7.  **CLEANUP:** Immediately execute `rm *.magi.*` once consensus is reached.
    The Liaison should report the final conclusion of the work.

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

### 8. Validation
Run the standard suite (`git cl presubmit`, `gn check`, and unit tests).

## When to Invoke
- When an automated review finds a flaw that is hard to resolve without
  trade-offs.
- When a bug occurs only on a specific platform and the fix might impact others.
- When adding a new feature that has significant performance or security
  implications.
