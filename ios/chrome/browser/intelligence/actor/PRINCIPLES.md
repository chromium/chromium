# Actor Principles

This document states the principles behind
`//ios/chrome/browser/intelligence/actor`: the "why" of the design, the external
context that cannot be inferred from the code, and the high-level guidance that
every change to this directory is expected to honor. It is written for humans
and agents alike.

It deliberately contains no implementation details. For those, read the code.

[TOC]

## Purpose

<!-- TODO(crbug.com/575833837): Describe what the actor is. -->

## Principles

Each principle follows the same shape so it can be applied to a CL consistently:
a one-sentence statement, why it holds, what it implies for a change, and the
known exceptions.

### AI-provider agnostic

**Statement.** The actor service is AI-provider agnostic.

**Why.** The actor service is intended to be compatible with any AI provider
that wants to actuate in the browser.

**For a CL, this means.** There are no mentions of, or specialized logic for, a
given AI provider or client (e.g. Gemini).

**Exceptions.** Caller identification for attribution or telemetry, without
branching behavior on it.

### Decoupled UI

**Statement.** The actor service is decoupled from any UI.

**Why.** UIs are subject to change, and the actor service can be run headless.

**For a CL, this means.** The actor service is the headless layer under
`model/`, `tools/` and `public/`; `ui/` and `coordinator/` are the actor's own
presentation layer, which consumes the service like any other client. The
service should not be the source of any direct UI elements, such as text labels,
or alter its state to drive UI presentation.

**Exceptions.** UI data flowing *through* the actor service, though not
originating from it.

### Single entry point

**Statement.** The actor service is the sole entry point to the actor.

**Why.** Routing all interactions through the actor service keeps actor task
lifecycle, state transitions, tool engine, and execution invariants encapsulated
in one place.

**For a CL, this means.** Neither the actor task nor the actor engine is exposed
publicly or manipulated directly outside the actor service. Clients, including
`ui/` and `coordinator/`, interact with tasks exclusively through the actor
service.

**Exceptions.** Unit tests that test the actor task or engine in isolation.

## External context

<!-- TODO(crbug.com/575833837): Link any useful external context, such as the
Desktop counterpart. -->
