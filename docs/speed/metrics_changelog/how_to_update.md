---
name: update-web-vitals-changelog
description: Instructions, workflows, templates, and pre-submission checklists for maintaining and updating Chrome Core Web Vitals speed metrics changelogs (INP, LCP, CLS, Soft Navigations).
---

# How to Update Web Vitals Changelogs

This document defines the instructions, formatting standards, discovery workflows, and validation checklists for maintaining Core Web Vitals speed metrics changelogs. It is designed to guide both human contributors and AI agents to produce high-quality, standardized changelogs requiring minimal review iterations.

---

## 1. Scope & Audience (Public Communications)

These changelogs are **public communications intended for external web developers, browser engineers, and RUM (Real User Monitoring) analytics providers**.

Their goal is to explain:
- What changes affect Core Web Vitals measurements.
- How Performance Timeline APIs behave in JavaScript.
- How field (CrUX / PageLoad UKM) or lab data shifts across Chrome releases.

### Inclusion Rules

| Category | Include | Do NOT Include |
| :--- | :--- | :--- |
| **APIs & Web Platform** | Changes to web-exposed performance entries (`PerformanceEventTiming`, `PerformanceSoftNavigation`, `LargestContentfulPaint`, `LayoutShift`), new IDL attributes, IDL type modifications, timing algorithm fixes, and exposed target encapsulation behavior. | Internal Blink/V8 bindings refactorings that preserve identical IDL types and runtime behavior. |
| **Field Metrics (CrUX / UKM)** | Changes to metric definitions, calculation algorithms, aggregation logic, or browser engine optimizations that shift real-world Web Vitals scores in CrUX / PageLoad UKM. | Internal Chromium histograms, UMA breakdown additions, or internal browser telemetry. |
| **Diagnostic Attribution** | Modifications to attribution data (e.g. `entry.target`, `entry.sources`, `interactionId` assignment timing, coordinate units). | Internal Perfetto trace categories, trace tracks, or debugging logs. |
| **Engine Maintenance** | N/A | Dead code removals, compiler warning fixes, test deflaking, and private architectural refactoring without observable impact. |

---

## 2. Two-Tier Document Architecture

All changelog documentation follows a **two-tier hierarchy**:

```
docs/speed/metrics_changelog/
├── how_to_update.md               # This skill and instruction document
├── inp.md                         # Tier 1: Main index for Interaction to Next Paint
├── lcp.md                         # Tier 1: Main index for Largest Contentful Paint
├── cls.md                         # Tier 1: Main index for Cumulative Layout Shift
├── soft_navigations.md            # Tier 1: Main index for Soft Navigations
├── 2025_01_inp.md                 # Tier 2: Dedicated milestone deep-dive (Chrome 134)
├── 2026_02_cls.md                 # Tier 2: Dedicated milestone deep-dive (Chrome 145)
├── 2026_02_lcp.md                 # Tier 2: Dedicated milestone deep-dive (Chrome 147)
└── ...
```

### Tier 1: Main Metric Index Files (`inp.md`, `lcp.md`, `cls.md`)
- Reverse-chronological table of contents organized by Chrome milestone.
- Standardized, single-bullet entries with fixed category prefixes.
- Every bullet links either to a **Tier 2 dedicated document** (for major items) or directly to a **public Gerrit CL** (for minor bugfixes).

### Tier 2: Dedicated Milestone Files (`YYYY_MM_<metric>.md`)
- Standalone deep-dive documents for major feature launches, metric definition revisions, standard API updates, or complex behavioral changes.
- File naming convention: `YYYY_MM_<metric>.md`, where `YYYY_MM` is the year and month of the Chrome release milestone branch point (e.g. `2026_02_inp.md` for Chrome 147 branching in February 2026).

### Decision Matrix: Dedicated Doc vs. Inline Bullet

