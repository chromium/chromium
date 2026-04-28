---
name: magi-mode
description: Resolves complex or ambiguous architectural problems in
  //remoting using the MAGI multi-agent debate protocol. Use when the
  primary agent is stuck, faces conflicting platform requirements, or
  needs a high-level architectural decision.
---

# MAGI Protocol (Multi-Agent Debate)

This skill implements the MAGI multi-agent debate protocol to resolve
complex, high-stakes, or ambiguous problems. It utilizes three specialized
sub-agents to explore different dimensions of a problem simultaneously.

## The Personas

1.  **The Security Expert:** Prioritizes absolute memory safety, security,
    and logical correctness. Focuses on preventing Use-After-Free (UAF),
    bounds checking, and input sanitization.
2.  **The Performance Expert:** Prioritizes performance, efficiency, and
    low-latency. Focuses on zero-copy pathways, minimal locking, and
    resource optimization.
3.  **The Architect:** Prioritizes maintainability, idiomatic style, and
    architectural alignment. Focuses on readability, proper //base usage,
    and following Chromium conventions.

## Workflow

### 1. Preparation
- Identify the ambiguous file and the core problem.
- Ensure the workspace is clean.

### 2. Parallel Compute
Invoke three `generalist` sub-agents in parallel (`wait_for_previous:
false`). Instruct each to write its proposed solution to a temporary file.

**Prompt Template:**
> "You are [The Security Expert / The Performance Expert / The Architect].
> Your goal is to solve the following problem in [filename]: [problem
> description].
> Your priority is [Security / Performance / Maintainability].
> Write your entire proposed implementation to a new file named
> `[filename].[persona].magi.[ext]`.
> Do NOT modify the original file. Do NOT run tests. Just output the code."

### 3. Synthesis & State Summarization
Once all three sub-agents finish:
1.  Read the contents of the three `.magi` files.
2.  Identify the "dissenting opinions" and conflicting trade-offs.
3.  Synthesize the best elements into a proposed Draft solution in the
    original file.
4.  **State Management:** The Orchestrator MUST maintain a structured
    internal **State Block** to replace verbatim logs and prevent context
    bloat:
    *   **Iteration:** [N]
    *   **Resolved:** [Addressed critiques]
    *   **Active Conflicts:** [Specific trade-offs, e.g., "Security vs.
        Performance"]
    *   **Stall Count:** [Count of non-productive iterations]

### 4. The Rumination Cycle (Dynamic Routing)
1.  **Blind Critique (Star Topology):** Push the synthesized Draft back to
    the three sub-agents.
    **Prompt Template:**
    > "Role: [Persona]. Priority: [Priority].
    > Task: Review Draft [filename].
    > Output ONLY: `Verdict: [ACCEPT/REJECT]` and `Reasoning: [Bullet
    > points]`."
2.  **Convergence & Iteration:** If any agent rejects, increment Iteration
    and Stall Count (if applicable), update the State Block, iterate on
    the Draft to address the specific comments, and push it back for
    another round.
3.  **The Stall Detector (Roundtable):** If `Stall Count > 2`, switch to a
    Moderated Roundtable Topology. Provide the sub-agents with the
    **Active Conflicts** from the State Block and ask them to propose a
    negotiated compromise.
4.  **Executive Tie-Breaker (Handover):** If deadlock persists, the
    Orchestrator MUST pause and escalate to the human with a structured
    **Deadlock Report**, presenting Option A vs. Option B and requesting a
    high-level directive. Feed the human's decision back to break the tie.
5.  **CLEANUP:** Immediately execute `rm *.magi.*` once consensus is
    reached.

### 5. Specialized Modes
*   **Paranoia Mode:** For high-stakes security code (e.g., core IPC,
    network parsing), invoke MAGI using a **Multi-Model Cohort**. Run each
    specialist persona through at least two different foundation models to
    eliminate single-model blindspots.

### 6. Production Hardening Checklist (Lessons Learned)
When synthesizing the final solution, the Orchestrator MUST ensure:
1.  **Lifetime Safety:** Use `base::RefCountedDeleteOnSequence` for
    objects managing timers or sequence-bound primitives.
2.  **Zero-Copy:** Prefer `std::move`, `base::RefCountedString`, and
    `std::string_view` for large string buffers.
3.  **DoS Mitigation:** Enforce strict length limits (e.g., 64KB) on input
    and agent-generated data.
4.  **Atomic State:** Ensure callback checks (e.g., `if (callback_)`) are
    atomically sound or strictly sequence-enforced to prevent double-runs.

### 7. Validation
Run the project's standard validation suite (`git cl presubmit`,
`gn check`, and unit tests) on the merged solution.

## When to Invoke
- When an automated review finds a flaw that is hard to resolve without
  trade-offs.
- When a bug occurs only on a specific platform and the fix might impact
  others.
- When adding a new feature that has significant performance or security
  implications.