# WIP Actor Model

NOTE: this is a work-in-progress prototype, see [tracking bug](https://issues.chromium.org/issues/472289603).

This directory contains the core logic for the Actor Service, which executes actions on behalf of intelligence features.

## Components

*   **ActorService**: The main entry point for executing actions. It uses `ActorToolFactory` to instantiate the appropriate tool for a given action.
*   **ActorToolFactory**: Creates instances of `ActorTool` based on the action type.
*   **../tools/**: Directory containing the `ActorTool` interface and concrete implementations (e.g., `NavigateTool`).

## Implementation Details

The implementation of these tools and the service structure is based on the desktop implementation found in `chrome/browser/actor/tools/` but may differ due to platform-specific limitations.
