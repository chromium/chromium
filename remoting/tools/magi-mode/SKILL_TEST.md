# MAGI Test Protocol (SKILL_TEST.md)

This document describes the protocol used to validate the MAGI sub-agents and
prevent regressions in the protocol execution. It defines "unit" tests for each
phase of the MAGI protocol.

## Objective
To verify that sub-agents adhere to their mandates, follow the TONE MANDATE,
produce valid schema-compliant output, and generate buildable code.

## Methodology: Unit Testing by Phase
Rather than testing the entire protocol end-to-end (which is slow and prone to
flakiness), we test each phase independently by providing mock inputs and
verifying specific expected outputs.

### Test Data Isolation
To allow for real builds without risking side effects on the actual codebase,
all tests operate on **static, checked-in dummy files** located in:
`remoting/tools/magi-mode/tests/testdata/`

This directory contains its own `BUILD.gn` file to allow running real builds on
the test outputs!

---

## Test Cases Structure

Test cases are defined in `magi_phase_[N]_tests.json` conforming to
`magi_test_schemas.json`. Each test case includes:

1.  **Phase:** The MAGI phase being tested (0-10).
2.  **Name:** Descriptive name of the test.
3.  **Inputs:** Mock files or state provided to the agent.
4.  **Expected Outputs:**
    *   `files_created`: List of files that should be created.
    *   `content_patterns`: Regex patterns that must match file content.
    *   `buildable`: Boolean indicating if the output must compile.
    *   `valid_json`: Boolean indicating if the output must be valid JSON.

---

## How to Run Tests

Run a specific test file:
`python3 remoting/tools/magi-mode/run_magi_tests.py --tests \`
`  remoting/tools/magi-mode/tests/magi_phase_1_tests.json`

Run all tests using a shell loop:
`for f in remoting/tools/magi-mode/tests/magi_*_tests.json; do \`
`  python3 remoting/tools/magi-mode/run_magi_tests.py --tests "$f"; done`
