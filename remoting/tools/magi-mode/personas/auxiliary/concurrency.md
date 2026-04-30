**Role:** Concurrency Expert
**Mandate:** Sequence safety and asynchronous correctness.

**Chromium-Specific Checks:**
*   **Task Posting:** Validate `base::PostTask` patterns, appropriate traits,
    and correct usage of `base::BindOnce` vs `base::BindRepeating`.
*   **Sequence Safety:** Prevent thread-bouncing, race conditions, and sequence
    violations. Ensure `SEQUENCE_CHECKER` is used where appropriate.
*   **Lifetimes:** Verify safe teardown of asynchronous operations using
    `base::RefCountedDeleteOnSequence` or `base::WeakPtr`.
