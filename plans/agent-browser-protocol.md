# Agent Browser Protocol - Technical Design Spec

## Overview

The Agent Browser Protocol (ABP) is a REST-based API that provides engine-native browser control for AI agents. Unlike CDP (Chrome DevTools Protocol) or browser extensions, ABP operates at the C++ engine level, offering direct access to browser internals with lower latency and greater capability.

For complete API reference, see [API.md](./API.md).

## Goals

- **Engine-Native Control**: Direct C++ integration bypassing JavaScript/extension sandboxes
- **REST Interface**: Simple HTTP-based API for agent consumption
- **Low Latency**: Minimal overhead for real-time agent interactions
- **Full Browser Access**: Capabilities beyond what CDP/extensions can provide
- **Human-Equivalent Input**: All keyboard and mouse actions a human can perform
- **Event-Driven Response**: Every action returns screenshot + events for agent awareness
- **Security Isolation**: Controlled access patterns for safe agent operation

## Non-Goals

- Compatibility with existing CDP tooling
- Browser extension support for ABP features
- Multi-tenant/shared browser scenarios (single agent per instance)

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      Agent (Client)                         │
└─────────────────────────┬───────────────────────────────────┘
                          │ HTTP/REST
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                   ABP REST Server                           │
│              (Embedded HTTP Server in Browser)              │
└─────────────────────────┬───────────────────────────────────┘
                          │ Direct C++ Calls
                          ▼
┌─────────────────────────────────────────────────────────────┐
│                  Browser Engine Core                        │
│  ┌───────────┐ ┌───────────┐ ┌───────────┐ ┌─────────────┐  │
│  │  Content  │ │   Blink   │ │  Network  │ │    Input    │  │
│  │   Shell   │ │  Renderer │ │   Stack   │ │   System    │  │
│  └───────────┘ └───────────┘ └───────────┘ └─────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. ABP Server (`//components/abp_server/`)

Embedded HTTP server handling REST requests. Built on Chromium's network stack.

**Responsibilities:**
- Listen on configurable port (default: 9222)
- Parse/validate REST requests
- Route to appropriate controllers
- Serialize responses as JSON

### 2. Browser Controller (`//components/abp_server/controllers/browser_controller`)

Global browser state and lifecycle management.

- Browser info and version
- Window management (bounds, minimize, maximize, fullscreen)
- Graceful shutdown

### 3. Tab Controller (`//components/abp_server/controllers/tab_controller`)

Manages tab lifecycle and meta-operations.

- Create, close, duplicate tabs
- Switch between tabs (activate)
- Get tab info (URL, title, loading state, favicon)
- Move, pin, mute tabs
- Navigation (URL, back, forward, reload, stop)

### 4. Mouse Controller (`//components/abp_server/controllers/mouse_controller`)

Engine-level mouse input injection replicating all human mouse actions.

- Click (left, right, middle button; single, double, triple click)
- Mouse down/up (for drag operations)
- Mouse move (with configurable steps for smooth movement)
- Drag and drop
- Scroll (wheel events with delta)
- Hover (move + wait for hover states)
- Modifier keys (Shift, Ctrl, Alt, Meta with clicks)

### 5. Keyboard Controller (`//components/abp_server/controllers/keyboard_controller`)

Engine-level keyboard input injection replicating all human keyboard actions.

- Type text (with configurable delay between keystrokes)
- Press key (single key down + up)
- Key down/up (for held keys)
- Keyboard shortcuts (modifier + key combinations)
- Raw text insertion (bypass key events for large text)
- Full key support (letters, numbers, F-keys, navigation, editing, special keys)

### 6. Content Controller (`//components/abp_server/controllers/content_controller`)

Page content extraction and JavaScript execution.

- Get page HTML/text/title/URL
- Execute JavaScript expressions and retrieve results
- Wait conditions (navigation, network idle)

