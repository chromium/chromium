# Chromium `SECURITY.md` Guidelines

This document explains how to write an effective SECURITY.md for components in Chromium. A SECURITY.md lives in your component's directory and documents its security boundaries, process model, threat assumptions, and explicit non-bugs for reviewers, maintainers, security researchers, and AI tools.

## Authors

* Aaron Selya (selya@google.com)

## Participate

* CLs are welcome!

## Why Write a `SECURITY.md` in Chromium?

A `SECURITY.md` establishes a clear security contract for your component. Without one, security researchers and automated scanning tools may file bugs on intentional design decisions, safe-context crashes, or intended error recovery, wasting your team's engineering time on triage.

A well-crafted Chromium `SECURITY.md` answers four basic questions:

**1. Where** does the code run (process type, sandbox level, privilege tier)?

**2. What** inputs are trusted vs. untrusted (and does it defend against a compromised renderer)?

**3. Which** vulnerability classes and behaviors are in scope vs. explicit non-bugs?

**4. Why** were specific design trade-offs made, and what risks are accepted?

## Checklist for Chromium Component Authors

Check off these items before committing your `SECURITY.md`:

- [ ] Location: Place it in your component's root directory (../my_component/SECURITY.md).
- [ ] Scope: State which directory paths are covered and which are excluded (e.g., testing/, test/).
- [ ] Process & Sandbox: Name the process type (Browser, Renderer etc.) and sandbox constraints.
- [ ] Compromised Renderer Assumption: State whether your code defends against an attacker who has compromised a renderer process.
- [ ] Rule of Two: Explain how this component complies with Chromium's Rule of Two
- [ ] Mojo IPC & Origin Validation: Document argument validation, origin checks (ChildProcessSecurityPolicy), and ReportBadMessage() behavior.
- [ ] Component-Specific Non-Bugs: Write concrete, testable out-of-scope statements
- [ ] Accepted Risks: Document architectural trade-offs (e.g., shared memory IPC) and mitigations.
- [ ] Reporting: Direct reporters (including agents) to review [security-for-agents.md](https://source.chromium.org/chromium/chromium/src/+/main:docs/security/security-for-agents.md) to double check any findings relevant before submitting a bug.

## Chromium's Security Architecture

Chromium's multi-process security model and bug taxonomy are canonically defined in central documentation:

* [Security for Agents](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-for-agents.md): Canonical guide on process boundaries, bug classification, evidence requirements, and universal non-bugs.
* [Chrome Security Rules](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rules.md): Core architectural rules, including the Rule of Two and IPC boundary constraints.
* [Chrome Security FAQ](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/faq.md): High-level security architecture, threat model, and isolation principles.
* [Severity Guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md): Vulnerability severity ratings and impact assessment.

When authoring a component `SECURITY.md`, you do not need to re-explain Chromium's global security model. Instead, specify how your component integrates with these architectural boundaries:

### Process Type and Sandbox Level

Identify the execution context of your component:

* **Browser Process:** Unsandboxed, highest privilege. Has direct access to the filesystem, network, and profile data. All IPC endpoints must treat inputs from other processes as untrusted.
* **Renderer Process:** Strongly sandboxed, lowest privilege. Runs untrusted web code (HTML/JS/Wasm/DOM). Other processes should assume that the renderer is compromised by attackers.
* **Utility Process:** Sandboxed (sandbox strength can be stronger than the renderer's but that depends on utility sandbox type and whether code is reachable from web content). Used for decoding, parsing, or unpacking untrusted data.
* **Network Service (only on macOS) / GPU Process:** Specialized sandboxed processes handling GPU commands and network I/O.

  *`Note: The sandbox varies based on the OS. Android in particular has weak sandboxing so Android GPU bugs are always Sev-0.`*

### The Compromised Renderer Assumption

Vulnerabilities that rely on a malicious web page compromising its own renderer process are valid as is the bug that allowed the renderer to be compromised.

* Your threat model must specify whether your code defends against a compromised renderer.
* While it is possible for a compromised renderer to leak information between sites, we generally don't have to worry about that because it is difficult for an attacker to arrange for two different sites to share the same renderer. In environments where Site Isolation is utilized, it ensures cross-site data is partitioned into separate processes; a compromised renderer cannot access another site's process without a sandbox escape or Site Isolation bypass bug.

### The Rule of Two

Chromium permits at most two of these dangerous properties in the same process:

**1. Untrustworthy input** (web content, network data)

**2. Complex processing** (parsing, decompression, compilation)

**3. Memory-unsafe language** (C/C++)

If all three are combined, the code must run in a sandboxed process.

*`Note: Do not prompt agents to audit Rule of Two compliance directly, as valid browser C++ will trigger false positives.`*

## Recommended Structure & Authoring Guide

Below is the recommended structure for a Chromium `SECURITY.md` file.

### 1. Scope

Define exact source tree paths and explicit exclusions.

```
## Scope

This policy applies to code located in `//chrome/browser/my_component/` and subdirectories.

It does not apply to `//chrome/browser/my_component/testing/` or mock utilities.
```

### 2. Security Boundaries & Threat Model

Define execution context, input hostility, and trust assumptions.

```
## Security Boundaries & Threat Model

### Process type & Sandbox

Runs in the sandboxed Renderer process (or: sandboxed Utility process, un-sandboxed Browser process).

### Inputs

- Untrusted: JavaScript parameters and DOM data passed from web content.

- Trusted: Mojo IPC responses received from the Browser process.

### Trust relationships & Renderer Compromise

- Defends against a compromised Renderer: Yes / No.

- Trusts the Browser process. Does not trust peer Renderer processes.
```

### 3. Security Boundaries vs. Out-of-Scope (Non-Bugs)

Vulnerabilities that are globally in scope across Chromium, do not need to be restated in the SECURITY.md files as doing so risks accidental negative-implication by scanners or researchers. The security boundaries section should focus on component-specific boundaries based on common false-positive patterns such as:

* **Intra-Process Privacy Partitioning:** Bypassing client-side storage partitioning or fenced frame restrictions from within the same compromised renderer process (privacy features are not process-isolated security boundaries).
* **Feature Flags/ Disabled code:** Flaws in code paths that are disabled by default or behind experimental flags without a proof of concept that the exploit can also enable the flag or code.

*`Note: AI security scanners rely heavily on this section, so making sure that it's clear can reduce the number of and improve the quality of bugs that your engineers will need to triage.`*

```
## Security Boundaries & Explicit Non-bugs

### Security Boundaries
- Enforces origin checks on privileged actions requested by Mojo IPC.

- Sanitizes untrusted data before allowing other components to access it.

### Out of scope (non-bugs)

- Crashes in safe sandboxed contexts: CHECK failures or null-pointer dereferences in the sandboxed Utility process that terminate safely without memory corruption.

- Intra-process state: Bypassing local in-memory cache limits from within the same renderer process.

- Internal API preconditions: Calling FooService methods directly with invalid pointers via unit tests or internal C++ calls without a reachable IPC path.
```
### 4. Design Justifications & Accepted Risks

Document deliberate architectural trade-offs made for performance or memory efficiency.
```
## Design Justifications & Accepted Risks

### Shared memory IPC

Uses shared memory for high-throughput video frame transfers. Mitigated by validating all header metadata and buffer sizes before reading.

### Accepted risk: network timing

Network setup timing side channels are accepted residual risks.
```

### 5. Further Documentation (Optional)

Link to deeper design documents or architecture diagrams.
```
## Further Documentation
- [Component Design Doc](path-to/my-component-design): Architecture overview.

- [Mojo Interface Specification](path-to/my-component-mojo): IPC definitions and security considerations.
```
## Example Template: Privileged Browser-Process Service
```
# Security Model for [Browser Service Name]

## Scope

This policy applies to code in `//chrome/browser/my_service/` and subdirectories.

## Security Boundaries & Threat Model

### Process Type & Sandbox

Runs in the un-sandboxed Browser process. Has full system access and profile data capability.

### Inputs

- Untrusted / Semi-trusted: Mojo IPC messages received from sandboxed Renderer processes (`//third_party/blink/`). Renderers may be compromised.

- Trusted: OS signals and internal local configuration.

### Mojo IPC Validation

Must validate all Mojo arguments and verify calling process origin via ChildProcessSecurityPolicy. Out-of-bounds arguments or invalid origins must trigger mojo::ReportBadMessage().

## In-Scope vs. Out-of-Scope

### In scope

- Failure to validate origin or process permissions on incoming Mojo messages.

- Memory corruption or logic bypasses in the Browser process triggered by IPC messages.

### Out of scope (non-bugs)

- Expected feature disablement: Inability to access protected resources when user permission is denied.
```

## Policy Reference Table

Consult these canonical policy guides when defining your component's security contract:


| Category | Policy / Guide | Best Used For |
| :---- | :---- | :---- |
| Core Rules | [Chrome Security Rules](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/rules.md) | All Chromium components |
| Core Rules | [Chrome Security FAQ](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/faq.md) | All Chromium components |
| Core Rules | [Security for Agents](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/security-for-agents.md) | AI-agent scannability |
| Core Rules | [Severity Guidelines](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/severity-guidelines.md) | Bug classification |
| Mojo & Web | [Mojo IPC Security](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/mojo.md#Security) | Mojo interface safety |
| Mojo & Web | [Web Platform](https://chromium.googlesource.com/chromium/src/+/HEAD/docs/security/web-platform-security-guidelines.md) | Web-facing APIs |

## Real-World Canonical Examples & References

Consult live SECURITY.md files and reference documentation in the Chromium repository for gold-standard patterns:

* [//content/SECURITY.md](https://chromium.googlesource.com/chromium/src/+/HEAD/content/SECURITY.md): Process boundaries & compromised renderer threat modeling
* [//v8/SECURITY.md](https://chromium.googlesource.com/chromium/src/+/HEAD/v8/SECURITY.md): Top-level security contract for the V8 engine
* [Security for Agents](https://source.chromium.org/chromium/chromium/src/+/main:docs/security/security-for-agents.md): Canonical agent security guidance
* [AI-Generated Security Bugs FAQ](https://chromium.googlesource.com/chromium/src/+/main/docs/security/ai-generated-security-bugs-faq.md): AI bug filing context
