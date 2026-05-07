# Gemini Metrics & Telemetry Layer
*Last updated: May 2026*

This directory contains the metrics, histograms, and user actions tracking for the **Gemini (BWG)** feature integration on Chrome for iOS. It facilitates telemetry collection for onboarding consent, session engagement, AI latency, and hardware feature permissions.

## Monitored Telemetry Areas

### 1. First Run Experience (Consent & Promos)
*   **FRE Promotion Action**: Records user choices (Accept/Dismiss/Link Click) when presented with the Gemini FRE introductory promo (`RecordFREPromoAction`).
*   **FRE Consent Action**: Tracks similar user actions on the main Gemini Consent sheet (`RecordFREConsentAction`), recording link clicks for South Korea and standard flows.
*   **FRE State Tracker**: Records preference states related to whether a user has completed, bypassed, or dismissed the onboarding flow.

### 2. Prompt & Context Statistics
*   **Prompt Submission Method**: Records methods used to send queries (e.g. direct text inputs, edit menu text selections, smart suggestion chips, or omnibox prompts).
*   **Attachment Stats**: Logs image counts attached per query, as well as remix triggers and long-press inclusions.
*   **Page Context Status**: Logs if page context (annotated inner text) is attached when submitting a query.

### 3. AI Service & Network Latency
*   **Response Latency**: Logs prompt-to-response round-trip times, partitioned by whether page context was shared (`Latency.WithContext` vs `Latency.WithoutContext`) or if an image was generated (`Latency.WithGeneratedImage`).
*   **First Response Tracker**: Logs latency and success when the first query response is successfully received in a new session.

### 4. Session Duration & Engagement
*   **Session Engagement**: Tracks total queries per session and sessions that are abandoned vs successfully completed.
*   **Session Length**: Records elapsed duration between floaty presentation and dismissal, segmented by first-run status.
*   **Transition Tracking**: Logs visual transitions (e.g., collapsed to expanded, hidden to shown) and minimize/maximize durations.

### 5. Camera Permission Workflow
*   **OS Camera Permissions**: Telemetry tracking initial authorization statuses (Authorized, Denied, NotDetermined) and the final consent result.
*   **Permissions Alert**: Records button taps on the Custom Settings prompt if permissions are denied, and the camera picker results (completed with/without image or canceled).

---

## File Index

| File | Description |
| :--- | :--- |
| `gemini_metrics.h / .mm` | Public API declaring UMA histogram keys, metric enums, and recording functions. |
| `gemini_metrics_unittest.mm` | Unit tests asserting correct histogram population, bucketing, and mock session durations. |
| `BUILD.gn` | GN build targets defining metrics dependencies. |
