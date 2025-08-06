# Debugging Chromium

This document provides a high-level overview of debugging the Chromium browser
and its child processes. For more detailed and platform-specific instructions,
please see the links at the end of this guide.

## Getting Started: Attaching to Processes

Chromium is a multi-process application. The first step in debugging is often
identifying and attaching to the correct process.

*   **Browser Process:** This is the main process. You can start it directly
    in a debugger (e.g., `gdb --args out/Default/chrome`).
*   **Renderer, GPU, and Utility Processes:** These are child processes launched
    by the browser. To debug them from startup, you need to use special
    command-line flags.

### Useful Command-Line Flags for Debugging

Pass these to the `chrome` executable to alter its behavior for debugging.

*   `--wait-for-debugger`
    *   Pauses the child process (renderer, GPU, etc.) on startup until a
        debugger is attached. This is the most reliable way to debug child
        process initialization.
*   `--renderer-cmd-prefix='xterm -e gdb --args'`
    *   Launches new renderer processes inside an `xterm` with `gdb` attached.
        This is a common and useful debugging flag on Linux.
*   `--utility-cmd-prefix='xterm -e gdb --args'`
    *   Same as above, but for utility processes.
*   `--no-sandbox`
    *   Disables the security sandbox in all processes. This can make it easier
        to attach a debugger, but be aware of the security implications.
*   `--single-process`
    *   Runs the renderer and browser in the same process. **Note:** This is not
        a realistic representation of how Chrome runs and can mask or create
        bugs that don't exist in multi-process mode. Use with caution.
*   `--enable-logging --v=1`
    *   Enables verbose logging, which can be very helpful for understanding
        what's happening. Increase the `v` value for more verbosity.

### Finding the Right Process

When Chrome is already running, you can use its built-in Task Manager to find
the process you want to debug.
*   **Open Task Manager:** `Shift+Esc` (or `More Tools > Task Manager` in
    the menu).
*   **Get Process ID (PID):** Right-click on the table header and enable the
    "Process ID" column. You can then use this PID to attach your debugger
    (`gdb -p <pid>`).

## Platform-Specific Guides

For detailed instructions on setting up and using a debugger on your specific
platform, please refer to the following guides:

*   **[Debugging on Linux](linux/debugging.md)**
*   **[Debugging on Windows](windows/debugging.md)**
*   **[Debugging on macOS](mac/debugging.md)**
*   **[Debugging on ChromeOS](chromeos/debugging.md)**
*   **[Debugging on Android](android/debugging.md)**
*   **[Debugging on Fuchsia](fuchsia/debug_instructions.md)**

## Other Useful Debugging Resources

*   **[Debugging GPU-related code](gpu/debugging_gpu_related_code.md)**
*   **[Debugging memory issues](memory/debugging_memory_issues.md)**
*   **[Debugging with crash keys](debugging_with_crash_keys.md)**
