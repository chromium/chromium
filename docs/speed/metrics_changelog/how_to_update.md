# How to Update Web Vitals Changelogs

This document provides instructions for updating the changelogs in this directory. It is intended for both human contributors and AI agents.

## File Formats

All changelogs in this directory use **Markdown** (`.md`). This format is preferred because:
1. It is human-readable.
2. It renders well in Gitiles (Chromium's code browser).
3. It is easy for AI agents to parse and generate.

## Expected Format

To make it easier for AI agents to update these files reliably, a **consistent structure** is recommended.

### Structure
- Reverse chronological order by Chrome version.
- Bulleted list under each version.
- Each bullet should start with a category prefix followed by a colon and a link or description. *Note: For complex overhauls or experimental features (like Soft Navigations), descriptive headers may be used instead of category prefixes if they provide better clarity.*

**Common Categories:**
- `Metric definition improvement:` Changes to the algorithm or what counts towards the metric.
- `Metric bug fix:` Fixes to bugs in the metric implementation.
- `Bug:` Known issues or bugs introduced.
- `Implementation optimizations:` Performance improvements in the measurement code.
- `Launch feature:` New features or flags enabled.

**Example:**
```markdown
* Chrome 143
  * Metric bug fix: [Fixed text presentation timestamps reporting too early, impacting LCP (introduced in M140)](2025_09_lcp.md)
```

## Tone and Level of Detail

The changelogs are intended for **external developers** to understand what might affect their metrics or how they measure them, as well as for maintainers.

### Focus on Developer Impact
- **Prioritize Web-Exposed Effects**: Focus on changes that have observable web effects or change how metrics are reported (e.g., new event types, API changes, behavioral differences).
- **Explain the "Why"**: Adopt an explanatory tone similar to the INP documents (e.g., `inp.md` and linked files). Explain the *rationale* behind the change and *how it affects a site's metrics*.

### Handling Complex Changes
- **Use Dedicated Files**: When a change is complicated (e.g., a major revamp or a new Origin Trial), create a dedicated file for it and link to it from the main index file (like `inp.md` does).
- **Structure for Dedicated Files**:
  - **Feature Description**: Explain the feature and rationale.
  - **How does this affect a site's metrics?**: Explain the expected impact on field or lab data.
  - **When were users affected?**: List release milestones and rollout details.

### Preserving Implementation Details for Maintainers
- If lower-level details (refactorings, optimizations, internal bugfixes) are useful for maintainers to keep in the document, group them under a single parent bullet like `* Other implementation changes:` and use sub-bullets for the specific items, keeping the descriptions succinct.

## Instructions for AI Agents

### Finding Changes

To find relevant changes, use a combination of path-based and author-based searches:

1.  **Path-based Search**: Review logs for the directories listed in the "Relevant Directories and Files to Check" section at the bottom of this file.
2.  **Author-based Search**: Review **ALL commits** by any of the owners listed in `third_party/blink/renderer/core/timing/OWNERS` for the target milestone period. These owners frequently land changes related to Web Vitals and performance metrics, and their commits are highly likely to contain relevant updates, even if they touch files outside the primary directories.
3.  **Keyword-based Search**: Search commit messages for specific keywords if path-based search is too broad or misses context. High-value keywords include:
    *   "SoftNavigation" or "soft nav"
    *   "InteractionContentfulPaint" or "ICP"
    *   "TaskAttribution"

*Tip for Jujutsu (`jj`) users:* To search for commits affecting a file, use the `files()` function in your revset (e.g., `jj log -r "files(path/to/file)"`), as the `file()` function does not exist.

### To Update the Changelog

- First, find the last update made to each changelog, to get a baseline for which Chrome milestones already have published changes.
- Next, scan for any code changes made after that date.
  - Recommend dumping the full log of changes (descriptions + files changed, without full patch) into a temporary file
  - Filter the list down to relevant commit messages.  A commit message is relevant if it modifies code related to Web Vitals or Soft Navs.
  - Filter the list down for relatively simple or uninteresting changes-- we only need to document important or web exposed changes.
- Start with a summary of changes in the main markdown files, without creating detailed descriptions (i.e. by each date/milestone)
- Once the human reviewer confirms the high level descriptions are adequete, create detailed descriptions for each date/milestone, and link to the relevant CLs, wherever useful. **IMPORTANT**: Use public sharing links for CLs, such as full Gerrit review URLs (`https://chromium-review.googlesource.com/c/chromium/src/+/...`) or `crrev.com/<hash>` shorteners. Do NOT use local `file://` URLs.
- **Verification**: Whenever you add a link to a CL to the changelogs, read the full contents of that CL (or its commit message and diff) to double check if your summary is correct. Avoid relying solely on high-level notes without verifying the actual code changes.

## Relevant Directories and Files to Check

When searching for changes to include in the logs, AI agents should check the following locations in the repository. Changes in these directories often impact the metrics, and commit messages should be reviewed to see if they are worth mentioning:

- `third_party/blink/renderer/core/timing/`
  - Primary location for Web Vitals APIs and performance entries.
  - Check for changes to LCP, INP, CLS, and Soft Navigations here.
  - *Tip for Soft Navs:* Pay special attention to `soft_navigation_heuristics.cc` and `soft_navigation_context.cc`.
- `third_party/blink/renderer/core/layout/`
  - Contains computation logic for Layout Shift (e.g., look for files related to layout shift tracking).
- `third_party/blink/renderer/core/paint/`
  - Contains logic for Paint Timing and Largest Contentful Paint computations.
- `components/page_load_metrics/`
  - Contains browser-side metrics collection and reporting logic (often where metric definitions are finalized or recorded for UKM).
- `third_party/blink/renderer/core/scheduler/`
  - Contains Task Attribution logic (e.g., `task_attribution_tracker_impl.cc`), which is critical for tracking causality in Soft Navigations.
