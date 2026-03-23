# Remoting Host Instructions

Instructions for working in `//remoting/host`.

## Architecture Overview

The `host` directory contains the logic that allows a machine to share its
screen and accept remote control from a client.

### Core Components

*   **`ChromotingHostContext`:** The central nervous system for threading in the host.
    Because CRD relies heavily on **thread affinity** (using `AutoThread`), the
    context owns and manages the dedicated threads (e.g., network, audio, video,
    input) and provides the `AutoThreadTaskRunner`s required to dispatch tasks to
    them safely.
*   **`ChromotingHost`:** The central controller of the host process. It manages
    incoming connections and ties together the networking, audio/video
    capturing, and input injection.
*   **`ClientSession`:** Represents an active connection from a single remote
    client. It handles sending video/audio and receiving input events.
*   **`DesktopEnvironment`:** The interface that provides the host with
    platform-specific mechanisms to capture screen/audio and inject input
    (e.g., mouse movements, keystrokes).
*   **`DesktopSessionProxy`:** Used in multi-process setups to proxy desktop
    operations over IPC to a dedicated desktop process.

## Key Files to Read

*   `remoting/host/chromoting_host_context.h`: Threading context and `AutoThread` management.
*   `remoting/host/chromoting_host.h`: The main entry point for the host logic.
*   `remoting/host/client_session.h`: Manages a single client connection.
*   `remoting/host/desktop_environment.h`: Interface for OS interactions.
*   `remoting/host/host_main.h` and `remoting/host/remoting_me2me_host.cc`:
    Entry points for the actual host binaries.

