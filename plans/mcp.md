# Agent Browser Protocol - MCP Server Specification

## Overview

The ABP MCP Server exposes browser control capabilities through the Model Context Protocol (MCP), enabling AI agents to interact with the browser using standardized tool calls. The MCP server acts as a bridge between MCP-compatible AI systems and the ABP REST API.

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    AI Agent / LLM                           │
└─────────────────────────┬───────────────────────────────────┘
                          │ MCP Protocol (stdio/SSE)
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   ABP MCP Server                            │
│              (Node.js / Python / Rust)                      │
└─────────────────────────┬───────────────────────────────────┘
                          │ HTTP/REST
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   ABP REST Server                           │
│              (Embedded in Chromium)                         │
└─────────────────────────────────────────────────────────────┘
```

## Transport Options

### stdio (Default)
Standard input/output for local process communication.

```bash
abp-mcp-server --transport stdio --abp-url http://localhost:9222
```

### SSE (Server-Sent Events)
HTTP-based transport for remote connections.

```bash
abp-mcp-server --transport sse --port 3000 --abp-url http://localhost:9222
```

## Configuration

### Environment Variables

```bash
ABP_URL=http://localhost:9222    # ABP REST server URL
ABP_AUTH_TOKEN=secret            # Optional auth token
ABP_MCP_PORT=3000                # Port for SSE transport
ABP_MCP_LOG_LEVEL=info           # Logging level
```

### Config File (`abp-mcp.json`)

```json
{
  "abp": {
    "url": "http://localhost:9222",
    "auth_token": null,
    "timeout_ms": 30000
  },
  "mcp": {
    "transport": "stdio",
    "port": 3000,
    "name": "abp-browser",
    "version": "1.0.0"
  },
  "features": {
    "screenshots": true,
    "network_interception": true,
    "javascript_execution": true
  }
}
```

---

## Standard Action Parameters

All action tools (tools that modify browser state) accept a standard `wait_until` parameter and return a standard response envelope.

### Wait Until Parameter

Every action tool accepts an optional `wait_until` object:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | string | `action_complete` | Wait condition type |
| `timeout_ms` | number | 30000 | Maximum wait time |
| `idle_time_ms` | number | 500 | Idle duration for `network_idle` |
| `duration_ms` | number | - | Fixed wait for `time` type |

**Wait types:**
- `immediate` - Return immediately after action dispatch
- `action_complete` - Wait for engine rendering/navigation lull (default)
- `network_idle` - Wait until no network activity
- `time` - Wait for fixed duration

### Screenshot Parameter

Every action tool accepts an optional `screenshot` object:

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `area` | string | `viewport` | `none` or `viewport` |
| `markup` | string | `none` | Element markup overlay |

**Markup values:**
- `none` - No element markup
- `interactive` - All interactive elements (clickable + typeable)
- `clickable` - Buttons, links, clickable elements
- `typeable` - Text inputs, textareas, contenteditable
- `inputs` - All form inputs

### Standard Action Response

All action tools return:

```json
{
  "result": { ... },
  "screenshot": {
    "data": "base64-encoded-webp",
    "format": "webp",
    "width": 1920,
    "height": 1080,
    "markup": "interactive",
    "marked_elements": [
      {
        "index": 0,
        "type": "button",
        "bounds": {"x": 100, "y": 200, "width": 80, "height": 32},
        "center": {"x": 140, "y": 216},
        "text": "Submit"
      }
    ]
  },
  "scroll": {
    "horizontal_percent": 0,
    "vertical_percent": 25.5,
    "page_width": 1920,
    "page_height": 4700
  },
  "events": [
    {
      "type": "event_type",
      "timestamp": 1699999999999,
      "data": { ... }
    }
  ],
  "timing": {
    "action_started": 1699999999000,
    "action_completed": 1699999999050,
    "wait_completed": 1699999999500,
    "total_ms": 500
  }
}
```

### Marked Elements

When `screenshot.markup` is not `none`, the response includes `marked_elements` with clickable coordinates:

| Field | Type | Description |
|-------|------|-------------|
| `index` | number | Element index (matches label drawn on screenshot) |
| `type` | string | `button`, `link`, `input`, `textarea`, `select`, `checkbox`, `radio` |
| `bounds` | object | Bounding box `{x, y, width, height}` |
| `center` | object | Center point `{x, y}` - use for clicking |
| `text` | string | Visible text content |
| `tag` | string | HTML tag name |
| `input_type` | string | Input type for `<input>` elements |
| `placeholder` | string | Placeholder text |
| `href` | string | Link URL for `<a>` elements |

The `scroll` object indicates the current scroll position as a percentage (0-100) for both axes, along with total page dimensions.

### Event Types

Events captured between action and wait completion:

| Type | Description |
|------|-------------|
| `navigation` | Tab navigated to new URL |
| `dialog` | Alert/confirm/prompt appeared |
| `file_chooser` | Native file picker opened |
| `file_selected` | Files were selected in file chooser |
| `file_chooser_cancelled` | File chooser dismissed without selection |
| `popup` | New window/tab opened |
| `tab_closed` | Tab was closed |
| `scroll` | Page was scrolled (includes delta and final position) |
| `download_started` | Download initiated |
| `download_completed` | Download finished successfully |

The `events` array allows agents to detect and respond to dialogs, file choosers, scrolls, downloads, and other browser events that occurred as a result of the action.

---

## MCP Tools

### Browser Management

#### `browser_get_info`
Get browser version and status information.

**Parameters:** None

**Returns:**
```json
{
  "browser": "ABP-Chromium",
  "version": "120.0.0.0",
  "abp_version": "1.0.0"
}
```

#### `browser_shutdown`
Gracefully shut down the browser.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `timeout_ms` | number | No | Shutdown timeout (default: 5000) |

---

### Tab Management

#### `browser_list_tabs`
List all open browser tabs.

**Parameters:** None

**Returns:**
```json
{
  "tabs": [
    {
      "id": "tab_abc123",
      "url": "https://example.com",
      "title": "Example Domain",
      "active": true
    }
  ]
}
```

#### `browser_new_tab`
Create a new browser tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `url` | string | No | URL to navigate to (default: blank) |
| `activate` | boolean | No | Make tab active (default: true) |

**Returns:**
```json
{
  "tab_id": "tab_xyz789",
  "url": "https://example.com"
}
```

#### `browser_close_tab`
Close a browser tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab to close |

#### `browser_switch_tab`
Switch to a specific tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab to activate |

#### `browser_get_tab_info`
Get detailed information about a tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab |

**Returns:**
```json
{
  "id": "tab_abc123",
  "url": "https://example.com",
  "title": "Example Domain",
  "loading": false,
  "can_go_back": true,
  "can_go_forward": false
}
```

#### `browser_duplicate_tab`
Create a copy of a tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab to duplicate |

#### `browser_move_tab`
Move tab to a new position.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab to move |
| `index` | number | Yes | New position index |

#### `browser_pin_tab`
Pin or unpin a tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab |
| `pinned` | boolean | No | Pin state (default: true) |

#### `browser_mute_tab`
Mute or unmute a tab.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | ID of tab |
| `muted` | boolean | No | Mute state (default: true) |

---

### Navigation

#### `browser_navigate`
Navigate to a URL.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `url` | string | Yes | URL to navigate to |
| `referrer` | string | No | Referrer URL |
| `wait_until` | object | No | Wait condition (see Standard Action Parameters) |

**Returns:** Standard action response. The `result` object contains:
```json
{
  "result": {
    "url": "https://example.com",
    "title": "Example Domain",
    "status_code": 200
  },
  "screenshot": { ... },
  "events": [ ... ],
  "timing": { ... }
}
```

#### `browser_go_back`
Navigate back in history.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |

#### `browser_go_forward`
Navigate forward in history.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |

#### `browser_reload`
Reload the current page.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `ignore_cache` | boolean | No | Bypass cache (default: false) |

#### `browser_stop`
Stop page loading.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |

---

### Mouse Actions

#### `browser_click`
Click at coordinates.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `x` | number | Yes | X coordinate |
| `y` | number | Yes | Y coordinate |
| `button` | string | No | `left`, `right`, `middle` (default: `left`) |
| `click_count` | number | No | 1=single, 2=double, 3=triple (default: 1) |
| `modifiers` | array | No | `["shift", "ctrl", "alt", "meta"]` |
| `wait_until` | object | No | Wait condition (see Standard Action Parameters) |

**Returns:** Standard action response with screenshot and events.

#### `browser_mouse_move`
Move mouse to coordinates.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `x` | number | Yes | X coordinate |
| `y` | number | Yes | Y coordinate |
| `steps` | number | No | Intermediate steps for smooth movement |

#### `browser_scroll`
Scroll the page.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `x` | number | No | X position to scroll at (default: center) |
| `y` | number | No | Y position to scroll at (default: center) |
| `delta_x` | number | No | Horizontal scroll amount |
| `delta_y` | number | No | Vertical scroll amount (negative = down) |

#### `browser_drag`
Drag from one position to another.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `from_x` | number | Yes | Start X coordinate |
| `from_y` | number | Yes | Start Y coordinate |
| `to_x` | number | Yes | End X coordinate |
| `to_y` | number | Yes | End Y coordinate |

#### `browser_hover`
Move mouse to coordinates and wait (trigger hover states).

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `x` | number | Yes | X coordinate |
| `y` | number | Yes | Y coordinate |
| `duration_ms` | number | No | Time to hover (default: 100) |

#### `browser_mouse_down`
Press mouse button without releasing.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `x` | number | Yes | X coordinate |
| `y` | number | Yes | Y coordinate |
| `button` | string | No | `left`, `right`, `middle` (default: `left`) |

#### `browser_mouse_up`
Release mouse button.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `x` | number | Yes | X coordinate |
| `y` | number | Yes | Y coordinate |
| `button` | string | No | `left`, `right`, `middle` (default: `left`) |

---

### Keyboard Actions

#### `browser_type`
Type text at current focus position.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `text` | string | Yes | Text to type |
| `delay_ms` | number | No | Delay between keystrokes |

#### `browser_press_key`
Press a single key.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `key` | string | Yes | Key to press (e.g., `Enter`, `Tab`, `Escape`) |
| `modifiers` | array | No | `["shift", "ctrl", "alt", "meta"]` |

#### `browser_shortcut`
Execute a keyboard shortcut.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `keys` | array | Yes | Keys to press together, e.g., `["ctrl", "a"]` |

#### `browser_key_down`
Press key without releasing.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `key` | string | Yes | Key to press |

#### `browser_key_up`
Release a pressed key.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `key` | string | Yes | Key to release |

#### `browser_insert_text`
Insert text directly without key events.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `text` | string | Yes | Text to insert |

---

### Page Content

#### `browser_get_page_content`
Get the page content.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `format` | string | No | `html`, `text`, or `markdown` (default: `text`) |

**Returns:**
```json
{
  "content": "Page text content...",
  "title": "Example Domain",
  "url": "https://example.com"
}
```

#### `browser_execute_javascript`
Execute JavaScript in the page context and retrieve results.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `expression` | string | Yes | JavaScript expression to evaluate |
| `await_promise` | boolean | No | Wait for promise resolution (default: true) |
| `timeout_ms` | number | No | Timeout for promises (default: 5000) |

**Returns:**
```json
{
  "value": 42,
  "type": "number"
}
```

**Example expressions:**
```javascript
// Count elements
"document.querySelectorAll('button').length"

