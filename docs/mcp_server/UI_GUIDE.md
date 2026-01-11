# MCP Server - UI Implementation Guide

## Overview

This document details the Settings UI implementation for MCP Server, including Polymer components, TypeScript controllers, and localization.

**Location:** `chrome://settings/ai` → MCP Server section
**Framework:** Polymer 3.0 Web Components
**Language:** TypeScript + HTML Templates

---

## UI Architecture

### Component Hierarchy

```
settings-ui (root)
└── settings-main
    └── settings-ai-page ← MCP Server UI lives here
        ├── settings-section (AI features)
        └── settings-section (MCP Server) ← Our component
            ├── settings-toggle-button
            └── div.settings-box (connection info)
```

---

## File Structure

```
chrome/browser/resources/settings/
├── ai_page/
│   ├── ai_page.html              # Template with MCP UI
│   ├── ai_page.ts                # Controller with logic
│   └── ai_page.html.js           # Generated from .html
├── controls/
│   └── settings_toggle_button.js # Toggle component (existing)
└── settings_shared.css.js        # Shared styles (existing)

chrome/app/
└── settings_strings.grdp          # Localized strings

chrome/browser/ui/webui/settings/
└── settings_localized_strings_provider.cc  # String registration
```

---

## HTML Template Implementation

**File:** `chrome/browser/resources/settings/ai_page/ai_page.html`

### Full Component Code

```html
<!-- MCP Server Control Section -->
<settings-section page-title="$i18n{mcpServerSectionTitle}">
  <!-- Main Toggle Control -->
  <settings-toggle-button id="mcpServerToggle"
      pref="{{prefs.mcp_server.enabled}}"
      label="$i18n{mcpServerLabel}"
      sub-label="[[getMcpServerSubLabel_(mcpServerStatus_.running, mcpServerStatus_.port)]]">
  </settings-toggle-button>

  <!-- Connection Information (shown when server is running) -->
  <div class="settings-box" hidden="[[!mcpServerStatus_.running]]">
    <div class="start settings-box-text">
      $i18n{mcpServerConnectionInfo}
      <div>HTTP: http://localhost:[[mcpServerStatus_.port]]/mcp/tabs</div>
      <div>WebSocket: ws://127.0.0.1:[[mcpServerStatus_.port]]/ws</div>
    </div>
  </div>
</settings-section>
```

### Element Breakdown

#### 1. `<settings-section>`
- **Purpose:** Container for a logical group of settings
- **Attribute `page-title`:** Section heading (localized)
- **Used by:** All settings pages for consistent layout

#### 2. `<settings-toggle-button>`
- **Purpose:** Two-state control with preference binding
- **Attributes:**
  - `id`: Unique identifier for testing/debugging
  - `pref`: Two-way binding to preference (`{{prefs.mcp_server.enabled}}`)
  - `label`: Main text (localized)
  - `sub-label`: Status text (computed via method)

**Preference Binding:**
```html
pref="{{prefs.mcp_server.enabled}}"
```
- `{{...}}` = two-way data binding (Polymer syntax)
- Changes to toggle → updates pref automatically
- Changes to pref → updates toggle automatically
- No manual event handlers needed!

**Computed Sub-Label:**
```html
sub-label="[[getMcpServerSubLabel_(mcpServerStatus_.running, mcpServerStatus_.port)]]"
```
- `[[...]]` = one-way data binding (computed property)
- Calls `getMcpServerSubLabel_()` method
- Re-computes when `mcpServerStatus_.running` or `.port` changes
- Returns: "Running on port 9224" or "Stopped"

#### 3. Connection Info Box
```html
<div class="settings-box" hidden="[[!mcpServerStatus_.running]]">
```
- **Visibility:** Only shown when `mcpServerStatus_.running === true`
- **Class `settings-box`:** Standard Chrome Settings styling
- **Class `settings-box-text`:** Text content styling
- **Class `start`:** Left-alignment (flex layout)

**Dynamic Port Display:**
```html
<div>HTTP: http://localhost:[[mcpServerStatus_.port]]/mcp/tabs</div>
```
- Port number dynamically inserted from `mcpServerStatus_.port`
- Updates automatically when port changes

---

## TypeScript Controller Implementation

