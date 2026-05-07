# Gemini Model Layer
*Last updated: May 2026*

This directory contains the core business logic, data models, tab helpers, browser agents, and backend service bridges for the **Gemini (BWG)** feature integration on Chrome for iOS.

Due to the comprehensive integration of Gemini, the components are categorized into distinct logical subsystems:

---

## Subsystems & Core Components

### 1. Core State & Scope Management
*   **[gemini_browser_agent.h](./gemini_browser_agent.h) & [gemini_browser_agent.mm](./gemini_browser_agent.mm)**:
    A browser-scoped agent (`BrowserUserData`) managing the presentation of the Gemini UI overlay ("floaty"). It monitors scroll and keyboard events to temporarily hide/show the overlay, handles user preferences, and constructs the active session configuration.
*   **[gemini_tab_helper.h](./gemini_tab_helper.h) & [gemini_tab_helper.mm](./gemini_tab_helper.mm)**:
    A tab-scoped agent (`web::WebStateObserver` & `web::WebStateUserData`) that governs the Gemini context for a single tab. It monitors navigation, page loading, title settings, and favicon changes, coordinates with `PageContextWrapper` to package page data, and implements the zero-state suggestion chips pipeline.
*   **[gemini_page_context.h](./gemini_page_context.h) & [gemini_page_context.mm](./gemini_page_context.mm)**:
    An Objective-C data container wrapping C++ move-only `optimization_guide::proto::PageContext` objects. Implements custom getters with **"consume-on-read"** semantics, moving ownership of the proto context exactly once to the consumer.
*   **[gemini_configuration.h](./gemini_configuration.h) & [gemini_configuration.mm](./gemini_configuration.mm)**:
    A configuration class encapsulating settings (client ID, server ID, animations, suggestions, image attachments) required to initialize or resume a Gemini overlay session.
*   **[gemini_startup_configuration.h](./gemini_startup_configuration.h) & [gemini_startup_configuration.mm](./gemini_startup_configuration.mm)**:
    Data model representing entry-point configurations used during startup flows.

### 2. Session Management
*   **[gemini_session_handler.h](./gemini_session_handler.h) & [gemini_session_handler.mm](./gemini_session_handler.mm)**:
    Bridges callbacks from the Gemini UI SDK (`GeminiSessionDelegate`) into actual Chrome systems, mapping unique client IDs back to active `WebState`s, updating background session storage, and logging telemetry.
*   **[gemini_session_delegate.h](./gemini_session_delegate.h)**:
    Declares callback hooks for session creations, prompts, responses, feedback, cancel reasons, and settings taps.
*   **[gemini_view_state_delegate.h](./gemini_view_state_delegate.h)**:
    Internal delegate protocol transmitting view state updates.

### 3. Task Actuation (AI-Agent Automation)
*   **[gemini_actuation_handler.h](./gemini_actuation_handler.h) & [gemini_actuation_handler.mm](./gemini_actuation_handler.mm)**:
    Implements the [GeminiActuationDelegate](./gemini_actuation_delegate.h) protocol, bridging between the Gemini UI and the C++ `actor::ActorService`.
*   **Capabilities**:
    *   Creates automated browser automation tasks.
    *   Executes serialized action Protos (e.g., clicking, typing, navigating).
    *   Captures page inner-text and screenshot context across multiple controlled tabs asynchronously via `base::BarrierCallback`.

### 4. Smart Suggestions & zero-state Chips
*   **[gemini_suggestion_handler.h](./gemini_suggestion_handler.h) & [gemini_suggestion_handler.mm](./gemini_suggestion_handler.mm)**:
    Implements the [GeminiSuggestionDelegate](./gemini_suggestion_delegate.h) protocol to fetch Zero-State suggestion chips (e.g. "Summarize", "Ask about page") via the active `GeminiTabHelper` pipeline.

### 5. Camera & Link Management
*   **[gemini_camera_handler.h](./gemini_camera_handler.h) & [gemini_camera_handler.mm](./gemini_camera_handler.mm)**:
    Implements the [GeminiCameraDelegate](./gemini_camera_delegate.h) protocol. Coordinates system camera permissions and presents `UIImagePickerController` to take pictures for attachment within Gemini.
*   **[gemini_link_opening_handler.h](./gemini_link_opening_handler.h) & [gemini_link_opening_handler.mm](./gemini_link_opening_handler.mm)**:
    Implements [BWGLinkOpeningDelegate](./bwg_link_opening_delegate.h), using the `UrlLoadingBrowserAgent` to load hyperlinks clicked inside the Gemini UI in new, same, or background tabs.

### 6. Page & View State Handlers
*   **[gemini_page_state_change_handler.h](./gemini_page_state_change_handler.h) & [gemini_page_state_change_handler.mm](./gemini_page_state_change_handler.mm)**:
    Monitors page changes and coordinates user preferences/consents (such as page content sharing) with the active profile. Presents prompt alerts if consent needs to be granted.
*   **[gemini_view_state_change_handler.h](./gemini_view_state_change_handler.h) & [gemini_view_state_change_handler.mm](./gemini_view_state_change_handler.mm)**:
    Target handler listening to overlay view state transitions (e.g., expanded, collapsed).

### 7. Services & Factories
*   **[gemini_service.h](./gemini_service.h) & [gemini_service_impl.h](./gemini_service_impl.h) / [gemini_service_impl.mm](./gemini_service_impl.mm)**:
    Profile-scoped service managing profile-level Gemini eligibility checks (workspace policies, capabilities).
*   **[gemini_service_factory.h](./gemini_service_factory.h) & [gemini_service_factory.mm](./gemini_service_factory.mm)**:
    Standard profile service factory defining lifecycle and dependency injection rules for the service.
*   **[fake_gemini_service.h](./fake_gemini_service.h) & [fake_gemini_service.mm](./fake_gemini_service.mm)**:
    A test double service providing mock capabilities for unit and integration testing.

### 8. Observers & Utilities
*   **[bwg_snapshot_utils.h](./bwg_snapshot_utils.h) & [bwg_snapshot_utils.mm](./bwg_snapshot_utils.mm)**:
    Utility capturing cropped screenshots (omitting status bars and omniboxes) of active tabs to pass as image attachments to Gemini.
*   **[gemini_scroll_observer.h](./gemini_scroll_observer.h) & [gemini_scroll_observer.mm](./gemini_scroll_observer.mm)**:
    Scroll observer forwarding scroll coordinates and events.
*   **[gemini_tab_helper_observer.h](./gemini_tab_helper_observer.h)**:
    Observation protocol notifying of tab-specific context changes.

---

## Test Suites

All components in this folder are covered by specialized unit test suites in the main `ios_chrome_unittests` target:
*   `gemini_browser_agent_unittest.mm`
*   `gemini_tab_helper_unittest.mm`
*   `gemini_page_context_unittest.mm`
*   `gemini_session_handler_unittest.mm`
*   `gemini_actuation_handler_unittest.mm`
*   `gemini_camera_handler_unittest.mm`
*   `gemini_link_opening_handler_unittest.mm`
*   `gemini_page_state_change_handler_unittest.mm`
*   `gemini_suggestion_handler_unittest.mm`
*   `gemini_view_state_change_handler_unittest.mm`
*   `gemini_configuration_unittest.mm`
*   `gemini_service_factory_unittest.mm`
*   `gemini_service_impl_unittest.mm`
*   `bwg_snapshot_utils_unittest.mm`