Note: JavaScript execution is for reading state/extracting data. All interactions remain coordinate-based.

### 7. Screenshot Controller (`//components/abp_server/controllers/screenshot_controller`)

Visual capture at the engine level.

- Full viewport screenshot
- Full page screenshot (scrolled)
- Region screenshot
- Multiple formats (PNG, JPEG, WebP)

### 8. Network Controller (`//components/abp_server/controllers/network_controller`)

Network interception and monitoring.

- Request logging
- Request interception (pause, continue, fulfill, abort)
- Cookie management (get, set, delete)

### 9. Dialog Controller (`//components/abp_server/controllers/dialog_controller`)

Handle browser dialogs (alert, confirm, prompt).

- Get pending dialog info
- Accept/dismiss dialogs
- Provide prompt input

### 10. Download Controller (`//components/abp_server/controllers/download_controller`)

Manage file downloads at the engine level.

- Configure download path and behavior
- List and monitor downloads
- Wait for download completion
- Cancel/resume downloads

### 11. File Chooser Controller (`//components/abp_server/controllers/file_chooser_controller`)

Handle native OS file picker dialogs automatically.

- Pre-configure files for open dialogs
- Set save paths for save dialogs
- Auto-select mode for seamless file operations
- Cancel pending file choosers

### 12. Event Collector (`//components/abp_server/event_collector`)

Captures browser events during action execution for response envelope.

- Subscribe to navigation, dialog, file chooser, scroll events
- Track popup window/tab creation
- Monitor tab closures
- Track download start/completion
- Buffer events between action start and wait completion

### 13. Wait Controller (`//components/abp_server/wait_controller`)

Implements wait_until semantics for action completion detection.

- `action_complete` heuristic using engine signals
- `network_idle` monitoring via network stack
- Timeout handling
- Rendering quiescence detection via compositor

## Response Envelope Design

All action endpoints return a standard envelope containing:

1. **Result**: Action-specific return data
2. **Screenshot**: Compressed WebP image of viewport after wait completion
3. **Events**: Array of browser events that occurred between action and wait
4. **Timing**: Performance metrics for the action

### Wait Until Semantics

Actions accept a `wait_until` parameter controlling when to capture the response:

| Type | Description |
|------|-------------|
| `immediate` | Return right after action dispatch |
| `action_complete` | Wait for rendering/navigation lull (default) |
| `network_idle` | Wait for network quiescence |
| `time` | Fixed duration wait |

The `action_complete` heuristic monitors:
- Rendering quiescence (no pending paint/layout)
- Navigation settled (no pending loads/redirects)
- Script idle (no pending tasks/promises)
- Minimum delay after last activity

### Event Capture

Events captured during wait period inform agents of side effects:

- **navigation**: Page navigated to new URL
- **dialog**: Alert/confirm/prompt appeared
- **file_chooser**: Native file picker opened
- **file_selected**: Files were selected in file chooser
- **file_chooser_cancelled**: File chooser dismissed without selection
- **popup**: New window/tab opened
- **tab_closed**: Tab was closed
- **scroll**: Page was scrolled (includes delta, final position, source)
- **download_started**: Download initiated
- **download_completed**: Download finished successfully

### Scroll Position

Every action response includes current scroll state:

- **horizontal_percent / vertical_percent**: Position as percentage (0-100)
- **horizontal_px / vertical_px**: Position in pixels
- **page_width / page_height**: Total scrollable dimensions
- **viewport_width / viewport_height**: Visible viewport size

This design allows agents to:
1. See the visual result of every action
2. Know exact scroll position to understand visible content
3. Detect dialogs/file choosers requiring handling
4. Track file selection and download completion
5. Track navigation, scroll, and popup side effects

## Security Model

### Localhost Binding
ABP server binds to `127.0.0.1` only by default. Remote access requires explicit flag.

### Authentication
Optional bearer token authentication via `--abp-auth-token` flag.

