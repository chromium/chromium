# Keyboard Shortcuts in Chrome: Guidelines & Lifecycle Process

[TOC]

## Overview:

Chrome's keyboard shortcut landscape is constrained by a finite set of
ergonomic, accessible, and collision-free key combinations. Over time, as new
features and capabilities are added across Desktop (Windows, macOS, Linux,
ChromeOS), the availability of unassigned keyboard shortcuts has effectively
become 0.

Adding a global keyboard shortcut carries substantial long-term technical and
user-experience costs:
* **Internationalization & Keyboard Layouts**: Modifiers and punctuation keys
  behave differently across international keyboards (AZERTY, QWERTZ, Dvorak,
  JIS, etc.), often clashing with `AltGr` or non-ASCII character generation
  (see [Why Ctrl+Alt shouldn't be used as a shortcut
  modifier](https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003)).
* **Operating System Collisions**: Each host OS reserves dozens of key
  combinations for window management, assistive tools, and system actions.
* **Web Platform & Extension Conflicts**: Shortcuts intercept key events before
  or after web pages, potentially breaking user workflows in web productivity
  apps (Google Docs, Sheets, VS Code Web, Figma) and extensions.
* **Accessibility**: Improper shortcut design interferes with screen readers,
  switch navigation, and voice control.
* **User Muscle Memory**: Once a shortcut is shipped to hundreds of millions of
  users, changing or removing it causes severe disruption.

Therefore, **global keyboard shortcuts are treated as a strictly conserved
browser resource**. New shortcuts must pass strict qualification criteria,
telemetry validation, and OWNERS approval.

---

## Acceptable vs. Unacceptable Shortcuts

### Permitted Key & Modifier Conventions

| Platform | Standard Modifier Patterns | Acceptable Key Range |
| :--- | :--- | :--- |
| **Windows / Linux** | `Ctrl + <Key>`<br>`Ctrl + Shift + <Key>`<br>`Alt + Shift + <Key>` | `A–Z`, `0–9`, Function keys (`F1–F12`), Navigation keys (`Tab`, `Esc`, `Enter`, `Home`, `End`, `PgUp`, `PgDn`, Arrows) |
| **macOS** | `Cmd + <Key>`<br>`Cmd + Shift + <Key>`<br>`Cmd + Option + <Key>` | `A–Z`, `0–9`, Navigation keys (`Tab`, `Esc`, `Enter`, `Home`, `End`, `PgUp`, `PgDn`, Arrows) |
| **ChromeOS** | `Ctrl + <Key>`<br>`Alt + <Key>`<br>`Search/Launcher (Cmd) + <Key>` | `A–Z`, `0–9`, ChromeOS top-row action keys, Navigation keys |

---

### Prohibited & Disallowed Key Combinations

