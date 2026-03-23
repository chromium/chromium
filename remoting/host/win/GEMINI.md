# Windows Host Implementation Details

This directory contains Windows-specific implementations for the CRD host. It
is characterized by deep integration with Windows services, COM, and WTS
(Windows Terminal Services).

## Key Components

*   **Daemon Process (Windows Service):** `remoting/host/win/host_service.cc`
    *   This is the entry point when running as a Windows service.
    *   It manages the lifecycle of the `Network` and `Desktop` processes.
*   **RDP Integration:** `remoting/host/win/rdp_desktop_session.cc` and
    `rdp_client.cc`
    *   CRD on Windows uses a loopback RDP connection to create the user session
        for 'curtained' (private) remote access.
*   **Privilege Transitions:** `remoting/host/win/launch_process_with_token.cc`
    *   Critical for launching user-level processes (like `Network` or
        `Desktop`) from the SYSTEM-level `Daemon`.
*   **WTS Management:** `remoting/host/win/wts_session_process_delegate.cc`
    *   Handles interaction with Windows Terminal Services for session
        detection and process attachment.
*   **Security & ACLs:** `remoting/host/win/security_descriptor.cc` and
    `com_security.cc`
    *   Manages SDDLs and ACLs for objects and COM interfaces to ensure secure
        IPC.

## Landmines & Warnings

1.  **COM Security:** Many interfaces rely on `CoInitializeSecurity`. DO NOT
    change COM security settings without expert review.
2.  **Session 0 Isolation:** The `Daemon` process runs in Session 0. It cannot
    interact with the UI directly. The same is true for the `Network` process
    which is also sandboxed (low-integrity) and runs as a low privilege user.
3.  **WTS Session Tracking:** WTS session IDs can change during a connection
    (e.g., when a user logs in). Use `WtsSessionChangeObserver` to track these
    changes.
4.  **Resource Scripts:** `core.rc.jinja2` is used to generate the resource file
    for the host. Ensure string IDs and resource mappings are consistent.

## Troubleshooting Guidance

If a process fails to launch on Windows, check:

*   `launch_process_with_token.cc` logs for token creation failures.
*   Windows Event Logger (`windows_event_logger.cc`) for service-level errors.
*   ACL settings on the Mojo pipe or named objects.