### Request Limits
Rate limiting and request size limits to prevent resource exhaustion.

## Command Line Flags

```
--enable-abp                    Enable Agent Browser Protocol server
--abp-port=9222                 Port for ABP server (default: 9222)
--abp-auth-token=<token>        Require bearer token authentication
--abp-allow-remote              Allow non-localhost connections
--abp-cors-origin=<origin>      Set allowed CORS origin
```

## Implementation Phases

### Phase 1: Foundation
- [ ] Embedded HTTP server infrastructure
- [ ] Tab controller (create, close, list, switch, info)
- [ ] Basic navigation (URL, back, forward, reload)
- [ ] Screenshot capture

### Phase 2: Human Input
- [ ] Mouse controller (click, move, scroll, drag)
- [ ] Keyboard controller (type, press, shortcuts)
- [ ] Input modifiers and combinations

### Phase 3: DOM & Content
- [ ] DOM querying and element info
- [ ] Element interactions (click, type, focus)
- [ ] Content extraction (HTML, text)
- [ ] JavaScript evaluation

### Phase 4: Network & Dialogs
- [ ] Network request monitoring
- [ ] Request/response interception
- [ ] Cookie management
- [ ] Dialog handling

### Phase 5: Downloads & File Chooser
- [ ] Download configuration and management
- [ ] Download monitoring and wait conditions
- [ ] Native file chooser interception
- [ ] Auto-select mode for file dialogs

### Phase 6: Advanced
- [ ] Wait conditions
- [ ] Window management
- [ ] Performance metrics

## File Structure

```
//components/abp_server/
├── BUILD.gn
├── abp_server.h
├── abp_server.cc
├── http_server/
│   ├── http_server.h
│   ├── http_server.cc
│   ├── request_handler.h
│   ├── request_parser.h
│   └── response_builder.h
├── controllers/
│   ├── browser_controller.h
│   ├── browser_controller.cc
│   ├── tab_controller.h
│   ├── tab_controller.cc
│   ├── mouse_controller.h
│   ├── mouse_controller.cc
│   ├── keyboard_controller.h
│   ├── keyboard_controller.cc
│   ├── content_controller.h
│   ├── content_controller.cc
│   ├── screenshot_controller.h
│   ├── screenshot_controller.cc
│   ├── network_controller.h
│   ├── network_controller.cc
│   ├── dialog_controller.h
│   ├── dialog_controller.cc
│   ├── download_controller.h
│   ├── download_controller.cc
│   ├── file_chooser_controller.h
│   └── file_chooser_controller.cc
├── event_collector/
│   ├── event_collector.h
│   ├── event_collector.cc
│   ├── event_types.h
│   └── event_buffer.h
├── wait_controller/
│   ├── wait_controller.h
│   ├── wait_controller.cc
│   ├── action_complete_detector.h
│   ├── action_complete_detector.cc
│   └── network_idle_detector.h
└── util/
    ├── json_util.h
    ├── json_util.cc
    ├── response_builder.h
    └── response_builder.cc
```

## Dependencies

- `//net` - Network stack for HTTP server
- `//content/public/browser` - Browser-side content APIs
- `//third_party/blink/public` - Renderer interfaces
- `//components/download` - Download management
- `//ui/gfx` - Graphics/screenshot utilities
- `//ui/events` - Input event generation
- `//ui/shell_dialogs` - File chooser dialog handling
- `//cc` - Compositor for rendering quiescence detection
- `//third_party/libwebp` - WebP screenshot compression
- `//base` - Base utilities and threading

## Related Documents

- [API.md](./API.md) - Complete REST API specification
- [mcp.md](./mcp.md) - MCP server for AI agent integration
- [HTTP Server Implementation](./http-server-implementation.md) (TODO)
- [Input Injection Design](./input-injection-design.md) (TODO)
- [DOM Access Architecture](./dom-access-architecture.md) (TODO)
