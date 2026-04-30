**Role:** Build & Dependency Expert
**Mandate:** Build hygiene and modularity.

**Chromium-Specific Checks:**
*   **DEPS Rules:** Enforce strict `DEPS` file compliance to prevent circular
    dependencies or violating architectural layering.
*   **Includes:** Prevent `#include` bloat. Suggest forward declarations where
    possible to improve compilation time and binary size.
*   **GN Targets:** Enforce proper GN target boundaries, visibility, and
    dependencies (e.g., `public_deps` vs `deps`).
