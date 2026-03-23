# Chrome Remote Desktop Host Architecture

This directory contains the host-side logic for Chrome Remote Desktop. The host
is a multi-process system with platform-specific implementations and complex
Mojo IPC orchestration.

## Host Modes: Me2Me vs. It2Me

The host codebase serves two distinct feature modes:

1.  **Me2Me (Remote Access):**
    *   *Persistent:* Runs as an OS daemon/service.
    *   *Auto-Start:* Can start before a user logs in.
    *   *Architecture:* Multi-process (Daemon, Network, Desktop).
    *   *Entry Point:* `remoting/host/host_main.cc`.
2.  **It2Me (Remote Support):**
    *   *On-Demand:* Launched by the browser as a Native Messaging Host (NMH).
    *   *User-Initiated:* Requires a manual access code to connect.
    *   *Architecture:* Single-process (typically).
    *   *Entry Point:* `remoting/host/it2me/it2me_native_messaging_host_main.cc`.
    *   *More Info:* See `remoting/host/it2me/GEMINI.md`.

## Me2Me Process Types & Entry Points

Most Me2Me processes are launched via a single binary (e.g.,
`remoting_me2me_host`) using the `--type` switch. The routing logic is in
`remoting/host/host_main.cc`.

*   **`single_process_host`**
    *   *Purpose:* Default mode for Mac/Linux (official).
    *   *Entry:* `remoting/host/remoting_me2me_host.cc`
*   **`network`**
    *   *Purpose:* Handles network I/O and WebRTC.
    *   *Entry:* `remoting/host/remoting_me2me_host.cc`
*   **`daemon`**
    *   *Purpose:* Privileged process (SYSTEM/Root).
    *   *Entry:* `remoting/host/win/host_service.cc` (Win),
        `remoting/host/linux/daemon_process_main.cc` (Linux)
*   **`desktop`**
    *   *Purpose:* Captures screen and handles input.
    *   *Entry:* `remoting/host/desktop_process_main.cc`
*   **`file_chooser`**
    *   *Purpose:* Windows-specific file picker for file transfer.
    *   *Entry:* `remoting/host/file_transfer/file_chooser_main_win.cc`
*   **`rdp_desktop_session`**
    *   *Purpose:* Handles RDP-based desktop sessions on Windows.
    *   *Entry:* `remoting/host/win/chromoting_module.cc`
*   **`url_forwarder_configurator`**
    *   *Purpose:* Configures URL forwarding on Windows.
    *   *Entry:*
        `remoting/host/remote_open_url/url_forwarder_configurator_main_win.cc`
*   **`x_session_chooser`**
    *   *Purpose:* Linux-specific X11 session selection.
    *   *Entry:* `remoting/host/xsession_chooser_linux.cc`

## Navigation Guide

*   **IPC Definitions:** All Mojo interfaces for process communication are in
    `remoting/host/mojom/`.
*   **Platform Specifics:**
    *   Windows: `remoting/host/win/`
    *   Mac: `remoting/host/mac/`
    *   Linux: `remoting/host/linux/`
    *   ChromeOS: `remoting/host/chromeos/` (See `remoting/host/chromeos/GEMINI.md`)
*   **Base Utilities:** `remoting/host/base/` contains shared constants, exit
    codes, and threading helpers.
*   **Security/Policy:** `remoting/host/policy_hack/` and
    `remoting/host/it2me/` (for remote support).

## Landmines & Strict Rules

1.  **Mojo Brokerage:** The `daemon` process is typically the Mojo Broker.
    NEVER change brokerage logic in `host_main.cc` without verifying security
    impact.
2.  **Privilege Separation:** Code in `desktop` or `network` processes should
    NEVER assume it has SYSTEM/Root privileges. Use Mojo to request privileged
    operations from the `daemon`.
3.  **Platform Macros:** This directory is heavily guarded with
    `BUILDFLAG(IS_WIN)`, `BUILDFLAG(IS_APPLE)`, and `BUILDFLAG(IS_LINUX)`.
    ALWAYS check all three platforms when modifying shared files.
4.  **Installer Dependencies:** Files in `remoting/host/installer/` are
    sensitive to path changes.

## Recommended RAG Topics

Use `history_rag_query_topics` with these queries for deep dives:

*   "CRD Multi-process Architecture and Process Spawning"
*   "Mojo IPC orchestration in remoting host"
*   "Remoting Host Screen Capture and Input Injection"
*   "Remoting Host Authentication and Session Setup"