#### 1. DO NOT use `Ctrl + Alt` as a modifier on Windows or Linux
* **Reason**: On Windows and Linux, `Ctrl + Alt` is identically mapped to
  `AltGr` (`VK_RMENU`). International keyboard layouts (e.g. German, French,
  Polish, Spanish, Nordic) require `AltGr` to type essential characters and
  symbols (such as `@`, `€`, `\`, `[`, `]`, `~`, `|`, `{`, `}`, `ł`). Binding
  `Ctrl + Alt + <key>` disables typing those characters for international users
  (see [Why Ctrl+Alt shouldn't be used as a shortcut
  modifier](https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003)).

#### 2. DO NOT use `Option` or `Shift + Option` as modifiers on macOS
* **Reason**: On macOS, `Option` (and `Shift + Option`) by itself without `Cmd`
  is reserved for text input and language typing to generate accented
  characters, diacritics, and special glyphs (e.g., `Option + e` for acute
  accent `´`, `Option + u` for umlaut `¨`, `Option + c` for `ç`, `Option + Shift
  + /` for `¿`). Binding shortcuts to `Option + <key>` or `Shift + Option +
  <key>` intercepts and breaks normal typing for international languages.

#### 3. DO NOT use unstable punctuation keys
* **Reason**: Punctuation key positions vary drastically across physical and
  software keyboard layouts (e.g., `OEM_1` through `OEM_8`, brackets, slashes,
  backticks, semicolons; AZERTY swaps numbers and symbols; French/German
  keyboards require `Shift` or `AltGr` for brackets and slashes).
* This is enforced by an automated unit test
  `AcceleratorTableTest.DontUseKeysWithUnstablePositions` in
  `chrome/browser/ui/accelerator_table_unittest.cc`. Only standard alphanumeric
  keys (`A–Z`, `0–9`) and navigation/function keys are permitted. (Existing
  exceptions like Zoom In/Out `Ctrl + +/-` are grandfathered legacy bindings).

#### 4. DO NOT introduce single-key shortcuts in global browser scope
* **Reason**: Violates Google Accessibility Rating (GAR Web Criterion 1.19 /
  GAR 2027) and WCAG 2.1/2.2 Criterion 2.1.4 (*Character Key Shortcuts*).
  Single keys (keys without `Ctrl`/`Cmd`/`Alt`) accidentally trigger actions
  during typing, break voice-typing input, and hinder switch-access users.
* **Scope**: Single-key shortcuts may only be used when focus is explicitly
  trapped inside a dedicated custom view (such as full-screen media playback
  or PDF viewer), never globally in the browser window.

#### 5. DO NOT conflict with OS or Window Manager Reserved Keys
*(Note: The following platform lists are non-exhaustive; always refer to each
OS's official keyboard design guidelines).*
* **Windows**: Any Windows key combination (`Win + <key>`) is strictly reserved
  for the operating system. Common OS-reserved combinations include `Win + *`,
  `Ctrl + Shift + Esc` (Windows Task Manager), `Alt + Tab`, `Alt + F4`, `Alt +
  Space`, `Ctrl + Alt + Del`, `Win + L`, `Win + D`.
* **macOS**: `Cmd + Tab`, `Cmd + Space`, `Cmd + Q`, `Cmd + H`, `Cmd + M`, `Ctrl
  + Up/Down` (Mission Control), `Cmd + Option + Esc`.
* **Linux**: Window manager bindings (`Alt + F1–F12`, `Super + *`, workspace
  switching).
* **ChromeOS**: OS-specific keys (brightness, volume, power, lock, launcher
  search).

#### 6. DO NOT conflict with Core Web & Editing Standards
* **Editing Standards**: `Ctrl/Cmd + C` (Copy), `Ctrl/Cmd + V` (Paste),
  `Ctrl/Cmd + X` (Cut), `Ctrl/Cmd + Z` (Undo), `Ctrl/Cmd + Y` / `Ctrl/Cmd +
  Shift + Z` (Redo), `Ctrl/Cmd + A` (Select All).
* **Browser Navigation Foundations**: `Ctrl/Cmd + T` (New Tab), `Ctrl/Cmd + W`
  (Close Tab), `Ctrl/Cmd + N` (New Window), `Ctrl/Cmd + L` (Focus Omnibox),
  `Ctrl/Cmd + R` (Reload), `Ctrl/Cmd + F` (Find), `Ctrl/Cmd + P` (Print),
  `Ctrl/Cmd + S` (Save Page), `Ctrl/Cmd + D` (Bookmark), `Ctrl/Cmd + Shift + N`
  (Incognito), `Ctrl/Cmd + Shift + T` (Restore Tab).

#### 7. DO NOT implement "Shadow Shortcuts"
* **Rule**: Never listen to raw `OnKeyPressed` / `ui::KeyEvent` in random views
  to implement a global feature shortcut. All browser shortcuts must be
  registered centrally in the accelerator table or ActionManager to ensure
  conflict detection, documentation, and accessibility remapping.

---

## Shortcut Lifecycle: Retiring & Swapping Shortcuts

Because shortcut real estate is finite, adding a high-priority shortcut often
requires **retiring a dormant shortcut** or **swapping key combinations**.

### Criteria for Retiring or Swapping a Shortcut
1. **Low Telemetry Usage**: The shortcut exhibits consistently low
   invocation frequency across 3 or more Chrome stable milestones via
   `Browser.Shortcuts.TriggeredCommandId` without qualifying as a critical
   accessibility lifeline, recovery mechanism, or specialized power-user
   workflow. Telemetry should demonstrate that reclaiming the shortcut causes
   minimal user disruption. (Note: Evaluation is case-by-case; standard
   invocation baseline thresholds are TBD).
2. **High Conflict Friction**: The shortcut collides with web standards or OS
   specific overrides.
3. **Feature Deprecation or Demotion**: The associated feature is being retired,
   redesigned, or moved to a secondary entry point (such as the 3-dot app menu
   or Omnibox Actions).

### The Retirement & Swapping Process

1. **Step 1: Baseline Telemetry Audit**
   * Inspect UMA histogram `Browser.Shortcuts.TriggeredCommandId` for the
     command's `IDC_*` value over at least 3 stable milestones.
   * Compare shortcut invocation volume against overall feature usage (e.g.,
     button clicks vs. keyboard triggers).

2. **Step 2: Submit an Intent to Implement to UI OWNERS**
   * File a Chromium bug under component `UI>Input>KeyboardShortcuts`.
   * Email `chrome-desktop-ui@google.com` outlining:
     - Proposed shortcut to retire or swap.
     - Telemetry data and usage rationale.
     - Replacement interaction (e.g., menu item, Omnibox action).

3. **Step 3: Phased Deprecation & User Notification**
   * **ChromeOS**: Add the entry to `kDeprecatedAccelerators` and
     `kDeprecatedAcceleratorsData` in `ash/accelerators/accelerator_table.h`.
     Ash will automatically show in-product notifications guiding users to the
     new shortcut and log `Ash.Accelerators.Deprecated.{ActionName}`.
   * **Desktop (Windows / Mac / Linux)**: If swapping a visible shortcut,
     provide a transition phase using a Finch feature flag and contextual User
     Education promos / toasts where appropriate.

4. **Step 4: Execute the Code Change**
   * Update `chrome/browser/ui/accelerator_table.cc` (or `ActionItem`).
   * Update Mac menu builders (`main_menu_builder.mm` /
     `global_keyboard_shortcuts_mac.mm`).
   * Update ChromeOS layout tables
     (`ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h`).
   * Update unit tests in `chrome/browser/ui/accelerator_table_unittest.cc`.
   * Update external documentation (Google Chrome Help Center).

---

## How to View & Measure Shortcut Usage (Telemetry)

Chrome automatically logs keyboard shortcut activations to UMA:

### Primary Metric: `Browser.Shortcuts.TriggeredCommandId`
* **Source Code**: Emitted in `BrowserView::UpdateAcceleratorMetrics` in
  `chrome/browser/ui/views/frame/browser_view.cc`:
  ```cpp
  if (!accelerator.IsRepeat()) {
    base::UmaHistogramSparse("Browser.Shortcuts.TriggeredCommandId",
                             command_id);
  }
  ```
* **Command ID Mapping**: Values correspond to `IDC_*` constants defined in
  `chrome/app/chrome_command_ids.h`.
* **Enum Definition**: `MappedChromeCommandId` in
  `tools/metrics/histograms/metadata/browser/enums.xml`.
* **Histogram Metadata**:
  `tools/metrics/histograms/metadata/browser/histograms.xml`.

### Querying Usage on Dashboards
1. Navigate to the Chrome UMA Dashboard (internal metric viewers).
2. Filter for histogram: `Browser.Shortcuts.TriggeredCommandId`.
3. **Filter by Chrome Milestone**: Because `IDC_*` integer values in
   `chrome_command_ids.h` can shift between milestones, always slice data by
   specific Chrome release milestones (e.g. M124, M125).
4. Identify your feature's `IDC_*` numeric value (from
   `chrome_command_ids.h`) to view absolute trigger count and relative volume.

### Feature-Specific User Actions
Specific shortcuts also emit dedicated `UserMetricsAction` events (e.g.,
`Accel_NewTabInGroup`, `Accel_NewIncognitoWindow`, `ExitFullscreen_Accelerator`,
`Accel_FocusLocation_D`). Consult `BrowserView::UpdateAcceleratorMetrics` for
existing actions or add one if detailed sequencing analysis is needed.

---

## Governance: Who to Reach Out to & Approval Workflow

### Approval Gatekeepers

| Surface | Approval Authority / OWNERS | Contact Channel |
| :--- | :--- | :--- |
| **Chrome Desktop UI (Windows, Mac, Linux, Views)** | `//chrome/browser/ui/OWNERS` | `top-chrome-desktop-ui@google.com`<br>`chrome-desktop-ui@google.com` |
| **ChromeOS System Shortcuts** | `//ash/accelerators/OWNERS` | `cros-device-enablement@google.com` |
| **macOS Menu & Cocoa Shortcuts** | `//chrome/browser/ui/cocoa/OWNERS` | `chrome-desktop-ui@google.com` |
| **Android Shortcuts** | `//chrome/android/java/src/org/chromium/chrome/browser/OWNERS` | `clank-dev@google.com` |
| **Accessibility (a11y) Review** | Chrome Accessibility Team | `chrome-accessibility@google.com` |

---

## Technical Implementation Guide Across Platforms

### Step 1: Define Command ID and ActionItem
1. Add `IDC_<FEATURE_NAME>` to `chrome/app/chrome_command_ids.h`.
2. Add `kAction<FeatureName>` to
   `chrome/browser/ui/actions/chrome_action_id.h` and register the action in
   `chrome/browser/ui/views/browser_actions.cc`.

### Step 2: Register Views Shortcut (Windows, Linux, ChromeOS)
* Edit `chrome/browser/ui/accelerator_table.cc`:
  Add entry to `kAcceleratorMap`:
  ```cpp
  {ui::VKEY_<KEY>, ui::EF_<MODIFIERS>, IDC_<FEATURE_NAME>},
  ```

### Step 3: Register macOS Shortcut
* **In Main Menu**: Add item with key equivalent in
  `chrome/browser/ui/cocoa/main_menu_builder.mm`.
* **Not in Main Menu**: Add to `GetShortcutsNotPresentInMainMenu()` in
  `chrome/browser/global_keyboard_shortcuts_mac.mm`.

### Step 4: Register ChromeOS Shortcut & Shortcuts App
* If system-level: Add to `ash/public/cpp/accelerator_actions.h` and
  `ash/accelerators/accelerator_table.h`.
* For Key Shortcuts App discovery: Add entry to
  `ash/webui/shortcut_customization_ui/backend/accelerator_layout_table.h`
  (and `kAcceleratorLayouts`).

### Step 5: Register Android Shortcut (if applicable)
* Add mapping to
  `chrome/android/java/src/org/chromium/chrome/browser/KeyboardShortcuts.java`.

### Step 6: Verify with Automated Tests
* Run `chrome/browser/ui/accelerator_table_unittest.cc`:
  Ensure `CheckDuplicatedAccelerators` and `DontUseKeysWithUnstablePositions`
  pass without failures.

---

## Alternatives to Adding a Global Keyboard Shortcut

Before proposing a global keyboard shortcut, consider whether an alternative
UX surface is more appropriate:

1. **Omnibox / Chrome Actions (Action Chips)**:
   * Ideal for discoverable, query-based actions (e.g. typing "clear cache",
     "manage passwords", "customize chrome").
2. **3-Dot App Menu & Submenus**:
   * Standard home for secondary features; exposes discoverability without
     consuming global key bindings.
3. **Context Menus (Right-Click)**:
   * Scoped directly to the relevant content (image, link, tab, selected
     text).
4. **View-Scoped Accelerators**:
   * If the shortcut only needs to work when a specific dialog, side panel, or
     view is focused, register it on that `views::View` or
     `views::FocusManager` locally rather than adding it to the global
     `kAcceleratorMap`.

---

## Frequently Asked Questions

#### Q1: Why can't every new feature have its own keyboard shortcut?
**A:** Global keyboard shortcuts are a shared, finite space. Overloading key
combinations causes internationalization breakage (AltGr), conflicts with web
applications (e.g. Google Docs, Figma), harms accessibility, and creates
cognitive overload for users.

#### Q2: Can I use `Ctrl + Alt + <Letter>` on Windows?
**A:** **No.** `Ctrl + Alt` is identical to `AltGr` on Windows and Linux, which
is required by international users to type normal characters and symbols (see
[Why Ctrl+Alt shouldn't be used as a shortcut
modifier](https://devblogs.microsoft.com/oldnewthing/20040329-00/?p=40003)).
Using `Ctrl + Alt` will break basic text typing for millions of users and is
blocked by automated code checks (`DCHECK`).

#### Q3: Why did `AcceleratorTableTest.DontUseKeysWithUnstablePositions` fail?
**A:** Punctuation keys (`[`, `]`, `/`, `\`, `;`, `'`, etc.) shift locations
across international keyboard layouts (AZERTY, QWERTZ, JIS) and frequently
require `Shift` or `AltGr`. Chrome guidelines restrict new shortcuts to
alphanumeric characters (`A–Z`, `0–9`) and standard navigation/function keys.

#### Q4: How does Chrome prioritize conflicting shortcuts with web pages?
**A:** Accelerators follow a defined routing hierarchy:
1. **OS Reserved** (e.g., power, lock on ChromeOS).
2. **Browser Reserved** (e.g., `Ctrl+W`, `Ctrl+N` on certain platforms).
3. **Web Contents** (focused web page gets the key event).
4. **Browser Non-Reserved / Post-Target** (if unhandled by the web page,
   browser executes the accelerator).

#### Q5: Can users customize their keyboard shortcuts?
**A:** On ChromeOS, users can customize shortcuts using the Key Shortcuts app
(`chrome://shortcut-customization`). On Desktop Chrome, this is not possible
natively at this time.
