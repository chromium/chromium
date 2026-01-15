# Agent Browser Protocol - API Specification

## Base URL

```
http://localhost:9222/api/v1
```

## Authentication

If `--abp-auth-token` is set, all requests must include:

```
Authorization: Bearer <token>
```

---

## Standard Request Envelope (Actions)

All action endpoints (POST/DELETE that modify state) accept standard parameters:

```json
{
  "action_params": { ... },
  "wait_until": {
    "type": "action_complete",
    "timeout_ms": 30000
  },
  "screenshot": {
    "area": "viewport",
    "markup": "interactive"
  }
}
```

### Screenshot Options

Control screenshot capture and element markup in the response.

#### Screenshot Area

| Value | Description |
|-------|-------------|
| `none` | No screenshot returned |
| `viewport` | Capture visible viewport (default) |

#### Screenshot Markup

Bounding boxes drawn over elements to help identify interactive targets:

| Value | Description |
|-------|-------------|
| `none` | No markup overlay (default) |
| `interactive` | All interactive elements (clickable + typeable) |
| `clickable` | Buttons, links, and other clickable elements |
| `typeable` | Text inputs, textareas, contenteditable elements |
| `inputs` | All form inputs (text, checkbox, radio, select, etc.) |

**Markup appearance:**
- Each marked element gets a colored bounding box
- Box colors indicate element type (e.g., blue for links, green for buttons, orange for inputs)
- Element index labels are drawn for reference
- Boxes are semi-transparent to not obscure content

**Example request with markup:**
```json
{
  "x": 100,
  "y": 200,
  "wait_until": {"type": "action_complete"},
  "screenshot": {
    "area": "viewport",
    "markup": "interactive"
  }
}
```

### Wait Until Types

| Type | Description |
|------|-------------|
| `immediate` | Return immediately after dispatching the action |
| `action_complete` | Wait for engine rendering/navigation lull after action (default) |
| `network_idle` | Wait until no network activity for `idle_time_ms` (default: 500ms) |
| `time` | Wait for a fixed duration specified by `duration_ms` |

### Wait Until Parameters

