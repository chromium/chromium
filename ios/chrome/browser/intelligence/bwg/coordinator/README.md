# Gemini Coordinator Layer
*Last updated: 2026*

This directory contains components responsible for navigating, presenting, and orchestrating user flows for the **Gemini (BWG)** feature integration on iOS.

Following the strict Coordinator-Mediator separation pattern of Chrome for iOS, coordinators in this directory are responsible solely for view presentation, UI transitions, and scene handling, while delegating business logic decisions to their respective mediators.

## Components & Flow

### 1. Entry Flow Coordination
*   **[gemini_entry_flow_coordinator.h](./gemini_entry_flow_coordinator.h) & [gemini_entry_flow_coordinator.mm](./gemini_entry_flow_coordinator.mm)**:
    Manages the full startup and entry flow sequence for Gemini. It determines whether the user needs to sign in (presenting `SigninCoordinator` if unauthenticated), performs profile and enterprise eligibility checks via the `GeminiService`, and presents the `AccountMenuCoordinator` to allow account switching if Gemini is disabled/restricted for their active Google account.

### 2. Container Coordination
*   **[gemini_container_coordinator.h](./gemini_container_coordinator.h) & [gemini_container_coordinator.mm](./gemini_container_coordinator.mm)**:
    Coordinates presentation of the Gemini container bottom sheet.

---

## File Index

| File | Description |
| :--- | :--- |
| `gemini_entry_flow_coordinator.h / .mm` | Orchestrates authentication and eligibility checks on startup. |
| `gemini_container_coordinator.h / .mm` | Manages presentation and dismissal of the Gemini container. |
| `gemini_container_mediator.h / .mm` | Handles container mediator event handling and state updates. |
| `BUILD.gn` | GN build file defining dependency targets for the coordinator module. |
| `DEPS` | Directory-specific dependency rules for coordinator sources. |
