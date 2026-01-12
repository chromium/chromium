# WIP Actuation Model

NOTE: this is a work-in-progress prototype, see [tracking bug](https://issues.chromium.org/issues/472289603).

This directory contains the core logic for the Actuation Service, which executes actions on behalf of intelligence features.

## Components

*   **ActuationService**: The main entry point for executing actions. It uses `ActuationToolFactory` to instantiate the appropriate tool for a given action.
*   **ActuationToolFactory**: Creates instances of `ActuationTool` based on the action type.
*   **tools/**: Directory containing the `ActuationTool` interface and concrete implementations (e.g., `NavigateTool`).

## Implementation Details

The implementation of these tools and the service structure is based on the desktop implementation found in `chrome/browser/actor/tools/` but may differ due to platform-specific limitations.
