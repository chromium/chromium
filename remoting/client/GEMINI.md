# Remoting Client Instructions

Instructions for working in `//remoting/client`.

## Architecture Overview

The `client` directory contains the native client implementation for Chrome
Remote Desktop.

### Usage and Platform Constraints
*   **Target Platform:** The native client code is exclusively built for
    **ChromeOS**.
*   **Primary Consumer:** This implementation is primarily used by the **Boca**
    project which lives in `//chromeos/ash/components/boca/spotlight`.
*   **Android/iOS/Web:** No implementation exists for these platforms in
    Chromium.

### Core Components

*   **`common/`:**
    *   `RemotingClient`: The primary class coordinating signaling, transport,
        and rendering on the client side.
    *   `FrameConsumerWrapper`: Adapts `protocol::FrameConsumer` for WebRTC
        pipeline usage.
*   **`cli/`:**
    *   Contains a command-line client (`remoting_client_main`) used for testing
        and validation of the client-side stack on Linux/ChromeOS.

## Key Files to Read
*   `remoting/client/common/remoting_client.h`: The main entry point for the
    native client logic.
*   `remoting/client/common/client_status_observer.h`: Interface for monitoring
    connection states.
