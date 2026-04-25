---
name: magi-mode
description: Resolves complex or ambiguous architectural problems in //remoting
  using the MAGI multi-agent debate protocol. Use when the primary agent is
  stuck, faces conflicting platform requirements, or needs a high-level
  architectural decision.
---

# MAGI Protocol (Multi-Agent Debate)

This skill implements the MAGI multi-agent debate protocol to resolve complex,
high-stakes, or ambiguous problems. It utilizes three specialized sub-agents to
explore different dimensions of a problem simultaneously.

## The Personas

1.  **The Security Expert:** Prioritizes absolute memory safety, security,
    and logical correctness. Focuses on preventing Use-After-Free (UAF), bounds
    checking, and input sanitization.
2.  **The Performance Expert:** Prioritizes performance, efficiency, and
    low-latency. Focuses on zero-copy pathways, minimal locking, and resource
    optimization.
3.  **The Architect:** Prioritizes maintainability, idiomatic style, and
    architectural alignment. Focuses on readability, proper //base usage, and
    following Chromium conventions.

## Workflow

### 1. Preparation
- Identify the ambiguous file and the core problem.
- Ensure the workspace is clean.

### 2. Parallel Compute
Invoke three `generalist` sub-agents in parallel (`wait_for_previous: false`).
Instruct each to write its proposed solution to a temporary file.

**Prompt Template:**
> "You are [The Security Expert / The Performance Expert / The Architect]. Your
> goal is to solve the following problem in [filename]: [problem description].
> Your priority is [Security / Performance / Maintainability].
> Write your entire proposed implementation to a new file named
> `[filename].[persona].magi.[ext]`.
> Do NOT modify the original file. Do NOT run tests. Just output the code."

### 3. Synthesis & Voting
Once all three sub-agents finish:
1.  Read the contents of the three `.magi` files.
2.  Identify the "dissenting opinions" and conflicting trade-offs between the
    three approaches.
3.  Synthesize the best elements of all three into the final solution in the
    original file.
4.  **CLEANUP:** Immediately delete all temporary `.magi` files using
    `run_shell_command`.

### 4. Validation
Run the project's standard validation suite (`git cl presubmit`, `gn check`,
and unit tests) on the merged solution.

## When to Invoke
- When an automated review finds a flaw that is hard to resolve without
  trade-offs.
- When a bug occurs only on a specific platform and the fix might impact others.
- When adding a new feature that has significant performance or security
  implications.