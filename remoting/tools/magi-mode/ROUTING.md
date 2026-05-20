# MAGI Module Routing Index

This file acts as a routing catalog for the Orchestrator. It identifies the
technical rulesets and implementation modules available for execution and
auditing.

## Execution Agents

These modules are responsible for investigation, scaffolding, and code
synthesis.

- **Scoping:** Investigation, codebase research, goal definition. *Path:*
  `src/remoting/tools/magi-mode/personas/core/scoping.json`
- **Synthesis:** Maintainability, Chromium idioms, `//base` primitives, and
  final code synthesis. *Path:*
  `src/remoting/tools/magi-mode/personas/core/implementation.json`

## Scanners (Auditors)

Specialized experts who perform rigorous, boolean-checklist-based audits.

- **The Security Scanner:** Memory safety, exploit prevention, logic. *Path:*
  `src/remoting/tools/magi-mode/personas/core/security.json`
- **The Performance Scanner:** Latency, zero-copy, sequence affinity. *Path:*
  `src/remoting/tools/magi-mode/personas/core/performance.json`
- **The Core Auditor:** Consistency with existing patterns and idioms. *Path:*
  `src/remoting/tools/magi-mode/personas/core/auditor.json`
- **The Test Expert:** Testability, edge-cases, framework usage. *Path:*
  `src/remoting/tools/magi-mode/personas/auxiliary/test.json`
- **The Concurrency Expert:** `base::PostTask` safety, preventing deadlocks.
  *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/concurrency.json`
- **The Privacy Expert:** PII prevention, UMA/UKM metrics. *Path:*
  `src/remoting/tools/magi-mode/personas/auxiliary/privacy.json`
- **The Build Expert:** `DEPS` compliance, `#include` bloat, GN boundaries.
  *Path:* `src/remoting/tools/magi-mode/personas/auxiliary/build.json`
- **The Readability Expert:** Clean code, naming, cognitive complexity. *Path:*
  `src/remoting/tools/magi-mode/personas/auxiliary/readability.json`

## Platform Specialists

Use these when modifying platform-specific implementations.

- **Windows File API Expert:** `ScopedHandle`, file locking, security
  descriptors. *Path:*
  `src/remoting/tools/magi-mode/personas/windows/file_api.json`
- **Linux Wayland Expert:** `wl_buffer` management, protocol integration.
  *Path:* `src/remoting/tools/magi-mode/personas/linux/wayland.json`

## Domain Specialists

Use these when modifying specific technical domains like media or networking.

- **WebRTC Expert:** `PeerConnection`, signaling, WebRTC threading model.
  *Path:* `src/remoting/tools/magi-mode/personas/domain/webrtc.json`
- **Codec (AV1) Expert:** `libaom` settings, bitrate adaptation, latency.
  *Path:* `src/remoting/tools/magi-mode/personas/domain/codec_av1.json`

## System Meta-Scanners

Use these when auditing or modifying the MAGI protocol itself.

- **LLM Behavior & Grounding Expert:** Hallucination prevention, prompt
  engineering, state machine safety. *Path:*
  `src/remoting/tools/magi-mode/personas/ai/llm.json`
- **MAS Architect:** Multi-agent system architecture, handoff stability,
  verification loop efficiency. *Path:*
  `src/remoting/tools/magi-mode/personas/ai/mas.json`
