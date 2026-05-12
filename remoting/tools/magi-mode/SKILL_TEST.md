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

#### Rules for Flawed Test Files
To prevent automated tooling (like static analyzers) from flagging intentional
flaws in test data, and to prevent humans from trying to fix them:
1.  **Extension:** Files containing intentional flaws MUST use the extension
    `.magi.test` (e.g., `complex_uaf.cc.magi.test`).
2.  **Descriptive Pre-Copy Naming:** The source file in the repository SHOULD
    have a name that indicates the flaw (e.g., `complex_uaf.cc.magi.test`) to
    make it clear to humans what is being tested.
3.  **Realistic Copied Naming:** The build system MUST rename the file to a
    normal, realistic name when copying it and stripping comments (e.g.,
    `bind_post_task_helper.cc`) to prevent the agent from anchoring on the
    filename.
4.  **Realistic Code:** Class names, function names, and non-MAGI
    comments in the file MUST be structured as if the code was valid
    and expected to work.
5.  **Annotations:** Annotate the files with `// MAGI: <comment>` to describe
    the flaw or reproduction steps for humans.
6.  **Preprocessing:** The `BUILD.gn` file MUST contain a GN `action` to copy
    these files, rename them, and strip out the `// MAGI:` comments.
7.  **Test Only:** All test targets in `BUILD.gn` MUST be marked
    `testonly = true`.
8.  **Line Numbers:** Line numbers in test cases are OPTIONAL and should
    generally be omitted to avoid brittleness caused by line shifts when
    comments are stripped. The primary verification should be based on file
    paths and content patterns rather than exact line numbers.

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

## Manual Test Execution via Agent

If the automated test runner fails or hangs (e.g., due to environment issues
with `agentapi`), an agent can manually execute the tests by interpreting the
JSON files:

1.  **Read the Test JSON**: Locate the test case you want to run.
2.  **Extract Prompt and Inputs**: Construct a clear prompt for a subagent,
    explaining the role (e.g., Supervisor) and providing the `base_inputs`
    and `override_inputs` from the test case.
3.  **Invoke Subagent**: Use the `invoke_subagent` tool with the constructed
    prompt.
4.  **Verify Output**: When the subagent responds, verify that its output
    matches the `expected_outputs` in the test case (e.g., valid JSON,
    specific content patterns).

This ensures that tests can still be run even if the automation script is not
fully functional in the current environment.

## See Also

*   [SKILL_TEST_PLAN.md](SKILL_TEST_PLAN.md) for the overall testing strategy and methodology.