```json
{
  "wait_until": {
    "type": "action_complete",
    "timeout_ms": 30000,
    "idle_time_ms": 500,
    "duration_ms": 1000
  }
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `type` | string | `action_complete` | Wait condition type |
| `timeout_ms` | number | 30000 | Maximum time to wait before returning |
| `idle_time_ms` | number | 500 | Idle duration for `network_idle` type |
| `duration_ms` | number | - | Fixed wait for `time` type |

### Action Complete Heuristic

The `action_complete` type uses engine-level signals to detect completion:

1. **Rendering quiescence**: No pending paint/layout operations
2. **Navigation settled**: No pending navigations or redirects
3. **Script idle**: No pending JavaScript tasks or promises
4. **Minimum delay**: Configurable minimum wait (default: 100ms) after last activity

---

## Standard Response Envelope (Actions)

All action responses include a screenshot, scroll position, and event log:

```json
{
  "success": true,
  "data": {
    "result": { ... },
    "screenshot": {
      "data": "base64-encoded-image",
      "format": "webp",
      "width": 1920,
      "height": 1080,
      "timestamp": 1699999999999,
      "markup": "interactive",
      "marked_elements": [
        {
          "index": 0,
          "type": "button",
          "bounds": {"x": 100, "y": 200, "width": 80, "height": 32},
          "center": {"x": 140, "y": 216},
          "text": "Submit",
          "tag": "button",
          "role": "button"
        },
        {
          "index": 1,
          "type": "link",
          "bounds": {"x": 50, "y": 300, "width": 120, "height": 20},
          "center": {"x": 110, "y": 310},
          "text": "Learn more",
          "tag": "a",
          "href": "https://example.com/learn"
        },
        {
          "index": 2,
          "type": "input",
          "bounds": {"x": 200, "y": 150, "width": 250, "height": 36},
          "center": {"x": 325, "y": 168},
          "tag": "input",
          "input_type": "email",
          "placeholder": "Enter email"
        }
      ]
    },
    "scroll": {
      "horizontal_percent": 0,
      "vertical_percent": 25.5,
      "horizontal_px": 0,
      "vertical_px": 1200,
      "page_width": 1920,
      "page_height": 4700,
      "viewport_width": 1920,
      "viewport_height": 1080
    },
    "events": [
      {
        "type": "navigation",
        "timestamp": 1699999999100,
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
}
```

### Screenshot Object

| Field | Type | Description |
|-------|------|-------------|
| `data` | string | Base64-encoded image |
| `format` | string | Image format (`webp`, `png`, `jpeg`) |
| `width` | number | Image width in pixels |
| `height` | number | Image height in pixels |
| `timestamp` | number | Capture timestamp |
| `markup` | string | Markup mode used (`none`, `interactive`, etc.) |
| `marked_elements` | array | Elements marked in screenshot (when markup enabled) |

### Marked Elements

When `screenshot.markup` is not `none`, the response includes `marked_elements` array with clickable coordinates:

| Field | Type | Description |
|-------|------|-------------|
| `index` | number | Element index (matches label on screenshot) |
| `type` | string | Element type: `button`, `link`, `input`, `textarea`, `select`, `checkbox`, `radio` |
| `bounds` | object | Bounding box `{x, y, width, height}` |
| `center` | object | Center point `{x, y}` - use for clicking |
| `text` | string | Visible text content (if any) |
| `tag` | string | HTML tag name |
| `role` | string | ARIA role (if any) |
| `input_type` | string | Input type for `<input>` elements |
| `placeholder` | string | Placeholder text (if any) |
| `href` | string | Link URL for `<a>` elements |

### Scroll Position

The `scroll` object provides current scroll state after wait completion:

| Field | Type | Description |
|-------|------|-------------|
| `horizontal_percent` | number | Horizontal scroll position as percentage (0-100) |
| `vertical_percent` | number | Vertical scroll position as percentage (0-100) |
| `horizontal_px` | number | Horizontal scroll offset in pixels |
| `vertical_px` | number | Vertical scroll offset in pixels |
| `page_width` | number | Total scrollable page width in pixels |
| `page_height` | number | Total scrollable page height in pixels |
| `viewport_width` | number | Visible viewport width in pixels |
| `viewport_height` | number | Visible viewport height in pixels |

### Event Types

Events are captured between action dispatch and wait completion:

#### `navigation`
Tab navigated to a new URL.

```json
{
  "type": "navigation",
  "timestamp": 1699999999100,
  "data": {
    "tab_id": "tab_abc123",
    "url": "https://example.com/page",
    "navigation_type": "link_click"
  }
}
```

**navigation_type values:** `link_click`, `form_submit`, `redirect`, `back_forward`, `reload`

#### `dialog`
Browser dialog appeared (alert, confirm, prompt).

```json
{
  "type": "dialog",
  "timestamp": 1699999999200,
  "data": {
    "tab_id": "tab_abc123",
    "dialog_type": "confirm",
    "message": "Are you sure you want to delete?",
    "default_prompt": "",
    "pending": true
  }
}
```

**dialog_type values:** `alert`, `confirm`, `prompt`, `beforeunload`

#### `file_chooser`
Native file picker dialog appeared.

```json
{
  "type": "file_chooser",
  "timestamp": 1699999999300,
  "data": {
    "tab_id": "tab_abc123",
    "chooser_type": "open",
    "accepts": [{"description": "Images", "extensions": ["jpg", "png"]}],
    "multiple": false,
    "pending": true
  }
}
```

**chooser_type values:** `open`, `open_multiple`, `save`

#### `popup`
New popup window or tab opened.

```json
{
  "type": "popup",
  "timestamp": 1699999999400,
  "data": {
    "source_tab_id": "tab_abc123",
    "new_tab_id": "tab_xyz789",
    "url": "https://example.com/popup",
    "popup_type": "window"
  }
}
```

**popup_type values:** `window`, `tab`

#### `tab_closed`
Tab was closed.

```json
{
  "type": "tab_closed",
  "timestamp": 1699999999500,
  "data": {
    "tab_id": "tab_abc123",
    "reason": "script"
  }
}
```

**reason values:** `script`, `user`, `navigation`

#### `scroll`
Page was scrolled.

```json
{
  "type": "scroll",
  "timestamp": 1699999999550,
  "data": {
    "tab_id": "tab_abc123",
    "delta": {
      "x": 0,
      "y": 300,
      "direction": "down"
    },
    "position": {
      "horizontal_percent": 0,
      "vertical_percent": 45.2,
      "horizontal_px": 0,
      "vertical_px": 2100
    },
    "source": "wheel"
  }
}
```

**delta.direction values:** `up`, `down`, `left`, `right`

**source values:** `wheel`, `keyboard`, `script`, `drag`

#### `download_started`
Download was initiated.

```json
{
  "type": "download_started",
  "timestamp": 1699999999600,
  "data": {
    "download_id": "dl_123",
    "url": "https://example.com/file.pdf",
    "filename": "file.pdf",
    "mime_type": "application/pdf",
    "total_bytes": 102400
  }
}
```

#### `download_completed`
Download finished successfully.

```json
{
  "type": "download_completed",
  "timestamp": 1699999999900,
  "data": {
    "download_id": "dl_123",
    "path": "/downloads/file.pdf",
    "filename": "file.pdf",
    "bytes_received": 102400,
    "mime_type": "application/pdf"
  }
}
```

#### `file_selected`
Files were selected in a file chooser dialog.

```json
{
  "type": "file_selected",
  "timestamp": 1699999999700,
  "data": {
    "tab_id": "tab_abc123",
    "chooser_type": "open",
    "files": ["/path/to/document.pdf", "/path/to/image.png"]
  }
}
```

For save dialogs:
```json
{
  "type": "file_selected",
  "timestamp": 1699999999700,
  "data": {
    "tab_id": "tab_abc123",
    "chooser_type": "save",
    "path": "/path/to/output.pdf"
  }
}
```

#### `file_chooser_cancelled`
File chooser was dismissed without selection.

```json
{
  "type": "file_chooser_cancelled",
  "timestamp": 1699999999800,
  "data": {
    "tab_id": "tab_abc123",
    "chooser_type": "open"
  }
}
```

### Screenshot Configuration

Control screenshot capture via request headers or global config:

```
X-ABP-Screenshot-Format: webp
X-ABP-Screenshot-Quality: 80
X-ABP-Screenshot-Disabled: false
```

| Format | Quality Range | Notes |
|--------|---------------|-------|
| `webp` | 1-100 | Default, best compression |
| `jpeg` | 1-100 | Wide compatibility |
| `png` | N/A | Lossless, larger size |

---

## Query Response Format (GET endpoints)

GET endpoints return data without screenshot/events (status queries):

```json
{
  "success": true,
  "data": { ... }
}
```

---

## Error Response Format

```json
{
  "success": false,
  "error": {
    "code": "ERROR_CODE",
    "message": "Human readable message",
    "details": { ... }
  },
  "screenshot": {
    "data": "base64-encoded-image",
    "format": "webp",
    "width": 1920,
    "height": 1080,
    "timestamp": 1699999999999
  },
  "events": [ ... ]
}
```

Note: Even on error, screenshot and events are included to aid debugging.

---

## Browser Management

### Get Browser Info

```
GET /browser
```

Returns browser version and status information.

**Response:**
```json
{
  "success": true,
  "data": {
    "browser": "ABP-Chromium",
    "version": "120.0.0.0",
    "user_agent": "Mozilla/5.0 ...",
    "abp_version": "1.0.0",
    "uptime_ms": 34521
  }
}
```

### Shutdown Browser

```
POST /browser/shutdown
```

Gracefully shuts down the browser.

**Request:**
```json
{
  "timeout_ms": 5000
}
```

---

## Tab Management

### List All Tabs

```
GET /tabs
```

Returns all open tabs.

**Response:**
```json
{
  "success": true,
  "data": {
    "tabs": [
      {
        "id": "tab_abc123",
        "index": 0,
        "url": "https://example.com",
        "title": "Example Domain",
        "active": true,
        "pinned": false,
        "audible": false,
        "muted": false,
        "loading": false,
        "favicon_url": "https://example.com/favicon.ico"
      }
    ],
    "active_tab_id": "tab_abc123",
    "count": 1
  }
}
```

### Get Tab Info

```
GET /tabs/{tab_id}
```

Returns detailed information about a specific tab.

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "tab_abc123",
    "index": 0,
    "url": "https://example.com",
    "title": "Example Domain",
    "active": true,
    "pinned": false,
    "audible": false,
    "muted": false,
    "loading": false,
    "favicon_url": "https://example.com/favicon.ico",
    "can_go_back": true,
    "can_go_forward": false,
    "zoom_level": 1.0,
    "bounds": {
      "x": 0,
      "y": 0,
      "width": 1920,
      "height": 1080
    }
  }
}
```

### Create New Tab

```
POST /tabs
```

Creates a new tab.

**Request:**
```json
{
  "url": "https://example.com",
  "active": true,
  "index": 0
}
```

All fields optional. Defaults to blank tab at end, made active.

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "tab_xyz789",
    "index": 1,
    "url": "about:blank",
    "active": true
  }
}
```

### Close Tab

```
DELETE /tabs/{tab_id}
```

Closes the specified tab.

**Response:**
```json
{
  "success": true,
  "data": {
    "closed_tab_id": "tab_xyz789",
    "new_active_tab_id": "tab_abc123"
  }
}
```

### Activate Tab (Switch To)

```
POST /tabs/{tab_id}/activate
```

Switches to the specified tab.

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "tab_xyz789",
    "active": true
  }
}
```

### Move Tab

```
POST /tabs/{tab_id}/move
```

Moves tab to a new position.

**Request:**
```json
{
  "index": 2
}
```

### Pin/Unpin Tab

```
POST /tabs/{tab_id}/pin
```

```
POST /tabs/{tab_id}/unpin
```

### Mute/Unmute Tab

```
POST /tabs/{tab_id}/mute
```

```
POST /tabs/{tab_id}/unmute
```

### Duplicate Tab

```
POST /tabs/{tab_id}/duplicate
```

Creates a copy of the tab.

**Response:**
```json
{
  "success": true,
  "data": {
    "original_tab_id": "tab_abc123",
    "new_tab_id": "tab_def456"
  }
}
```

---

## Navigation

### Navigate to URL

```
POST /tabs/{tab_id}/navigate
```

**Request:**
```json
{
  "url": "https://example.com",
  "referrer": "https://google.com",
  "wait_until": {
    "type": "action_complete",
    "timeout_ms": 30000
  }
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "result": {
      "url": "https://example.com",
      "title": "Example Domain",
      "status_code": 200
    },
    "screenshot": {
      "data": "UklGRlYAAABXRUJQVlA4I...",
      "format": "webp",
      "width": 1920,
      "height": 1080,
      "timestamp": 1699999999500
    },
    "events": [
      {
        "type": "navigation",
        "timestamp": 1699999999100,
        "data": {
          "tab_id": "tab_abc123",
          "url": "https://example.com",
          "navigation_type": "link_click"
        }
      }
    ],
    "timing": {
      "action_started": 1699999999000,
      "action_completed": 1699999999100,
      "wait_completed": 1699999999500,
      "total_ms": 500
    }
  }
}
```

### Go Back

```
POST /tabs/{tab_id}/back
```

**Request:**
```json
{
  "wait_until": "load",
  "timeout_ms": 30000
}
```

### Go Forward

```
POST /tabs/{tab_id}/forward
```

**Request:**
```json
{
  "wait_until": "load",
  "timeout_ms": 30000
}
```

### Reload

```
POST /tabs/{tab_id}/reload
```

**Request:**
```json
{
  "ignore_cache": false,
  "wait_until": "load",
  "timeout_ms": 30000
}
```

### Stop Loading

```
POST /tabs/{tab_id}/stop
```

---

## Mouse Actions

### Click

```
POST /tabs/{tab_id}/mouse/click
```

Performs a mouse click at the specified coordinates.

**Request:**
```json
{
  "x": 100,
  "y": 200,
  "button": "left",
  "click_count": 1,
  "modifiers": [],
  "wait_until": {
    "type": "action_complete",
    "timeout_ms": 5000
  }
}
```

**button options:** `"left"`, `"right"`, `"middle"`

**modifiers options:** `"shift"`, `"ctrl"`, `"alt"`, `"meta"`

**click_count:** 1 for single click, 2 for double click, 3 for triple click

**Response (example showing dialog triggered by click):**
```json
{
  "success": true,
  "data": {
    "result": {
      "x": 100,
      "y": 200,
      "button": "left"
    },
    "screenshot": {
      "data": "UklGRlYAAABXRUJQVlA4I...",
      "format": "webp",
      "width": 1920,
      "height": 1080,
      "timestamp": 1699999999500
    },
    "events": [
      {
        "type": "dialog",
        "timestamp": 1699999999200,
        "data": {
          "tab_id": "tab_abc123",
          "dialog_type": "confirm",
          "message": "Delete this item?",
          "pending": true
        }
      }
    ],
    "timing": {
      "action_started": 1699999999000,
      "action_completed": 1699999999050,
      "wait_completed": 1699999999500,
      "total_ms": 500
    }
  }
}
```

### Mouse Down

```
POST /tabs/{tab_id}/mouse/down
```

Presses mouse button without releasing.

**Request:**
```json
{
  "x": 100,
  "y": 200,
  "button": "left",
  "modifiers": []
}
```

### Mouse Up

```
POST /tabs/{tab_id}/mouse/up
```

Releases mouse button.

**Request:**
```json
{
  "x": 100,
  "y": 200,
  "button": "left",
  "modifiers": []
}
```

### Mouse Move

```
POST /tabs/{tab_id}/mouse/move
```

Moves mouse to coordinates.

**Request:**
```json
{
  "x": 100,
  "y": 200,
  "steps": 10
}
```

**steps:** Number of intermediate mousemove events (for smooth movement)

### Drag and Drop

```
POST /tabs/{tab_id}/mouse/drag
```

Performs a drag operation from source to destination.

**Request:**
```json
{
  "from_x": 100,
  "from_y": 200,
  "to_x": 300,
  "to_y": 400,
  "steps": 20,
  "button": "left"
}
```

### Scroll (Wheel)

```
POST /tabs/{tab_id}/mouse/scroll
```

Performs mouse wheel scroll.

**Request:**
```json
{
  "x": 100,
  "y": 200,
  "delta_x": 0,
  "delta_y": -300,
  "modifiers": []
}
```

Negative `delta_y` scrolls down, positive scrolls up.

### Hover

```
POST /tabs/{tab_id}/mouse/hover
```

Moves mouse to coordinates and waits (triggers hover states).

**Request:**
```json
{
  "x": 100,
  "y": 200,
  "duration_ms": 100
}
```

---

## Keyboard Actions

### Type Text

```
POST /tabs/{tab_id}/keyboard/type
```

Types text as if entered by user. Generates keydown, keypress, and keyup events.

**Request:**
```json
{
  "text": "Hello, World!",
  "delay_ms": 50
}
```

**delay_ms:** Delay between keystrokes (0 for instant)

**Response:**
```json
{
  "success": true,
  "data": {
    "typed": "Hello, World!",
    "length": 13
  }
}
```

### Press Key

```
POST /tabs/{tab_id}/keyboard/press
```

Presses a single key (keydown + keyup).

**Request:**
```json
{
  "key": "Enter",
  "modifiers": []
}
```

**Common key values:**
- Letters: `"a"` - `"z"`, `"A"` - `"Z"`
- Numbers: `"0"` - `"9"`
- Function keys: `"F1"` - `"F12"`
- Navigation: `"ArrowUp"`, `"ArrowDown"`, `"ArrowLeft"`, `"ArrowRight"`
- Editing: `"Backspace"`, `"Delete"`, `"Enter"`, `"Tab"`, `"Escape"`
- Whitespace: `"Space"`
- Modifiers: `"Shift"`, `"Control"`, `"Alt"`, `"Meta"`
- Special: `"Home"`, `"End"`, `"PageUp"`, `"PageDown"`, `"Insert"`

### Key Down

```
POST /tabs/{tab_id}/keyboard/down
```

Presses key without releasing.

**Request:**
```json
{
  "key": "Shift",
  "modifiers": []
}
```

### Key Up

```
POST /tabs/{tab_id}/keyboard/up
```

Releases a pressed key.

**Request:**
```json
{
  "key": "Shift"
}
```

### Key Combination (Shortcut)

```
POST /tabs/{tab_id}/keyboard/shortcut
```

Performs a keyboard shortcut.

**Request:**
```json
{
  "keys": ["Control", "a"]
}
```

**Common shortcuts:**
- Select all: `["Control", "a"]`
- Copy: `["Control", "c"]`
- Paste: `["Control", "v"]`
- Cut: `["Control", "x"]`
- Undo: `["Control", "z"]`
- Redo: `["Control", "Shift", "z"]`
- Find: `["Control", "f"]`
- Save: `["Control", "s"]`
- New tab: `["Control", "t"]`
- Close tab: `["Control", "w"]`
- Refresh: `["Control", "r"]` or `["F5"]`

### Insert Text (Raw)

```
POST /tabs/{tab_id}/keyboard/insert
```

Inserts text directly without key events. Useful for pasting large amounts of text.

**Request:**
```json
{
  "text": "Large block of text..."
}
```

## Page Content

### Get Page HTML

```
GET /tabs/{tab_id}/content/html
```

**Query params:**
- `outer=true` - Include `<html>` tag (default: true)

### Get Page Text

```
GET /tabs/{tab_id}/content/text
```

Returns visible text content.

### Get Page Title

```
GET /tabs/{tab_id}/content/title
```

### Get Page URL

```
GET /tabs/{tab_id}/content/url
```

### Execute JavaScript

```
POST /tabs/{tab_id}/content/execute
```

Execute JavaScript in the page context and retrieve results.

**Request:**
```json
{
  "expression": "document.querySelectorAll('a').length",
  "await_promise": true,
  "timeout_ms": 5000
}
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `expression` | string | required | JavaScript expression to evaluate |
| `await_promise` | boolean | true | Wait for promise resolution if result is a promise |
| `timeout_ms` | number | 5000 | Timeout for promise resolution |

**Response:**
```json
{
  "success": true,
  "data": {
    "result": {
      "value": 42,
      "type": "number"
    }
  }
}
```

**Supported return types:**
- Primitives: `string`, `number`, `boolean`, `null`, `undefined`
- Objects: Serialized as JSON (must be JSON-serializable)
- Arrays: Serialized as JSON array
- Promises: Resolved value returned (if `await_promise` is true)

**Example expressions:**
```javascript
// Get page data
"document.title"
"window.location.href"
"document.body.innerText.length"

// Query counts
"document.querySelectorAll('button').length"
"document.forms.length"

// Check state
"document.readyState"
"window.scrollY"

// Extract data
"JSON.stringify(Array.from(document.querySelectorAll('h1')).map(h => h.textContent))"

// Application state
"window.APP_STATE?.user?.isLoggedIn ?? false"
```

---

## Screenshots

### Full Page Screenshot

```
GET /tabs/{tab_id}/screenshot
```

**Query params:**
- `format=png` - Image format: `png`, `jpeg`, `webp`
- `quality=80` - Quality for jpeg/webp (1-100)
- `full_page=false` - Capture full scrollable page

**Response:** Binary image data with appropriate Content-Type header.

### Screenshot to Base64

```
POST /tabs/{tab_id}/screenshot
```

**Request:**
```json
{
  "format": "png",
  "quality": 80,
  "full_page": false,
  "encoding": "base64"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "image": "iVBORw0KGgo...",
    "width": 1920,
    "height": 1080
  }
}
```

### Region Screenshot

```
POST /tabs/{tab_id}/screenshot/region
```

**Request:**
```json
{
  "x": 0,
  "y": 0,
  "width": 800,
  "height": 600,
  "format": "png",
  "encoding": "base64"
}
```

---

## Network

### Get Network Log

```
GET /tabs/{tab_id}/network/requests
```

**Query params:**
- `limit=100` - Max entries to return
- `type=xhr` - Filter by resource type

**Response:**
```json
{
  "success": true,
  "data": {
    "requests": [
      {
        "id": "req_123",
        "url": "https://api.example.com/data",
        "method": "GET",
        "status": 200,
        "type": "xhr",
        "start_time": 1699999999000,
        "duration_ms": 145,
        "request_headers": {},
        "response_headers": {},
        "size_bytes": 1234
      }
    ]
  }
}
```

### Enable Request Interception

```
POST /tabs/{tab_id}/network/intercept
```

**Request:**
```json
{
  "patterns": [
    {
      "url_pattern": "*://api.example.com/*",
      "resource_types": ["xhr", "fetch"],
      "action": "pause"
    }
  ]
}
```

### Get Intercepted Requests

```
GET /tabs/{tab_id}/network/intercepted
```

### Continue Intercepted Request

```
POST /tabs/{tab_id}/network/intercepted/{request_id}/continue
```

**Request:**
```json
{
  "url": "https://modified-url.com",
  "method": "POST",
  "headers": {
    "X-Custom-Header": "value"
  }
}
```

### Fulfill Intercepted Request

```
POST /tabs/{tab_id}/network/intercepted/{request_id}/fulfill
```

**Request:**
```json
{
  "status": 200,
  "headers": {
    "Content-Type": "application/json"
  },
  "body": "{\"mocked\": true}"
}
```

### Abort Intercepted Request

```
POST /tabs/{tab_id}/network/intercepted/{request_id}/abort
```

---

## Cookies

### Get All Cookies

```
GET /tabs/{tab_id}/cookies
```

### Get Cookies for URL

```
GET /tabs/{tab_id}/cookies?url=https://example.com
```

### Set Cookie

```
POST /tabs/{tab_id}/cookies
```

**Request:**
```json
{
  "name": "session_id",
  "value": "abc123",
  "domain": "example.com",
  "path": "/",
  "secure": true,
  "http_only": true,
  "same_site": "Strict",
  "expires": 1699999999
}
```

### Delete Cookie

```
DELETE /tabs/{tab_id}/cookies/{cookie_name}
```

**Query params:**
- `domain=example.com`
- `path=/`

### Clear All Cookies

```
DELETE /tabs/{tab_id}/cookies
```

---

## Dialogs (Alerts, Confirms, Prompts)

### Get Pending Dialog

```
GET /tabs/{tab_id}/dialog
```

**Response:**
```json
{
  "success": true,
  "data": {
    "present": true,
    "type": "confirm",
    "message": "Are you sure you want to delete this item?",
    "default_prompt": ""
  }
}
```

### Accept Dialog

```
POST /tabs/{tab_id}/dialog/accept
```

**Request:**
```json
{
  "prompt_text": "User input for prompt dialogs"
}
```

### Dismiss Dialog

```
POST /tabs/{tab_id}/dialog/dismiss
```

---

## Wait Conditions

### Wait for Navigation

```
POST /tabs/{tab_id}/wait/navigation
```

**Request:**
```json
{
  "wait_until": "load",
  "timeout_ms": 30000
}
```

### Wait for Network Idle

```
POST /tabs/{tab_id}/wait/network-idle
```

**Request:**
```json
{
  "idle_time_ms": 500,
  "timeout_ms": 30000
}
```

---

## Window Management

### Get Window Info

```
GET /window
```

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "window_1",
    "state": "normal",
    "bounds": {
      "x": 0,
      "y": 0,
      "width": 1920,
      "height": 1080
    },
    "fullscreen": false,
    "minimized": false,
    "maximized": false
  }
}
```

