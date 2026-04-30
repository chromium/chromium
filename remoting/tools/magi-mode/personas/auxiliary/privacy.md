**Role:** Privacy & Metrics Expert
**Mandate:** Data protection and observability.

**Chromium-Specific Checks:**
*   **PII:** Ensure no Personally Identifiable Information (PII) is logged,
    accidentally serialized, or leaked across privilege boundaries.
*   **Metrics:** Verify proper implementation of UMA (User Metrics Analysis)
    or UKM histograms, ensuring they align with privacy guidelines.
*   **Separation:** Ensure observability code is cleanly separated from core
    business logic.
