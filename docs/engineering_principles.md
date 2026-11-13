# Chromium Engineering Principles

The goal of this document is to help Chromium engineers make good decisions,
faster. It provides principled defaults for balancing design and implementation
trade-offs. These principles are a starting point, not a rule; when they
conflict with local `OWNERS`, we defer to them to better understand the needs
of their part of the codebase.

## Judgment & Scope

*   **Strive for pragmatism.**
    *   Assume code will live roughly as long as it has already lived. Be a bit
        more lenient when reviewing new or prototype code so long as its impact
        is localized; if the code survives, plan on making it more solid over
        time.
*   **Scrutiny scales with reach.**
    *   Spend design and review energy in proportion to how expensive a decision
        is to change later. Internal implementations and local interfaces with
        few callers are cheap to refactor, so don't over-scrutinize them.
        Widely used interfaces (and cross-team boundaries) constrain many
        callers and are costly to change—invest proportionally more care there.
*   **Avoid premature abstraction and generalization.**
    *   Design for the concrete consumers that exist today; wait for a second
        concrete consumer before extracting a shared abstraction.
    *   Abstraction adds immediate cost (layering, indirection, harder-to-read
        code) for a hypothetical future use case, and the abstraction may not
        match what the second use case actually needs. See also
        [About `//components`](../components/README.md).

## Architecture & Dependencies

*   **Keep features modular.**
    *   Tangled dependencies slow down incremental builds and make features hard
        to reason about, test, or modify in isolation.
    *   Group each feature in its own directory with a narrow public API, and
        avoid circular dependencies.
    *   Don't directly couple unrelated sibling features; coordinate through a
        higher layer or an observer/delegate interface instead.
*   **Inject dependencies.**
    *   Pass the needed objects directly into the constructor rather than
        pulling from god objects or global state, so dependencies stay
        explicit, coupling stays narrow, and unit testing is straightforward.
*   **Prefer explicit lifetimes and scoped state ownership.**
    *   Tie mutable state to an explicit owner and scope (such as a profile,
        window, or tab) rather than storing it in process-global singletons,
        so initialization order, teardown, and test isolation remain
        predictable.
*   **Reduce the number of unique configurations.**
    *   Prefer run-time flags over compile-time flags or new GN args (e.g.,
        unnecessary `BUILDFLAG(ENABLE_FOO)` conditionals)—we don't build every
        compile-time configuration, and unbuilt code rots.
    *   Prefer good defaults over permanent run-time flags, and clean up
        temporary flags once launched. Experimental flags and kill switches are
        encouraged, but flag combinations cannot all be tested and stale flags
        bloat the codebase.

## Code & Testing

*   **Code for clarity and simplicity.**
    *   Code is read far more often than it is written. Optimize for the
        engineer who opens this file in five years with a crash report and no
        context. Avoid "clever" code.
*   **Test the public interface; be careful testing internal implementation.**
    *   Testing the public interface lets another developer swap out or upgrade
        the implementation without breaking unrelated tests.
    *   Testing internal implementation makes it harder for someone external to
        tell which breaking tests matter and which are implementation details.
    *   If you really want to test internals, consider extracting that logic
        into its own testable class.
*   **Be respectful of user resources.**
    *   Chromium runs on users' own devices, often alongside many other tabs and
        apps. Do not trade user memory, battery, startup time, or bandwidth for
        implementation convenience—prefer lazy, on-demand work over eager
        initialization, background polling, or keeping unused state alive.

## See Also

*   [`//chrome/browser` design principles](chrome_browser_design_principles.md)
*   [Accountability of Authors and Reviewers](accountability.md)
*   [CL Tips](cl_tips.md)
*   [Chromium Flag Guarding Guidelines](flag_guarding_guidelines.md)
*   [C++ Style Guide](../styleguide/c++/c++.md)
