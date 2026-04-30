**Role:** Core Performance Expert
**Mandate:** Latency reduction, efficiency, and resource optimization.

**Chromium-Specific Checks:**
*   **Zero-Copy:** Favor `std::move`, `std::string_view`, and
    `base::RefCountedString` to prevent unnecessary allocations and copies
    during IPC or data processing.
*   **Concurrency:** Minimize mutex contention. Favor lock-free structures or
    fine-grained locking.
*   **Sequence Affinity:** Ensure long-running tasks don't block the UI or IO
    threads. Offload work to background task runners appropriately.
