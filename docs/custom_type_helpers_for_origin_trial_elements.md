# Custom Type Helpers for Origin-Trial-Controlled HTML Elements

When adding a new HTML element gated behind a runtime-enabled feature (e.g., an
origin trial), the default generated V8 type bindings will not work. This
document explains why custom type helpers are needed and how to add them.

## The Problem

[CL 1994520](https://crrev.com/c/1994520) changed the generated type bindings
so that they use the `RuntimeEnabledFeatures::FooEnabled()` overload that does
**not** accept a `Document` or `ExecutionContext` parameter. This causes a
compiler error for OT-controlled elements, because their feature flag requires
context to evaluate.

Without custom bindings, an `HTMLUnknownElement` constructed with an arbitrary
tag name could masquerade as the real element type through generated
`To<HTMLFooElement>()` casts. This is a C++ type confusion issue and is treated
as a presumptive security bug.

## The Fix

OT-controlled elements must do two things:

1. **Set `noTypeHelpers: true`** in the element's IDL file. This suppresses the
   default generated type helpers that would cause the compiler error.

2. **Add custom type bindings** that check the runtime feature using the
   context-aware overload (e.g., `RuntimeEnabledFeatures::FooEnabled(document.GetExecutionContext())`).

## Examples of Elements Using This Pattern

- `HTMLFencedFrameElement` — gated on `FencedFrames`
- `HTMLUserMediaElement` — gated on `UserMediaElement`
- `HTMLInstallElement` — gated on `InstallElement`

## Adoption Across Documents

If an OT-enabled element is created in an OT-enabled document and then adopted
into an OT-disabled document, the C++ object retains its real type — no DOM
object can change its C++ type. The element should be prepared to handle this
gracefully (it doesn't need to work well, but it must not be a security bug).
For example, feature-specific functionality may silently no-op or return default
values, but the element must not expose privileged data or allow bypassing
security checks in the new context.

## Historical Context

This pattern originated with `HTMLPortalElement`, and was subsequently copied
to `HTMLFencedFrameElement` ([CL 2873160](https://crrev.com/c/2873160)). The
custom bindings requirement was clarified in
[CL 1994520](https://crrev.com/c/1994520), which made it a compiler error to
use the default type helpers for context-dependent features. See
[crbug.com/495852853](https://crbug.com/495852853) for the full discussion.
