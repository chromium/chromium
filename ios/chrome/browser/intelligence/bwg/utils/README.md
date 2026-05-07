# Gemini Utilities Layer
*Last updated: May 2026*

This directory contains shareable utilities, constants, preference stores, and feature eligibility checks for the **Gemini (BWG)** integration on Chrome for iOS.

---

## File Index & Descriptions

### 1. Core Enums & Constants
*   **[gemini_constants.h](./gemini_constants.h) & [gemini_constants.mm](./gemini_constants.mm)**:
    Defines shared enums and string keys used across the module:
    *   `EntryPoint`: Enumerates entry points to Gemini (AI Hub, Omnibox Summarize, Context Menu text selections, Onboarding chips).
    *   `InputType`: Tracks different submission formats (text, summarize page, related sites, suggested replies).
    *   `FloatyUpdateSource`: Triggers for hiding/showing the floating bubble (scroll event, keyboard appearance, page navigation).
    *   `FREState`: Represents onboarding progress (ConsentGranted, PromoDismissed).
    *   SF Symbol names, URL links (privacy notices, South Korea local terms), and UMA/UKM metric key prefixes.

### 2. Startup Results
*   **[gemini_entry_flow_result.h](./gemini_entry_flow_result.h)**:
    Declares the `GeminiEntryFlowResult` enum representing outcomes of starting the Gemini flow (Success, Cancelled, AccountIneligibleByEnterprise, AccountIneligibleByGemini, or Unknown).

### 3. Conditional Feature Eligibility
*   **[gemini_feature_availability.h](./gemini_feature_availability.h) & [gemini_feature_availability.mm](./gemini_feature_availability.mm)**:
    Provides logic to verify whether sub-features (such as `kImageRemix` / Image Editing) are available for a specific user profile, identity manager, or AccountInfo capability set, providing control beyond simple static Feature Flags.
*   **`gemini_feature_availability_unittest.mm`**:
    Unit tests validating feature availability under different capability configurations.

### 4. User Preferences & Policies
*   **[gemini_prefs.h](./gemini_prefs.h) & [gemini_prefs.mm](./gemini_prefs.mm)**:
    Encapsulates calls to `PrefService` to manage states:
    *   Enterprise policies governing GenAI and Gemini enablement.
    *   Onboarding consent states and tracking whether a user has already seen the Promo.
    *   Resetting consent preferences.