// Check state
"window.scrollY"
"document.readyState"

// Extract data
"Array.from(document.querySelectorAll('h1')).map(h => h.textContent)"

// Application state
"window.APP_STATE?.user?.isLoggedIn ?? false"
```

**Note:** This is for reading page state and extracting data. All user interactions (clicks, typing) should use coordinate-based tools to maintain human-equivalent behavior.

---

### Screenshots

#### `browser_screenshot`
Take a screenshot of the page.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `full_page` | boolean | No | Capture full scrollable page |
| `region` | object | No | Capture specific region `{x, y, width, height}` |
| `format` | string | No | `png`, `jpeg`, `webp` (default: `webp`) |

**Returns:**
```json
{
  "image": "base64-encoded-image-data",
  "width": 1920,
  "height": 1080
}
```

---

### Wait Operations

#### `browser_wait_for_navigation`
Wait for navigation to complete.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `wait_until` | string | No | `load`, `domcontentloaded`, `networkidle` |
| `timeout_ms` | number | No | Maximum wait time |

#### `browser_wait_for_network_idle`
Wait for network activity to stop.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `idle_time_ms` | number | No | Required idle duration (default: 500) |
| `timeout_ms` | number | No | Maximum wait time |

---

### Dialog Handling

#### `browser_handle_dialog`
Handle an alert, confirm, or prompt dialog.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `action` | string | Yes | `accept` or `dismiss` |
| `prompt_text` | string | No | Text for prompt dialogs |

---

### Cookies

#### `browser_get_cookies`
Get cookies for the current page.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `url` | string | No | Filter by URL |

**Returns:**
```json
{
  "cookies": [
    {
      "name": "session_id",
      "value": "abc123",
      "domain": "example.com",
      "path": "/",
      "secure": true,
      "httpOnly": true
    }
  ]
}
```

#### `browser_set_cookie`
Set a cookie.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `name` | string | Yes | Cookie name |
| `value` | string | Yes | Cookie value |
| `domain` | string | No | Cookie domain |
| `path` | string | No | Cookie path |
| `secure` | boolean | No | Secure flag |
| `http_only` | boolean | No | HttpOnly flag |
| `expires` | number | No | Expiration timestamp |

#### `browser_clear_cookies`
Clear all cookies.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |

---

### Downloads

#### `browser_configure_downloads`
Configure download behavior.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `download_path` | string | Yes | Directory to save downloads |
| `prompt` | boolean | No | Show save dialog (default: false) |
| `overwrite` | boolean | No | Overwrite existing files (default: true) |

#### `browser_list_downloads`
List downloads.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `state` | string | No | Filter: `in_progress`, `completed`, `cancelled`, `failed` |
| `limit` | number | No | Max entries to return |

**Returns:**
```json
{
  "downloads": [
    {
      "id": "dl_123",
      "url": "https://example.com/file.pdf",
      "filename": "file.pdf",
      "path": "/downloads/file.pdf",
      "state": "completed",
      "bytes_received": 102400,
      "total_bytes": 102400
    }
  ]
}
```

#### `browser_get_download`
Get information about a specific download.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `download_id` | string | Yes | Download ID |

#### `browser_wait_for_download`
Wait for a download to start or complete.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `state` | string | No | `started` or `completed` (default: `completed`) |
| `timeout_ms` | number | No | Maximum wait time |

**Returns:**
```json
{
  "id": "dl_456",
  "path": "/downloads/report.pdf",
  "state": "completed"
}
```

#### `browser_cancel_download`
Cancel an in-progress download.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `download_id` | string | Yes | Download ID |

---

### File Chooser (Native Dialogs)

Handle native OS file picker dialogs that appear when clicking file inputs or "Save As" buttons.

#### `browser_set_file_chooser_files`
Set files to be selected when a file chooser dialog appears. Call this before triggering the dialog, or when a dialog is pending.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `files` | array | No* | File paths for open dialogs |
| `path` | string | No* | Save path for save dialogs |

*Use `files` for open dialogs, `path` for save dialogs.

**Example (open dialog):**
```json
{
  "tab_id": "tab_abc123",
  "files": ["/path/to/document.pdf", "/path/to/image.png"]
}
```

**Example (save dialog):**
```json
{
  "tab_id": "tab_abc123",
  "path": "/path/to/save/output.pdf"
}
```

#### `browser_configure_file_chooser`
Pre-configure automatic file selection for future dialogs.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |
| `auto_select` | boolean | No | Auto-select files without waiting |
| `default_files` | array | No | Default files for open dialogs |
| `default_save_path` | string | No | Default directory for save dialogs |

When `auto_select` is `true`, file choosers will automatically use the configured paths without waiting for explicit `browser_set_file_chooser_files` calls.

#### `browser_cancel_file_chooser`
Dismiss a pending file chooser without selecting files.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `tab_id` | string | Yes | Target tab ID |

---

### Window Management

#### `browser_get_window_info`
Get browser window information.

**Parameters:** None

**Returns:**
```json
{
  "state": "normal",
  "bounds": {"x": 0, "y": 0, "width": 1920, "height": 1080},
  "fullscreen": false,
  "minimized": false,
  "maximized": false
}
```

#### `browser_set_window_bounds`
Set browser window size and position.

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `x` | number | No | X position |
| `y` | number | No | Y position |
| `width` | number | No | Window width |
| `height` | number | No | Window height |

#### `browser_set_window_state`
Set window state (minimize, maximize, fullscreen).

**Parameters:**
| Name | Type | Required | Description |
|------|------|----------|-------------|
| `state` | string | Yes | `normal`, `minimized`, `maximized`, `fullscreen` |

---

## MCP Resources

The MCP server also exposes resources for reading browser state.

### `browser://tabs`
List of all open tabs.