### Set Window Bounds

```
POST /window/bounds
```

**Request:**
```json
{
  "x": 100,
  "y": 100,
  "width": 1280,
  "height": 720
}
```

### Minimize Window

```
POST /window/minimize
```

### Maximize Window

```
POST /window/maximize
```

### Fullscreen

```
POST /window/fullscreen
```

### Restore Window

```
POST /window/restore
```

---

## Downloads

### Configure Download Behavior

```
POST /downloads/config
```

Configure how downloads are handled.

**Request:**
```json
{
  "download_path": "/path/to/downloads",
  "prompt": false,
  "overwrite": true
}
```

**prompt:** If `false`, downloads automatically save to `download_path`. If `true`, will wait for file chooser handling.

### List Downloads

```
GET /downloads
```

**Query params:**
- `state=in_progress` - Filter by state: `in_progress`, `completed`, `cancelled`, `failed`
- `limit=100` - Max entries

**Response:**
```json
{
  "success": true,
  "data": {
    "downloads": [
      {
        "id": "dl_123",
        "url": "https://example.com/file.pdf",
        "filename": "file.pdf",
        "path": "/downloads/file.pdf",
        "state": "completed",
        "bytes_received": 102400,
        "total_bytes": 102400,
        "mime_type": "application/pdf",
        "start_time": 1699999999000,
        "end_time": 1699999999500
      }
    ]
  }
}
```

