# MAGI Persona Index

This file acts as a routing catalog for the **Recruiter** sub-agent. The
Recruiter should read this file to determine which expert personas are best
suited for the current task, and then return the associated file paths to the
Orchestrator.

## Core Ideators (The Big Three)
These are the default personas which are relevant for most Chromium tasks.
*   **The Security Expert:** Memory safety, exploit prevention, logic.
    *Path:* `src/remoting/tools/magi-mode/personas/core/security.json`
*   **The Performance Expert:** Latency, zero-copy, sequence affinity.
    *Path:* `src/remoting/tools/magi-mode/personas/core/performance.json`
*   **The Architect:** Maintainability, Chromium idioms, `//base` primitives.
    *Path:* `src/remoting/tools/magi-mode/personas/core/architect.json`

## Auxiliary Reviewers (Expanded Panel)
These personas can be swapped in during Ideation, or added during the Expanded
Review cycle.
*   **The Test Expert:** Testability, edge-cases, framework usage.
    *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/test.json`
*   **The Concurrency Expert:** `base::PostTask` safety, preventing deadlocks.
    *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/concurrency.json`
*   **The Privacy Expert:** PII prevention, UMA/UKM metrics.
    *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/privacy.json`
*   **The Build Expert:** `DEPS` compliance, `#include` bloat, GN boundaries.
    *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/build.json`
*   **The I18n & A11y Expert:** Translations and screen-reader support.
    *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/i18n.json`
*   **The Readability Expert:** Clean code, naming, cognitive complexity.
    *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/readability.json`

## Platform Specialists
Use these when modifying platform-specific implementations.
*   **Windows File API Expert:** `ScopedHandle`, file locking, security
    descriptors.
    *Path:* `src/remoting/tools/magi-mode/personas/windows/file_api.json`
*   **Linux Wayland Expert:** `wl_buffer` management, protocol integration.
    *Path:* `src/remoting/tools/magi-mode/personas/linux/wayland.json`

## Domain Specialists
Use these when modifying specific technical domains like media or networking.
*   **WebRTC Expert:** `PeerConnection`, signaling, WebRTC threading model.
    *Path:* `src/remoting/tools/magi-mode/personas/domain/webrtc.json`
*   **Codec (AV1) Expert:** `libaom` settings, bitrate adaptation, latency.
    *Path:* `src/remoting/tools/magi-mode/personas/domain/codec_av1.json`