### `browser://tabs/{tab_id}`
Current state of a specific tab.

### `browser://tabs/{tab_id}/content`
Current page content (text).

### `browser://tabs/{tab_id}/screenshot`
Current page screenshot (base64 PNG).

### `browser://cookies`
All browser cookies.

### `browser://downloads`
List of all downloads.

### `browser://downloads/{download_id}`
Specific download info and status.

---

## MCP Prompts

Pre-built prompts for common browser automation tasks.

### `browser_fill_form`
Guide for filling out a form on the page.

**Arguments:**
| Name | Type | Description |
|------|------|-------------|
| `tab_id` | string | Target tab |
| `form_data` | object | Field name to value mapping |

### `browser_extract_data`
Guide for extracting structured data from a page.

**Arguments:**
| Name | Type | Description |
|------|------|-------------|
| `tab_id` | string | Target tab |
| `schema` | object | JSON schema for expected data |

### `browser_navigate_and_act`
Guide for multi-step navigation and interaction.

**Arguments:**
| Name | Type | Description |
|------|------|-------------|
| `steps` | array | List of navigation/action steps |

---

## Error Handling

MCP errors are returned in standard MCP error format:

```json
{
  "code": -32000,
  "message": "Tab not found",
  "data": {
    "tab_id": "invalid_id",
    "abp_error_code": "TAB_NOT_FOUND"
  }
}
```