```
┌────────────────────────────────────────────────────────┐
│ Is it a feature launch, API addition, metric revision, │
│ spec alignment, or broad performance optimization?     │
└───────────────────────────┬────────────────────────────┘
                            │
              ┌─────────────┴─────────────┐
              ▼                           ▼
            [YES]                        [NO]
  Create Dedicated Doc           Is it an isolated,
  YYYY_MM_<metric>.md          self-contained bugfix?
  and link from Index                     │
                                          ▼
                                Create Inline Bullet
                                with public CL link
                                in the Main Index
```

---

## 3. Standard Templates

### 3.1. Main Index Bullet Format (`inp.md`, `lcp.md`, etc.)

```markdown
* Chrome <Milestone>
  * Launch feature: [<Feature Title>](YYYY_MM_<metric>.md)
  * Metric definition improvement: [<Feature Title>](YYYY_MM_<metric>.md)
  * Implementation optimizations: [<Feature Title>](YYYY_MM_<metric>.md)
  * Metric bug fix: [<Fix Summary>](YYYY_MM_<metric>.md)
  * Metric bug fix: <Brief summary> ([<CL_ID>](https://chromium-review.googlesource.com/c/chromium/src/+/<CL_ID>)).
```

**Index Rules:**
1. **Deduplicate Merges/Backports**: If a fix was authored in milestone N but cherry-picked/merged into milestone N-1, **only** list it under milestone N-1 where users were first affected.
2. **Link Every Topic**: When a dedicated file contains multiple sections for the same milestone, ensure all corresponding bullets in the index file link to that dedicated file.

---

### 3.2. Dedicated Milestone Document Template (`YYYY_MM_<metric>.md`)

Use this template for all Tier 2 files. When documenting multiple distinct changes for the same milestone, separate each section with a horizontal rule (`---`).

```markdown
# <Metric Full Name> Changes in Chrome <Milestone>

## <Descriptive Feature Title>

<1-2 sentences summarizing what changed, the feature flag or API name, and the core motivation.>

<Detailed explanation comparing previous behavior with new behavior. Describe why the change was made, technical context, and how edge cases are resolved.>

<External references: W3C specification issues/PRs, ChromeStatus features, Chromium bug IDs, and Gerrit review CLs.>
The specification discussion can be found in [w3c/<repo>#<issue>](https://github.com/w3c/<repo>/issues/<issue>) and [chromestatus.com/feature/<id>](https://chromestatus.com/feature/<id>).
The source change can be found in [crrev.com/c/<cl_id>](https://chromium-review.googlesource.com/c/chromium/src/+/<cl_id>).

### How does this affect a site's metrics?

- **CrUX / UKM (Field Data)**: <Explicitly state whether aggregate field metrics (75th percentile) are expected to increase, decrease, become more stable, or remain unchanged.>
- **Real User Monitoring (RUM) & PerformanceObserver**: <Describe exact differences in JavaScript performance entries, such as earlier/intermediate entry delivery, null values for detached targets, interaction ID assignment timing, or new attributes.>
- **Metric Values vs. Attribution**: <Clarify whether the numerical metric score changes or if this is purely an API/reporting precision improvement.>

### When were users affected?

Chrome <Milestone> reached stable users in <Month Year>.
```

---

## 4. Writing the "How does this affect a site's metrics?" Section

To prevent human review iterations, this section MUST explicitly address the following criteria:

1. **Direction of Score Impact**: Explicitly declare if field metrics will **increase (regress)**, **decrease (improve)**, or **remain neutral**.
   - *Avoid vague terms*: Never say *"improves metrics"* without clarifying if it means faster load times (lower milliseconds), higher reporting coverage, or better diagnostic fidelity.
2. **Field (CrUX / UKM) vs. Lab / RUM**: Explicitly distinguish between aggregate field data (evaluated at page unload or first interaction) and real-time JavaScript `PerformanceObserver` emissions.
3. **Coverage & Reporting Rates**: If a fix prevents dropped entries during page unload or abandonment, note the increase in field reporting coverage.
4. **Attribution vs. Score**: If a change only affects metadata (e.g. `entry.target`, `entry.sources`, `interactionId`), explicitly state that the overall metric score values remain unchanged.

---

## 5. AI Agent Discovery & Execution Workflow

