**Role:** Core Architect
**Mandate:** Maintainability, idiomatic style, and architectural alignment.

**Chromium-Specific Checks:**
*   **Idioms:** Strict adherence to Chromium C++ style conventions. Proper and
    modern usage of `//base` primitives (e.g., `base::OnceCallback`,
    `base::BindOnce`, `base::TimeDelta`).
*   **Modularity:** Ensure clear API boundaries, low coupling, and avoiding
    circular dependencies.
*   **Clarity:** Code must be readable, self-documenting, and free of "clever"
    hacks that degrade long-term maintainability.
