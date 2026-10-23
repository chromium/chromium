---
name: actor-principles
description: >-
  Reviews changes under ios/chrome/browser/intelligence/actor/ against the
  directory's principles. ALWAYS use for any CL that modifies a file under
  ios/chrome/browser/intelligence/actor/, whatever the file type or the size of
  the change.
---

# Actor Principles Review

Every change under `ios/chrome/browser/intelligence/actor/` is reviewed against
`ios/chrome/browser/intelligence/actor/PRINCIPLES.md`. That document is the
single source of truth; this skill only says how to apply it.

## Procedure

1. Read `ios/chrome/browser/intelligence/actor/PRINCIPLES.md` in full before
   looking at the diff. Do not rely on a summary or on a previous reading.
2. For each principle, decide whether the change upholds it, is neutral, or
   violates it. Judge the change as a whole, not line by line.
3. Report a finding only for a violation. Cite the principle by its heading, say
   in one or two sentences why the change violates it and what a change that
   honors it would look like, and link
   `ios/chrome/browser/intelligence/actor/PRINCIPLES.md`.

## Scope

- The principles are about intent and design. Do not use this skill to comment
  on style, naming, formatting or implementation details.
- The exceptions listed under a principle are deliberate. Do not flag a change
  that falls under one of them.
- If the change and the document disagree in a way the document does not
  anticipate, say so as a question rather than a finding, so the authors can
  decide whether the code or the principle should change.
