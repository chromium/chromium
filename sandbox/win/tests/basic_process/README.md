# Basic Process API audit shim

This directory provides `apifw.dll`, a Windows-only shim for auditing a
binary's Windows API use against the Basic Process API surface. The shim
approximates that surface by redirecting its PE imports; it does not provide a
security boundary.

The shim tests whether a binary can run using only the Basic Process API
surface defined in `basic_sandbox.def` and identifies calls outside that
surface.

## How it works

The [`redirect_imports`](../../../../build/win/redirect_imports.gni) GN
template rewrites a PE image's import descriptors to name `apifw.dll` instead
of the original Windows DLLs. The shim directly forwards allowlisted APIs.
Other APIs are routed through thunks that log or modify the calls. APIs that
must not be called use a crashing tripwire.

For an allowlisted API, `exports_logging.def` forwards the import directly. For
example, `CreateThread = kernel32.CreateThread` routes `CreateThread` back to
`KERNEL32.dll`:

```text
Normal image                    Rewritten image
------------                    ---------------
content_shell.exe               basic_process_echo_service.exe
        |                               |
        | CreateThread                  | CreateThread
        v                               v
  KERNEL32.dll                    apifw.dll
                                        |
                                        | direct allowlisted forward
                                        v
                                  KERNEL32.dll
```

For an API outside the allowlist, `exports_logging.def` routes the import
through a shim thunk. For example, `CreateFileW = ApifwCreateFileW` routes
`CreateFileW` through a thunk that logs the call. With the audit, call-site,
and log-file switches enabled, the thunk also logs the call stack to a file and
forwards the call to the original API:

```text
Normal image                    Rewritten image
------------                    ---------------
content_shell.exe               basic_process_echo_service.exe
        |                               |
        | CreateFileW                   | CreateFileW
        v                               v
  KERNEL32.dll                    apifw.dll (ApifwCreateFileW shim thunk)
                                        |
                                        | log call and stack
                                        | forward in audit mode
                                        v
                                  KERNEL32.dll
```

Key files:

*   `basic_sandbox.def` defines the Basic Process API allowlist.
*   `shim/exports_logging.def` routes allowlisted APIs to Windows DLLs and
    other APIs to shim thunks.
*   `shim/thunks_*.cc` implements logged, modified, and unsupported APIs.
*   `tools/` checks that the allowlist, exports, and thunks remain consistent.

## Command-line switches

`apifw.dll` reads these optional switches once when it is loaded.

### `--basic-process-api-test-log-file=<path>`

Appends API records to `<path>`, creating the file if needed.

Without this switch, or if the path cannot be opened, ordinary records are
still sent to `OutputDebugString`.

### `--basic-process-api-test-log-call-sites`

Logs a call stack as `module+RVA` frames, omitting frames inside `apifw.dll`.
Directly forwarded allowlisted APIs do not produce records.

### `--basic-process-api-test-audit`

Allows logged forwarding thunks to call the original Windows API. Without this
switch, a thunk that reaches the forwarding gate logs the call and crashes
instead of invoking the API. Directly forwarded allowlisted APIs and modified
thunks that do not reach the gate are unaffected.

`ApifwNotReached` tripwires always log a stack and crash, even during an audit.

## Running the test

Use a non-component, non-ASan Windows build:

```gn
is_debug = false
is_component_build = false
is_basic_api_shim = true
```

Build and run the browser test:

```text
autoninja -C out\BasicProcess content_browsertests
out\BasicProcess\content_browsertests.exe --gtest_filter=BasicProcessEchoServiceBrowserTest.AuditsEchoServiceApiCalls
```

## Generating reports

The [Basic Process deep-dive skill](skills/basic-process-deep-dive/SKILL.md)
describes how to capture audit logs, symbolize call stacks, and produce
reviewer-focused Markdown and HTML reports. It accepts existing data or a fresh
EchoService run and includes an HTML renderer. Point your coding agent at the
skill file to use this workflow.