**File:** `chrome/browser/resources/settings/ai_page/ai_page.ts`

### Import Statements

```typescript
// Line 5-7
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import '../controls/settings_toggle_button.js';  // NEW: MCP Server toggle
import '../settings_page/settings_section.js';
```

**Why import toggle?**
- Polymer requires explicit imports for custom elements
- Ensures `<settings-toggle-button>` is registered before use
- Tree-shaking: Only imports what's actually used

### Class Definition

```typescript
export class SettingsAiPageElement extends SettingsAiPageElementBase {
  static get is() {
    return 'settings-ai-page';  // Custom element name
  }

  static get template() {
    return getTemplate();  // Returns compiled HTML template
  }

  static get properties() {
    return {
      // ... existing properties ...

      // MCP Server status (NEW)
      mcpServerStatus_: {
        type: Object,
        value: () => ({
          running: false,  // Is server currently running?
          port: 9224,      // Current or default port
        }),
      },
    };
  }

  // TypeScript property declaration (for type checking)
  declare private mcpServerStatus_: {running: boolean, port: number};
}
```

**Property Configuration:**
- `type: Object` - Polymer property type
- `value: () => ({...})` - Factory function for default value
  - **Important:** Use factory function, not object literal!
  - Each element instance gets its own object
  - Prevents shared state bugs

### Computed Method

```typescript
// Lines 238-243
private getMcpServerSubLabel_(running: boolean, port: number): string {
  if (running) {
    // Uses loadTimeData to format string with port number
    return loadTimeData.getStringF('mcpServerRunningOnPort', port);
  }
  return loadTimeData.getString('mcpServerStopped');
}
```

**Method Naming Convention:**
- Ends with `_` (underscore) → private method
- Parameters match property dependencies in template
- Polymer calls this automatically when dependencies change

**loadTimeData API:**
- `getString(key)` - Get simple string
- `getStringF(key, ...args)` - Get formatted string with placeholders
  - Replaces `$1`, `$2`, etc. with provided arguments
  - Thread-safe, locale-aware

---

## Polymer Data Binding

### Two-Way Binding (`{{...}}`)

```html
pref="{{prefs.mcp_server.enabled}}"
```

**Flow:**
1. User clicks toggle → fires `change` event
2. Polymer updates `prefs.mcp_server.enabled`
3. PrefsMixin saves to Chrome preference system
4. Preference change propagates back → toggle state updates

**Benefits:**
- Automatic synchronization
- No manual event listeners
- Preference persistence handled automatically

### One-Way Binding (`[[...]]`)

```html
sub-label="[[getMcpServerSubLabel_(mcpServerStatus_.running, mcpServerStatus_.port)]]"
hidden="[[!mcpServerStatus_.running]]"
```

**Flow:**
1. Property changes: `mcpServerStatus_.running = true`
2. Polymer detects change via dirty-checking
3. Re-computes all bindings that depend on this property
4. Updates DOM with new values

**Performance:**
- Only updates when dependencies actually change
- Batch updates during microtask (efficient)

### Property Observability

Polymer tracks changes via:
- **Property setters:** `this.mcpServerStatus_ = {...}` triggers observers
- **Deep observation:** For objects, use `this.set('mcpServerStatus_.running', true)`
- **Array mutation:** Use Polymer methods: `push()`, `splice()`, etc.

**Example:**
```typescript
// ❌ Wrong - won't trigger updates
this.mcpServerStatus_.running = true;

// ✅ Right - triggers Polymer observers
this.set('mcpServerStatus_.running', true);

// ✅ Also right - replaces entire object
this.mcpServerStatus_ = {running: true, port: 9224};
```

---

## Localization (i18n)

### String Definition (GRDP)

**File:** `chrome/app/settings_strings.grdp:4529-4544`

```xml
<!-- MCP Server Control -->
<message name="IDS_SETTINGS_MCP_SERVER_SECTION_TITLE"
         desc="Title for the MCP Server control section in AI innovations settings">
  MCP Server (Model Control Protocol)
</message>

<message name="IDS_SETTINGS_MCP_SERVER_LABEL"
         desc="Label for the MCP Server toggle">
  Enable MCP Server
</message>

<message name="IDS_SETTINGS_MCP_SERVER_STOPPED"
         desc="Sublabel when MCP Server is stopped">
  Stopped
</message>

<message name="IDS_SETTINGS_MCP_SERVER_RUNNING_ON_PORT"
         desc="Sublabel when MCP Server is running. $1 is the port number.">
  Running on port <ph name="PORT">$1<ex>9224</ex></ph>
</message>

<message name="IDS_SETTINGS_MCP_SERVER_CONNECTION_INFO"
         desc="Label for connection information display">
  Connection information:
</message>
```

