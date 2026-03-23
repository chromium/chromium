# IT2Me (Remote Support) Implementation

This directory contains the host-side logic for "Remote Support" (IT2Me), which
is architecturally distinct from the persistent "Remote Access" (Me2Me) host.

## Key Differences from Me2Me

*   **Browser-Driven Lifecycle:** IT2Me is NOT a persistent service. It is a
    "Native Messaging Host" (NMH) launched by the browser on demand.
*   **Security Context:** It runs in the user's session without privileged
    access to the system.
*   **User Invitation:** Requires a human at the host to generate a 12-digit
    access code.

## Core Components

*   **Entry Point:** `remoting/host/it2me/it2me_native_messaging_host_main.cc`
*   **Host Implementation:** `remoting/host/it2me/it2me_host.cc`
    *   This class manages the WebRTC connection and session lifecycle for
        IT2Me.
*   **Native Messaging:** `remoting/host/it2me/it2me_native_messaging_host.cc`
    *   Handles the JSON-based IPC with the Chrome browser extension.
*   **Confirmation Dialogs:** `it2me_confirmation_dialog_*.cc`
    *   Platform-specific UI to ensure the user explicitly approves the
        connection.

## Landmines & Warnings

1.  **Strict Lifecycle:** When the browser kills the NMH process, the session
    ends immediately. There is no "daemon" to restart it.
2.  **Origin Validation:** `it2me_native_messaging_host_allowed_origins.h`
    defines which browser extensions are allowed to launch this host.
3.  **No Privilege Escalation:** NEVER attempt to perform privileged operations
    from IT2Me. It is designed to be sandboxed to the user's permissions.
4.  **ChromeOS Stability:** On ChromeOS, the IT2Me host runs INSIDE the Chrome
    browser binary. A crash in the host code will crash the entire browser
    session (and potentially the OS session). Extreme care must be taken with
    memory safety and stability.
5.  **JSON IPC:** Communication with the browser uses a specific JSON protocol.
    Changes here must be compatible with the client-side code (which is outside
    this repo).

## Common Queries for Gemini

Use these for RAG deep dives:
*   "IT2Me Native Messaging Host lifecycle and IPC"
*   "Remote Support (IT2Me) authentication and session setup"
*   "IT2Me confirmation dialog implementation and platform differences"
