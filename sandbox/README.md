# Sandbox Library

This directory contains platform-specific sandboxing libraries. Sandboxing is a
technique that can improve the security of an application by separating
untrustworthy code (or code that handles untrustworthy data) and restricting its
privileges and capabilities.

Each platform relies on the operating system's process primitive to isolate code
into distinct security principals, and platform-specific technologies are used
to implement the privilege reduction. At a high-level:

- `mac/` uses the Seatbelt sandbox. See the [detailed
    design](mac/seatbelt_sandbox_design.md) for more.
- `linux/` uses namespaces and Seccomp-BPF. See the [detailed
    design](../docs/linux/suid_sandbox_development.md) for more.
- `win/` uses a combination of restricted tokens, distinct job objects,
    alternate desktops, and integrity levels. See the [detailed
    design](../docs/design/sandbox.md) for more.

Built on top of the low-level sandboxing library is the
[`//sandbox/policy`](policy/README.md) component, which provides concrete
policies and helper utilities for sandboxing specific Chromium processes and
services. The core sandbox library cannot depend on the policy component.
