<!--
Copyright 2026 The Chromium Authors
Use of this source code is governed by a BSD-style license that can be
found in the LICENSE file.
-->

# MAGI Skill Test Plan

This document outlines the strategy and methodology for testing the MAGI
protocol implementation in the agent framework.

## Methodology

To ensure the protocol is both correct (logic) and effective (agent
capability) while keeping testing costs and execution time reasonable, we
use a layered approach:

### 1. Mock Framework for Protocol Logic (End-to-End)
To test the orchestration, state transitions, and consolidation logic
(Phase 5) without the high cost and latency of LLM calls:
*   **Approach**: Use a mock harness in the test runner that injects
    predefined JSON responses simulating agent outputs (reviews, synthesis
    drafts).
*   **Goal**: Verify that the harness correctly moves between phases,
    enforces gates, and consolidates data correctly.
*   **Hard Gates Enforcement**: The mock framework will be used to verify
    that the harness refuses to proceed if mandatory steps (like building
    and testing) are not reported in the trace.

### 2. Agent-Based Testing with Flawed Files
To test the actual capability of the expert personas to find real issues:
*   **Approach**: Run real agent calls on a curated set of files containing
    intentional flaws.
*   **Goal**: Verify that the agent's expertise is sufficient to identify
    the flaws.
*   **Evaluation**: Use semantic checks or an "LLM-as-a-Judge" to evaluate
    the verdict and comments, rather than rigid line-number or string
    matching, to account for LLM non-determinism.

## Test Scenarios

We have developed a set of test files in `tests/testdata/` representing
common flaws to be used in both mocked and real agent tests:

*   **Use-After-Free**: Source `complex_uaf.cc.magi.test`, target
    `bind_post_task_helper.cc`. Expected to detect `base::Unretained` usage.
*   **Deadlock**: Source `unsafe_threading.cc.magi.test`, target
    `thread_safe_manager.cc`. Expected to detect reverse lock acquisition.
*   **Unsafe Cast**: Source `unsafe_casting.cc.magi.test`, target
    `type_converter.cc`. Expected to detect `reinterpret_cast` usage.
*   **Win Handle Leak**: Source `win_handle_leak.cc.magi.test`, target
    `file_manager_win.cc`. Expected to detect missing `CloseHandle`.
*   **Mac Retain Cycle**: Source `mac_retain_cycle.mm.magi.test`, target
    `notification_delegate.mm`. Expected to detect strong `self` capture in
    block.
*   **Linux FD Leak**: Source `linux_fd_leak.cc.magi.test`, target
    `socket_handler_linux.cc`. Expected to detect missing `close()` on early
    return.
*   **Flawed Test**: Source `tautological_assert_test.cc.magi.test`, target
    `mock_helper_unittest.cc`. Expected to detect trivial assert (`true==true`).

## Success Criteria

*   **Protocol Tests**: 100% pass rate on mocked protocol logic scenarios.
*   **Agent Capability Tests**: High recall on detecting the intentional
    flaws in the test dataset without excessive false positives.
*   **Safety**: Verification that the build and test steps are never
    skipped during a synthesis flow.

## Reporting

To ensure transparency and verify that no critical steps (such as building and
testing) are skipped during execution, the testing agent MUST generate a
structured **Test Execution Report** artifact after running a suite of tests.

The report must include:
*   **Summary**: Total cases executed, passed, failed, and skipped.
*   **Detailed Results Table**:
    *   **Phase**: The protocol phase tested.
    *   **Test Case Name**: Descriptive name.
    *   **Result**: PASS/FAIL.
    *   **Build Executed**: Yes/No/NA (Crucial for verifying safety).
    *   **Tests Run**: Yes/No/NA (Crucial for verifying safety).
    *   **Consensus Reached**: Yes/No/NA (For Phase 5).
