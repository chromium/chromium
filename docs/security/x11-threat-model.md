# X11 Threat Model

This document describes how X11-related security reports are triaged on Linux.

[TOC]

## Summary

X11 does not isolate clients connected to the same display. A process with an
X11 connection is therefore treated as unsandboxed. Issues that require control
of such a process, and whose impact is achieved through the X server, are not
security bugs.

As this hole cannot be remediated, Linux users should migrate to using Wayland.

## What any X11 client can do

*   **Inject input** via XTest, e.g. type commands into a terminal to execute
    arbitrary code as the user.
*   **Read input** via XInput2 raw events, including passwords typed into other
    applications.
*   **Capture the screen** via `GetImage` on the root window.
*   **Spoof UI** by placing override-redirect windows over other windows.
*   **Modify shared desktop state**, such as root window properties and
    XSETTINGS, for every client on the display.

## Processes with X11 connections

On X11, both the browser process and the GPU process hold an X11 connection.
The GPU process opens its connection before the sandbox is engaged (see
`OzonePlatformX11::InitializeGPU()`), so on X11 it is
[considered unsandboxed](process-sandboxes-by-platform.md).

Fully resolving this would require brokering the GPU process's X11 traffic
through the browser process. This is not possible in general: GPU drivers use
vendor-specific X11 extensions (e.g. for GLX and buffer presentation) whose
protocols are opaque and cannot be brokered. The only alternative would be to
load GPU drivers in the browser process, which defeats part of the purpose of
the GPU process: isolating the browser from the drivers.

## Triage guidance

Not security bugs:

*   An X11 client, such as a compromised GPU process, causing the browser
    process to load code or parse attacker-controlled data via X server state
    (e.g. XSETTINGS values like `gtk-modules` or theme names, X resources,
    window properties, selections).
*   A compromised GPU process using its X11 connection to capture the screen,
    read or inject input, or draw over browser UI.

Crashes caused by malformed X server state may still be fixed as ordinary bugs.

Still security bugs, triaged per the
[severity guidelines](severity-guidelines.md):

*   A process without an X11 connection, such as a renderer, obtaining one or
    causing another process to issue arbitrary X11 requests on its behalf.
*   Web content causing any of the above effects without a prior compromise.

This document applies only to X11. Wayland compositors isolate clients, so
Wayland issues are evaluated normally.

## Guidance for developers

Do not add mitigations against hostile X11 clients, such as sanitizing
XSETTINGS values, as security fixes. They cannot be complete, and they can
break desktop integration. For example, pinning `gtk-modules` crashed Chrome
at startup on KDE Plasma ([crbug.com/551107198](https://crbug.com/551107198)).
