# MAGI Mode (Multi-Agent Guided Iteration)

Welcome to **MAGI Mode**, an advanced multi-agent protocol designed to tackle
complex, high-stakes, or highly ambiguous architectural problems in large
codebases.

When you encounter a problem where standard AI agents get stuck, face
conflicting platform requirements, or need to make critical security and
performance trade-offs, MAGI Mode brings an entire virtual engineering team
to your local workspace.

## What is MAGI?

Unlike a standard chat interface where a single AI tries to do everything
(often losing context or hallucinating), MAGI utilizes a **"Consensus Loop"**
of highly specialized sub-agents.

Think of it like a real-world software engineering pipeline:
1.  A **Scoping Lead** investigates the bug and writes a strict spec.
2.  An **Architect** builds the scaffolding and interfaces.
3.  A **Test Expert** sets up Test-Driven Development (TDD) boundaries that
    intentionally fail.
4.  An **Engineering Manager** dynamically recruits specialized **Domain
    Experts** (e.g., Security, Performance, Platform) to implement the code
    in parallel.
5.  A **Synthesizing Architect** merges the parallel work.
6.  A panel of **Reviewers** blindly audits the work against strict checklists.
7.  A **Technical Program Manager (TPM)** consolidates the feedback into
    actionable constraints.
8.  A **Release Engineer** cleans up the workspace and uploads the changes.

## Why use MAGI?

*   **Context Efficiency:** By isolating agents to specific tasks and
    communicating through strict JSON contracts on disk, MAGI prevents the
    "context bloat" that causes standard AIs to forget instructions.
*   **Specialized Expertise:** Instead of one generalist, you get focused
    experts (e.g., "Windows C++ Security Expert") who evaluate code against
    strict, domain-specific checklists.
*   **Built-in TDD:** Code isn't written until failing tests are established
    and verified to fail.
*   **Self-Improving:** If the team misses a bug, a **Trainer** agent analyzes
    the failure and permanently upgrades the relevant expert's checklist for
    future runs.
*   **Deterministic Verification:** Progress is measured by a strict Boolean
    state machine. Code doesn't ship until all experts flip their checklist
    items to `true`.

## How to Invoke

When interacting with the Gemini CLI, simply request:

> "I have a complex IPC issue in the Windows host that's causing deadlocks.
> Please invoke the magi-mode skill to investigate and fix it."

The Orchestrator will handle the rest, keeping you informed at every major
milestone.

## Directory Structure Overview

*   `personas/`: The catalog of specialized expert definitions (Core, Domain,
    Auxiliary, Languages, OS).
*   `magi_schema.json`: The strict JSON data contracts that agents use to
    communicate.
*   `SKILL.md`: The core execution logic and protocol rules.
*   `.temp/`: A transient directory used by agents to store drafts and reviews
    without dirtying your git tree.

## Testing & Verification

To ensure the protocol's logic and the experts' capabilities remain reliable,
MAGI includes a comprehensive testing suite:

### Test Infrastructure
*   `SKILL_TEST_PLAN.md`: The high-level strategy for testing the protocol,
    including the use of mock harnesses and "flawed file" detection tests.
*   `SKILL_TEST.md`: Contains the specific unit tests and verification logic
    for the MAGI state machine.
*   `run_magi_tests.py`: The primary script to execute the MAGI test suite.
*   `PRESUBMIT.py` & `PRESUBMIT_test.py`: Ensure that any changes to MAGI
    files (schemas, personas) adhere to the protocol's strict standards before
    they are committed.

### Test Data & Scenarios
The `tests/` directory contains structured data used to validate each stage of
the protocol:
*   `tests/magi_stage_specify_tests.json`: Scenarios for requirement gathering
    and scoping.
*   `tests/magi_stage_generate_tests.json`: Scenarios for scaffolding, TDD, and
    implementation.
*   `tests/magi_stage_refine_tests.json`: Scenarios for review, consolidation,
    and the consensus loop.
*   `tests/testdata/`: A collection of files with intentional flaws (e.g.,
    Use-After-Free, Deadlocks, Memory Leaks) used to verify that the Domain
    Experts can accurately detect real-world issues.