### Error Codes

| MCP Code | ABP Code | Description |
|----------|----------|-------------|
| -32000 | TAB_NOT_FOUND | Tab does not exist |
| -32001 | ELEMENT_NOT_FOUND | Element not found |
| -32002 | TIMEOUT | Operation timed out |
| -32003 | NAVIGATION_FAILED | Navigation failed |
| -32004 | EVALUATION_ERROR | JavaScript error |
| -32005 | FILE_CHOOSER_NOT_PRESENT | No file chooser to handle |
| -32006 | FILE_NOT_FOUND | File path does not exist |
| -32007 | DOWNLOAD_NOT_FOUND | Download ID not found |
| -32008 | DOWNLOAD_FAILED | Download could not complete |
| -32600 | INVALID_REQUEST | Malformed request |

---

## Usage Examples

### Claude Desktop Configuration

Add to `claude_desktop_config.json`:

```json
{
  "mcpServers": {
    "browser": {
      "command": "abp-mcp-server",
      "args": ["--transport", "stdio"],
      "env": {
        "ABP_URL": "http://localhost:9222"
      }
    }
  }
}
```

### Example Agent Interaction

```
User: Go to example.com and click the "More information" link

Agent: I'll navigate to example.com with interactive markup to find clickable elements.

[Calls browser_new_tab with url="https://example.com", screenshot={markup: "interactive"}]
→ Response includes:
  - screenshot: Shows example.com with bounding boxes on interactive elements
  - marked_elements: [
      {index: 0, type: "link", text: "More information", center: {x: 450, y: 320}}
    ]
  - scroll: {vertical_percent: 0}

I can see the page. Element [0] is the "More information" link at (450, 320).

[Calls browser_click with x=450, y=320]
→ Response includes:
  - screenshot: Shows the IANA page after navigation
  - events: [{"type": "navigation", "data": {"url": "https://iana.org/..."}}]

I've clicked the "More information" link.
The page has navigated to the IANA website with documentation about example domains.
```