### Get Download Info

```
GET /downloads/{download_id}
```

Returns detailed information about a specific download.

### Wait for Download

```
POST /downloads/wait
```

Wait for a download to start or complete.

**Request:**
```json
{
  "timeout_ms": 30000,
  "state": "completed"
}
```

**state options:** `"started"`, `"completed"`

**Response:**
```json
{
  "success": true,
  "data": {
    "id": "dl_456",
    "path": "/downloads/report.pdf",
    "state": "completed",
    "bytes_received": 204800
  }
}
```

### Cancel Download

```
POST /downloads/{download_id}/cancel
```

### Resume Download

```
POST /downloads/{download_id}/resume
```

Resume a paused download.

### Delete Download

```
DELETE /downloads/{download_id}
```

**Query params:**
- `delete_file=false` - Also delete the downloaded file

---

## File Chooser (Native File Dialogs)

Handle native OS file picker dialogs that appear when clicking file inputs or save buttons.

### Get Pending File Chooser

```
GET /tabs/{tab_id}/file-chooser
```

Check if a file chooser dialog is pending.

**Response:**
```json
{
  "success": true,
  "data": {
    "present": true,
    "type": "open",
    "accepts": [
      {
        "description": "Images",
        "extensions": ["jpg", "png", "gif"]
      }
    ],
    "multiple": false
  }
}
```

