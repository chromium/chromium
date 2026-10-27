---
name: review-readiness
description: >-
  Scores how ready a CL is for human review, from 0 to 10, and reports the
  score as a single finding on the commit message. Always use for every CL this
  skill is loaded for, whatever the file types or the size of the change, and
  including CLs with no other findings.
---

# Review Readiness Score

Every change this skill is loaded for gets one readiness score so that authors
know how much work remains before asking a human. The score summarizes the
review; it never replaces individual findings.

## Procedure

1. Finish the review first. The score is computed from the final, deduplicated
   list of findings and from the change as a whole, not from candidate findings.
   Findings include everything the review produced: the general review and any
   skills or knowledge base rules loaded for this change.
2. Assign an integer from 0 to 10 using the scale below. Pick the lowest band
   whose description applies.
3. Report the score as exactly one additional finding (see Output). Report it
   even when the list of other findings is empty.

## Scale

- 10: No issues found. Small, single-purpose change; clear description; tests
  cover any new behavior. A reviewer can approve after a quick read.
- 8–9: Only minor suggestions, or one or two small issues that are quick to
  address. Scope and description are clear.
- 6–7: Several small issues, or one that needs a design decision; or the change
  is larger than its description suggests; or tests are thin for new logic.
- 4–5: At least one serious issue (correctness, safety, a broken contract), or a
  clear violation of guidance that applies to the change, or the change mixes
  unrelated concerns that should be split.
- 2–3: Multiple serious issues, or a violation of applicable guidance with no
  stated rationale, or the change cannot be evaluated without information that
  is not in the CL.
- 0–1: The change is not reviewable as is: pervasive correctness or safety
  problems, or it contradicts the stated purpose of what it touches.

Adjust within a band for description quality (does it say why, not only what),
test coverage of any new behavior, and change size relative to its purpose.

## Output

The review tooling has no change-level comment, so the score is attached to the
first line of the commit message:

- `file_path`: `/COMMIT_MSG`
- `start_line`: `1`
- `severity`: `INFO`
- `message`: first line `Review readiness: <N>/10`, then two to three short
  bullets naming the factors that set the score (for example the count and
  severity of other findings, scope, tests, description). Do not restate the
  other findings and do not introduce new issues here; anything actionable
  belongs in its own finding.

This finding is a summary by design. It is exempt from the rule that findings
must be actionable, and it is reported exactly once per review.