### Multi-Step Workflow

```
User: Log into my account on example.com

Agent: I'll help you log in. Let me navigate to the login page with input markup.

[Calls browser_navigate with url="https://example.com/login", screenshot={markup: "inputs"}]
→ Response includes:
  - screenshot: Shows login form with bounding boxes on inputs
  - marked_elements: [
      {index: 0, type: "input", input_type: "email", placeholder: "Email", center: {x: 300, y: 200}},
      {index: 1, type: "input", input_type: "password", placeholder: "Password", center: {x: 300, y: 260}},
      {index: 2, type: "button", text: "Sign In", center: {x: 300, y: 320}}
    ]

I can see the login form with email [0], password [1], and submit button [2].
Please provide your username and I'll fill it in.

User: my username is john@example.com

[Calls browser_click with x=300, y=200]
→ Response shows email field focused

[Calls browser_type with text="john@example.com"]
→ Response shows email entered

[Calls browser_click with x=300, y=260]
→ Response shows password field focused

I've entered your email and the password field [1] is now focused.
For security, please type your password directly or let me know when you're ready to submit.
```

### Download File Example

```
User: Download the PDF report from example.com/report and save it to my Downloads folder

Agent: I'll download that report for you.

[Calls browser_configure_downloads with download_path="/home/user/Downloads"]
[Calls browser_navigate with url="https://example.com/report"]
→ Response shows page with "Download PDF" button at (500, 350)

[Calls browser_click with x=500, y=350, wait_until={type: "network_idle"}]
→ Response includes:
  - screenshot: Shows page after download initiated
  - events: [
      {"type": "download_started", "data": {"download_id": "dl_123", "filename": "report.pdf"}},
      {"type": "download_completed", "data": {"path": "/home/user/Downloads/report.pdf"}}
    ]

The report has been downloaded to /home/user/Downloads/report.pdf
```