**type options:** `"open"`, `"open-multiple"`, `"save"`

### Set File Chooser Files

```
POST /tabs/{tab_id}/file-chooser/select
```

Automatically select files when a file chooser appears. Can be called before triggering the dialog.

**Request (for open dialogs):**
```json
{
  "files": [
    "/path/to/document.pdf",
    "/path/to/image.png"
  ]
}
```

**Request (for save dialogs):**
```json
{
  "path": "/path/to/save/output.pdf"
}
```

**Response:**
```json
{
  "success": true,
  "data": {
    "files_selected": ["/path/to/document.pdf"],
    "dialog_closed": true
  }
}
```

### Cancel File Chooser

```
POST /tabs/{tab_id}/file-chooser/cancel
```

Dismiss the file chooser without selecting files.

### Set Default File Chooser Behavior

```
POST /tabs/{tab_id}/file-chooser/config
```

Pre-configure automatic file selection for future dialogs.

**Request:**
```json
{
  "auto_select": true,
  "default_files": ["/path/to/default/file.txt"],
  "default_save_path": "/path/to/saves/"
}
```

When `auto_select` is `true`, file choosers will automatically use the configured files without waiting for explicit handling.

---

## Error Codes

| Code | Description |
|------|-------------|
| `INVALID_REQUEST` | Malformed request body |
| `TAB_NOT_FOUND` | Tab ID does not exist |
| `ELEMENT_NOT_FOUND` | Element ID does not exist or is stale |
| `SELECTOR_NOT_FOUND` | No element matches selector |
| `TIMEOUT` | Operation timed out |
| `NAVIGATION_FAILED` | Navigation could not complete |
| `ELEMENT_NOT_VISIBLE` | Element exists but is not visible |
| `ELEMENT_NOT_INTERACTABLE` | Element cannot receive input |
| `DIALOG_NOT_PRESENT` | No dialog to accept/dismiss |
| `FILE_CHOOSER_NOT_PRESENT` | No file chooser dialog to handle |
| `FILE_NOT_FOUND` | Specified file path does not exist |
| `FILE_NOT_READABLE` | File exists but cannot be read |
| `DOWNLOAD_NOT_FOUND` | Download ID does not exist |
| `DOWNLOAD_FAILED` | Download could not complete |
| `NETWORK_ERROR` | Network operation failed |
| `EVALUATION_ERROR` | JavaScript evaluation failed |
| `UNAUTHORIZED` | Missing or invalid auth token |
| `RATE_LIMITED` | Too many requests |