**GRDP Format:**
- `name` - Unique identifier (C++ constant name)
- `desc` - Description for translators
- `<ph>` - Placeholder for dynamic values
  - `name="PORT"` - Semantic placeholder name
  - `$1` - Position in argument list
  - `<ex>9224</ex>` - Example value for translators

### String Registration (C++)

**File:** `chrome/browser/ui/webui/settings/settings_localized_strings_provider.cc:483-488`

```cpp
void AddAiStrings(content::WebUIDataSource* html_source) {
  static constexpr webui::LocalizedString kLocalizedStrings[] = {
    // ... other AI strings ...

    // MCP Server strings
    {"mcpServerSectionTitle", IDS_SETTINGS_MCP_SERVER_SECTION_TITLE},
    {"mcpServerLabel", IDS_SETTINGS_MCP_SERVER_LABEL},
    {"mcpServerStopped", IDS_SETTINGS_MCP_SERVER_STOPPED},
    {"mcpServerRunningOnPort", IDS_SETTINGS_MCP_SERVER_RUNNING_ON_PORT},
    {"mcpServerConnectionInfo", IDS_SETTINGS_MCP_SERVER_CONNECTION_INFO},
  };
  html_source->AddLocalizedStrings(kLocalizedStrings);
}
```

**Mapping:**
- First string: JavaScript key (camelCase)
- Second value: C++ constant (UPPER_SNAKE_CASE)
- Loaded into `loadTimeData` at page load

### Using Strings in UI

**HTML Template:**
```html
<!-- Direct substitution -->
$i18n{mcpServerLabel}

<!-- Equivalent to -->
[[loadTimeData.getString('mcpServerLabel')]]
```

**TypeScript:**
```typescript
// Simple string
loadTimeData.getString('mcpServerStopped')
// Returns: "Stopped"

// Formatted string (with placeholders)
loadTimeData.getStringF('mcpServerRunningOnPort', 9224)
// Returns: "Running on port 9224"

// Check if string exists
loadTimeData.valueExists('mcpServerLabel')
// Returns: true

// Get boolean
loadTimeData.getBoolean('showMcpServer')
```

---

## Styling

### CSS Classes Used

```html
<div class="settings-box">
  <div class="start settings-box-text">
```

**Class Reference:**

| Class | Purpose | Source |
|-------|---------|--------|
| `settings-section` | Section container | Polymer element |
| `settings-box` | Content box with padding | `settings_shared.css` |
| `settings-box-text` | Text content styling | `settings_shared.css` |
| `start` | Flex align-items: flex-start | `cr_shared_style.css` |

### Available Settings Styles

From `chrome/browser/resources/settings/settings_shared.css`:

```css
.settings-box {
  padding: 16px 20px;
  border-top: 1px solid var(--cr-separator-color);
}

.settings-box-text {
  color: var(--cr-secondary-text-color);
  font-size: 13px;
  line-height: 1.5;
}

/* Flex layout utilities */
.start {
  align-items: flex-start;
}

/* Spacing */
.hr {
  border-top: 1px solid var(--cr-separator-color);
}
```

