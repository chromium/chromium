# ChromeOS Host Implementation Details

This directory contains the host-side logic for Chrome Remote Desktop on
ChromeOS. Unlike other platforms, the host on ChromeOS is tightly integrated
into the `chrome` binary and the Ash window manager.

## Core Architectural Difference

**ChromeOS does NOT support a traditional Me2Me (Remote Access) daemon.**
Everything on ChromeOS is built on top of the IT2Me (Remote Support) host
architecture.

### Connection Scenarios

1.  **Consumer Remote Support:** The standard scenario, initiated via the CRD
    website (remotedesktop.google.com) running in the Chrome browser.
2.  **Commercial (Enterprise):** Initiated via a remote command when a session
    is requested in the Admin Console (admin.google.com). This supports both:
    *   *Attended* (Remote Support style)
    *   *Curtained* (Remote Access style, but still utilizes the IT2Me host)
3.  **Education (BOCA):** Initiated via a remote command when a session is
    requested in the Class Tools app.

## Key Components

*   **Remoting Service:**
    *   `remoting/host/chromeos/remoting_service.cc`
    *   This is the central service that manages remote desktop connections
        on ChromeOS.
*   **Remote Support (Ash):**
    *   `remoting/host/chromeos/remote_support_host_ash.cc`
    *   Implements host logic specifically for the Ash environment.
*   **Aura Integration:**
    *   `mouse_cursor_monitor_aura.cc`, `clipboard_aura.cc`
    *   Uses Aura primitives for capturing input and handling the clipboard.
*   **Video Capture:**
    *   `ash_mojom_video_consumer.cc` and `frame_sink_desktop_capturer.cc`
    *   Integrates with the Ash video consumer and frame sink interfaces to
        capture the screen.
*   **Session Storage:**
    *   `file_session_storage.cc`
    *   Handles persistent storage for remoting sessions on ChromeOS.

## Landmines & Warnings

1.  **Stability is Critical:** On ChromeOS, the host runs within the `chrome`
    process. A crash in host code crashes the entire user session. Extreme
    caution is required when modifying memory-unsafe code.
2.  **Ash/Aura Dependency:** Much of this code depends on Ash or Aura APIs.
    Changes to these subsystems must be coordinated with the ChromeOS UI team.
3.  **Enterprise Policy:** ChromeOS has strict enterprise policy integration
    (`chromeos_enterprise_params.cc`). Ensure any changes respect these
    policies.
4.  **No Multi-Process (Yet):** While other platforms are multi-process, the
    ChromeOS host is currently single-process (inside the browser). Do NOT
    assume you can use multi-process Mojo orchestration as on Windows/Linux.

## Troubleshooting Guidance

If a connection fails on ChromeOS, check:

*   `chrome://system` logs for `chrome` process crashes.
*   Mojo interface connection status for Ash-specific services.
*   Enterprise policy settings if the connection is for a managed device.