### File Upload Example

```
User: Upload my resume to the job application form

Agent: I can see the form. There's an "Upload Resume" button at (400, 280).

[Calls browser_click with x=400, y=280]
→ Response includes:
  - screenshot: Shows the page with file chooser triggered
  - events: [{"type": "file_chooser", "data": {"chooser_type": "open", "pending": true}}]

The file chooser appeared. Now I'll provide the file path.

[Calls browser_set_file_chooser_files with files=["/home/user/Documents/resume.pdf"]]
→ Response includes:
  - screenshot: Shows the form with "resume.pdf" now selected
  - events: [{"type": "file_selected", "data": {"files": ["/home/user/Documents/resume.pdf"]}}]

Your resume has been uploaded to the form.
```

### Save Dialog Example

```
User: Export the chart and save it as chart.png on my desktop

Agent: I can see the "Export" button at (600, 150). I'll click it and handle the save dialog.

[Calls browser_set_file_chooser_files with path="/home/user/Desktop/chart.png"]
[Calls browser_click with x=600, y=150]
→ Response includes:
  - screenshot: Shows page after export
  - events: [{"type": "file_selected", "data": {"chooser_type": "save", "path": "/home/user/Desktop/chart.png"}}]

The chart has been saved to /home/user/Desktop/chart.png
```

---

## Implementation Notes

### State Management
- Tab IDs are persistent for the session
- Element IDs may become stale after navigation/DOM changes
- MCP server maintains mapping between friendly IDs and ABP IDs

### Concurrency
- One operation per tab at a time
- Multiple tabs can be controlled concurrently
- Long-running operations (navigation, waits) are async

### Security Considerations
- MCP server inherits ABP auth token requirement
- File upload paths are validated
- JavaScript evaluation can be disabled via config

---

## Related Documents

- [agent-browser-protocol.md](./agent-browser-protocol.md) - Core ABP architecture
- [API.md](./API.md) - REST API specification