**CSS Variables (from Chrome theme):**
- `--cr-separator-color` - Line color (#e0e0e0 in light mode)
- `--cr-secondary-text-color` - Subdued text (#5f6368)
- `--cr-primary-text-color` - Main text (#202124)

---

## Event Flow & User Interaction

### Toggle Click Sequence

1. **User clicks toggle**
   - Browser generates `click` event
   - `<settings-toggle-button>` captures event

2. **Toggle state change**
   - Toggle component updates internal `checked` state
   - Fires `settings-boolean-control-change` event
   - Updates bound preference via PrefsMixin

3. **Preference update**
   ```
   prefs.mcp_server.enabled = true
   ```
   - PrefsMixin calls `pref_service_->SetBoolean()`
   - Chrome persists to Local State file
   - Preference observers notified (future: start/stop server)

4. **UI updates**
   - Polymer re-computes `getMcpServerSubLabel_()`
   - Sub-label changes from "Stopped" to "Running on port 9224"
   - Connection info box becomes visible (`hidden=false`)

### Property Change Sequence

```typescript
// When server status updates (future implementation)
this.mcpServerStatus_ = {
  running: true,
  port: 9224
};
```

**Triggers:**
1. Polymer detects property change
2. Re-evaluates all bindings that depend on `mcpServerStatus_`:
   - `getMcpServerSubLabel_(mcpServerStatus_.running, mcpServerStatus_.port)`
   - `!mcpServerStatus_.running` (for visibility)
   - `mcpServerStatus_.port` (for URL display)
3. DOM updates with new computed values

---

## Testing the UI

### Manual Testing Steps

1. **Build Chrome**
   ```bash
   autoninja -C out/Default chrome
   ```

2. **Launch Chrome**
   ```bash
   ./out/Default/Chromium.app/Contents/MacOS/Chromium
   ```

3. **Navigate to Settings**
   - Address bar: `chrome://settings/ai`
   - OR: Settings menu → AI innovations

4. **Scroll to MCP Server Section**
   - Should appear below Tab organizer
   - Section title: "MCP Server (Model Control Protocol)"

5. **Test Toggle**
   - **Initial state:** Off, sublabel = "Stopped"
   - **Click toggle ON:**
     - Sublabel changes to "Running on port 9224"
     - Connection info box appears
     - Shows HTTP and WebSocket endpoints
   - **Click toggle OFF:**
     - Sublabel changes to "Stopped"
     - Connection info box disappears

6. **Test Persistence**
   - Enable MCP Server
   - Close Chrome completely
   - Relaunch Chrome
   - Navigate to `chrome://settings/ai`
   - **Expected:** MCP Server should still be enabled

### DevTools Inspection

**Open DevTools on Settings page:**
```
chrome://settings/ai
→ Right-click anywhere → Inspect
→ OR: F12 / Cmd+Option+I
```

**Check Polymer properties:**
```javascript
// In Console:
$0  // Currently selected element
$0.mcpServerStatus_
// → {running: false, port: 9224}

// Get ai-page element
document.querySelector('settings-ai-page')
// → <settings-ai-page>...</settings-ai-page>

// Check prefs
document.querySelector('settings-ai-page').prefs
// → {mcp_server: {enabled: {value: false}, port: {value: 9224}}}
```

**Monitor property changes:**
```javascript
const aiPage = document.querySelector('settings-ai-page');

// Observe property changes
aiPage.addEventListener('prefs-changed', (e) => {
  console.log('Preference changed:', e.detail);
});

// Manually trigger state change (for testing)
aiPage.mcpServerStatus_ = {running: true, port: 8080};
```

---

## Future Enhancements

### Planned UI Improvements

1. **Real-time status updates**
   - Poll server status every 2 seconds
   - Update `mcpServerStatus_` automatically
   - Show connection indicator (green dot when running)

2. **Port configuration**
   - Add number input for custom port
   - Validate port range (1024-65535)
   - Save to `mcp_server.port` preference

3. **Error states**
   - Show error message if server fails to start
   - Port already in use indicator
   - Retry button

4. **Advanced options**
   - Collapsible section for advanced settings
   - Enable/disable specific API endpoints
   - Authentication token configuration

### Implementation Example

```html
<!-- Future: Real-time status polling -->
<settings-section page-title="$i18n{mcpServerSectionTitle}">
  <settings-toggle-button id="mcpServerToggle"
      pref="{{prefs.mcp_server.enabled}}"
      label="$i18n{mcpServerLabel}"
      sub-label="[[getMcpServerSubLabel_(...)]]"
      on-settings-boolean-control-change="onMcpServerToggleChange_">
  </settings-toggle-button>

  <!-- Connection status indicator -->
  <div class="settings-box status-indicator" hidden="[[!mcpServerStatus_.running]]">
    <div class="status-dot" data-status="[[mcpServerStatus_.connectionStatus]]"></div>
    <span>[[getConnectionStatusText_(mcpServerStatus_.connectionStatus)]]</span>
  </div>

  <!-- Error message -->
  <div class="settings-box error-box" hidden="[[!serverError_]]">
    <cr-icon icon="cr:error"></cr-icon>
    <span>[[serverError_]]</span>
    <cr-button on-click="retryServerStart_">Retry</cr-button>
  </div>
</settings-section>
```

```typescript
// Future: Status polling
private statusPollInterval_: number|null = null;

override connectedCallback() {
  super.connectedCallback();
  this.startStatusPolling_();
}

override disconnectedCallback() {
  super.disconnectedCallback();
  this.stopStatusPolling_();
}

private startStatusPolling_() {
  this.updateMcpServerStatus_();
  this.statusPollInterval_ = window.setInterval(() => {
    this.updateMcpServerStatus_();
  }, 2000);  // Poll every 2 seconds
}

private async updateMcpServerStatus_() {
  // Call C++ backend to get actual server status
  const status = await this.browserProxy_.getMcpServerStatus();
  this.mcpServerStatus_ = status;
}
```

---

## Common Issues & Debugging

### Issue: Toggle doesn't update preference

**Symptoms:**
- Click toggle, but preference stays same
- No error in console

**Debug steps:**
1. Check preference binding syntax:
   ```html
   <!-- ✅ Correct -->
   pref="{{prefs.mcp_server.enabled}}"

   <!-- ❌ Wrong (one-way binding) -->
   pref="[[prefs.mcp_server.enabled]]"
   ```

2. Verify preference registration:
   ```cpp
   // chrome/browser/prefs/browser_prefs.cc
   registry->RegisterBooleanPref(prefs::kMCPServerEnabled, false);
   ```

3. Check PrefsMixin is applied:
   ```typescript
   const SettingsAiPageElementBase =
       PrefsMixin(PolymerElement);  // Must include PrefsMixin
   ```

### Issue: Strings not showing

**Symptoms:**
- UI shows `$i18n{mcpServerLabel}` literally
- Or shows key name instead of translated text

**Debug steps:**
1. Check GRDP syntax:
   ```xml
   <message name="IDS_SETTINGS_MCP_SERVER_LABEL" desc="...">
     Enable MCP Server
   </message>
   ```

2. Verify C++ registration:
   ```cpp
   {"mcpServerLabel", IDS_SETTINGS_MCP_SERVER_LABEL},
   ```

3. Check in Console:
   ```javascript
   loadTimeData.getString('mcpServerLabel')
   // Should return: "Enable MCP Server"
   ```

### Issue: Computed property not updating

**Symptoms:**
- Change `mcpServerStatus_`, but UI doesn't update
- Method not being called

**Debug steps:**
1. Check binding syntax:
   ```html
   <!-- ✅ Correct (function call) -->
   sub-label="[[getMcpServerSubLabel_(mcpServerStatus_.running, ...)]]"

   <!-- ❌ Wrong (property access) -->
   sub-label="[[getMcpServerSubLabel_]]"
   ```

2. Verify property is observable:
   ```typescript
   // ✅ Triggers Polymer
   this.mcpServerStatus_ = {running: true, port: 9224};

   // ❌ Doesn't trigger Polymer
   this.mcpServerStatus_.running = true;
   ```

3. Check method signature matches:
   ```typescript
   // Parameters must match binding order
   private getMcpServerSubLabel_(running: boolean, port: number): string {
     // ...
   }
   ```

---

## Resources

### Polymer Documentation
- [Polymer 3.0 Docs](https://polymer-library.polymer-project.org/3.0/docs/devguide/feature-overview)
- [Data Binding](https://polymer-library.polymer-project.org/3.0/docs/devguide/data-binding)
- [Properties](https://polymer-library.polymer-project.org/3.0/docs/devguide/properties)

### Chrome Settings Examples
- `chrome/browser/resources/settings/` - All settings pages
- `ai_page/` - Our implementation
- `appearance_page/` - Similar structure
- `privacy_page/` - More complex example

### Chromium UI Guidelines
- [WebUI Guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/webui_explainer.md)
- [Settings Best Practices](https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/resources/settings/README.md)

---

**Last Updated:** January 11, 2026
**Author:** MCP Server Team
**Status:** Week 1 Complete - UI Implementation
