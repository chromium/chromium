# Gemini UI Layer
*Last updated: 2026*

This directory contains shared user interface (UI) components and utilities for the **Gemini (BWG)** feature on Chrome for iOS.

## Component Details

### 1. Gemini Container
*   **[gemini_container_view_controller.h](./gemini_container_view_controller.h) & [gemini_container_view_controller.mm](./gemini_container_view_controller.mm)**:
    View controller for the Gemini container bottom sheet.
*   **[gemini_container_consumer.h](./gemini_container_consumer.h)**:
    Consumer interface for updating the container UI state.
*   **[gemini_container_mutator.h](./gemini_container_mutator.h)**:
    Mutator interface for user interactions in the container.

### 2. Helpers & Utilities
*   **[gemini_ui_utils.h](./gemini_ui_utils.h) & [gemini_ui_utils.mm](./gemini_ui_utils.mm)**:
    Internal utilities for layout math, safe areas, and visual component rendering.

---

## Testing

*   **Unit Tests**:
    *   `gemini_container_view_controller_unittest.mm`
    *   `gemini_ui_utils_unittest.mm`
*   **Integration Tests (EarlGrey 2)**:
    *   **[gemini_egtest.mm](./gemini_egtest.mm)**: Comprehensive EarlGrey integration test suite.