AI agents updating these changelogs MUST follow this step-by-step procedure:

```
[1. Baseline Check] ──> [2. Multi-Layered Search] ──> [3. Candidate Filter]
                                                              │
[6. Presubmit Check] <── [5. Dedicated Doc Authoring] <── [4. Index Drafting]
```

### Step 1: Baseline Check
Read the latest Chrome milestone already published in `inp.md`, `lcp.md`, `cls.md`, and `soft_navigations.md`.

### Step 2: Multi-Layered Search
To avoid missing commits due to file moves or directory restructurings, use all four search strategies:
1. **Subtree Path Searches (Recursive)**:
   - Query full subtrees: `third_party/blink/renderer/core/timing/`, `third_party/blink/renderer/core/paint/`, `third_party/blink/renderer/core/layout/`, `components/page_load_metrics/`.
   - *Do NOT search static lists of specific `.cc` files.*
2. **Feature Flag Transitions (`runtime_enabled_features.json5`)**:
   - Check `third_party/blink/renderer/platform/runtime_enabled_features.json5` for flags toggled to `status: "stable"` or `status: "experimental"`.
3. **Author Searches Across Timing OWNERS**:
   - Review commits authored by owners in `third_party/blink/renderer/core/timing/OWNERS` (e.g., Scott Haseley, Michal Mocny, Ian Clelland, etc.) during the target date window.
4. **Keyword Searches**:
   - Search commit logs for: `LCP`, `LargestContentfulPaint`, `INP`, `EventTiming`, `LayoutShift`, `SoftNavigation`, `ICP`, `PaintTiming`, `interactionId`, `targetSelector`.

### Step 3: Candidate Filtering
- Filter out internal telemetry (histograms.xml), trace events, compiler fixes, test deflaking, and private refactorings.
- Cross-check commit merge dates against official branch point dates to ensure exact milestone attribution.

### Step 4: Index Drafting
Draft bullet entries in the corresponding main index file (`inp.md`, `lcp.md`, etc.) using standard category prefixes.

### Step 5: Dedicated Doc Authoring
- Create `YYYY_MM_<metric>.md` files for major changes using the Section 3.2 template.
- **Verify with Source**: Read the full commit message, bug description, and diff of every CL to ensure technical accuracy.
- Search for W3C issues/PRs (`w3c/event-timing`, `w3c/largest-contentful-paint`, `WICG/layout-instability`) and ChromeStatus features.

### Step 6: Pre-Submission Quality Checklist
Before requesting review or finalizing edits, verify every item:
- [ ] **Spellcheck**: Double-check spelling (e.g. ensure `improved` is not misspelled as `imporved`).
- [ ] **Public URLs Only**: All links must use public URLs (`https://chromium-review.googlesource.com/...` or `crrev.com/...`, `https://github.com/w3c/...`). Never use local `file://` URLs.
- [ ] **Backport & Deduping**: Changes merged to an earlier milestone are only documented under that earlier milestone.
- [ ] **Cross-links Complete**: All items in multi-topic dedicated files are linked from the main index.
- [ ] **Presubmit Verification**: Run `git cl presubmit --force --upload` locally and confirm 0 errors and 0 warnings.

---

## 6. Key Repositories and File Paths Quick Reference

| Directory / File | Description & Focus |
| :--- | :--- |
| `third_party/blink/renderer/core/timing/` | Primary location for Web Vitals APIs (`PerformanceEventTiming`, `LargestContentfulPaint`, `PerformanceSoftNavigation`). |
| `third_party/blink/renderer/core/paint/` & `core/paint/timing/` | Paint Timing, FCP, LCP computation, text/image paint records. |
| `third_party/blink/renderer/core/layout/` | Layout Instability and Layout Shift tracking logic. |
| `components/page_load_metrics/` | Browser-side metric aggregation, UKM recording, and PageLoad observer logic. |
| `third_party/blink/renderer/core/scheduler/` | Task attribution and input deferral logic. |
| `third_party/blink/renderer/platform/runtime_enabled_features.json5` | Feature flag statuses (experimental/stable launches). |
