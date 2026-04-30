---
name: magi-mode
description: Resolves complex or ambiguous architectural problems in //remoting
  using the MAGI multi-agent debate protocol. Use when the primary agent is
  stuck, faces conflicting platform requirements, or needs a high-level
  architectural decision.
---

# MAGI Protocol (Multi-Agent Guided Iteration)

This skill implements the MAGI multi-agent debate protocol to resolve complex,
high-stakes, or ambiguous problems. It utilizes a "Funnel Architecture" of
specialized sub-agents to explore dimensions, moderate feedback, and synthesize
code without overwhelming the main Orchestrator.

## The Core Personas (Dynamic Selection)

The Orchestrator MUST dynamically select 3+ expert personas best suited for
the specific task. While **Security**, **Performance**, and **Architect** are
the defaults for production code, the Orchestrator may swap them for others
(e.g., **Test Expert**, **Platform Expert**) if the task context warrants it.

Each persona prompt MUST be anchored with the relevant **Platform** and
**Language** (e.g., "Windows C++ Security Expert").

## The Auxiliary Personas (The Funnel)

To prevent context bloat and semantic drift, the Orchestrator MUST NOT write
code or summarize state itself. It delegates to three auxiliary personas:
1.  **The Synthesizing Architect:** Writes the actual C++ code by combining
    initial drafts and adhering to constraints provided by the Moderator.
2.  **The Moderator:** Condenses raw feedback from multiple reviewers into a
    strict list of actionable constraints.
3.  **The Scribe:** Maintains the State Block across rounds, explicitly checking
    for "flip-flopping" or stalled progress.

## Workflow

### 1. Preparation & Persona Selection (The Recruiter)
- Identify the target file(s) and the core problem.
- **The Recruiter:** The Orchestrator MUST invoke a sub-agent acting as the
  "Recruiter". The Recruiter reads `src/remoting/tools/magi-mode/PERSONAS.md`
  (the routing catalog) to select the most appropriate experts. It returns the
  absolute file paths of their definition files to the Orchestrator.
- **Transparency:** The Orchestrator MUST output the Recruiter's persona
  selection logic to the human. Ensure the workspace is clean.
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
requires it.*

### 4. The Funnel Synthesis
Once the ideation agents finish:
1.  **The Scribe:** Initialize the State Block with the identified trade-offs
    to prevent context bloat:
    *   **Iteration:** [N]
    *   **Personas:** [Selected Experts]
    *   **Resolved:** [Addressed critiques]
    *   **Active Conflicts:** [Specific trade-offs, e.g., "Security vs.
        Performance"]
    *   **Stall Count:** [Count of non-productive iterations]
2.  **The Synthesizing Architect:** Pass the `.magi` drafts to this agent to
    synthesize into "Draft A" in the original file.

### 5. The Rumination Cycle (Expanded Review)
1.  **Blind Critique:** Push Draft A to an expanded panel of Reviewers. The
    panel MUST include the core ideators PLUS 3-6 specialized reviewer personas
    (e.g., **Test Expert**, **Readability Expert**, **Cross-Platform Expert**).
    **Prompt Template:**
    > "Role Details: Read your mandate from `[persona_file_path]`.
    > Priority: [Priority].
    > Task: Review Draft [filename].
    > Output ONLY: `Verdict: [ACCEPT/REJECT]` and `Reasoning: [Bullet points]`."
2.  **The Moderator:** If any agent rejects, pass all feedback to the Moderator
    agent to condense into a strict list of 3-5 Actionable Constraints.
3.  **The Scribe:** Pass the Moderator's constraints to the Scribe. The Scribe
    updates the State Block and checks for "flip-flopping" (e.g., Constraint 1
    violates a constraint from Round 1).
4.  **Convergence & Iteration:** Pass the updated State Block and Draft back to
    the Synthesizing Architect to generate Draft B.
5.  **Executive Tie-Breaker (Handover):** If the Scribe detects a deadlock, the
    Orchestrator MUST pause and escalate to the human with a structured
    **Deadlock Report**. Feed the human's decision back to break the tie.
6.  **CLEANUP:** Immediately execute `rm *.magi.*` once consensus is reached.

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
