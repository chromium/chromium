# Sandbox Debugging

The [macOS sandbox](../../sandbox/mac/seatbelt_sandbox_design.md) confines
Chrome processes that handle untrustworthy data or code (or both). It works by
blocking the process from accessing OS resources, which can have unintended side
effects that cause compatibility issues or crashes. This document provides
instructions for debugging such issues.

## Determining if the Sandbox is Responsible

The easiest way to test if the sandbox is causing a compatibility issue is to
**temporarily** disable it. This can be done like so:

    open -a 'Google Chrome' --args --no-sandbox

> Running with `--no-sandbox` is an insecure configuration. After performing the
> test, you should quit Chrome and re-launch normally.

If the issue still persists, then the issue is not caused by the sandbox. If the
issue no longer occurs, continue to help provide further debugging information.

## Debugging the Sandbox

If you have determined that the sandbox is responsible for the issue, the next
step is to determine what the sandbox is blocking. Sandbox violations are
written to the system log (rather than Chrome's standard output/error), so a
separate command is needed to access the data.

Start the `log` command to show sandbox violation errors:

    log stream --predicate '((processID == 0) AND (senderImagePath CONTAINS "/Sandbox")) OR (subsystem == "com.apple.sandbox.reporting")'

Then launch Chrome with with the `--enable-sandbox-logging` argument:

    open -a 'Google Chrome' --args --enable-sandbox-logging

After you reproduce the issue, quit Chrome and `Ctrl-C` the `log` command to
stop it. Copy and paste the entire output of the log command to a text file and
attach it to the bug tracker.

You can also access historical sandbox violations using the `log` command like
so:

    log show --start '2020-09-21 17:45:00' --predicate '((processID == 0) AND (senderImagePath CONTAINS "/Sandbox")) OR (subsystem == "com.apple.sandbox.reporting")'

Adjust the `--start` (and potentially add an `--end` date/time in the same
format) to limit the amount of output.

## Breaking on Sandbox Violations

In order to determine what is causing a sandbox violation, it can be helpful to
use the `send-signal` action on a sandbox rule, like so:

    (deny file-write (path "/foo/bar") (with send-signal SIGSTOP))

That will cause the process to stop until a debugger is attached or *SIGCONT* is
sent to the process.
