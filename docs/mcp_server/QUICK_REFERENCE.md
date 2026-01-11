# MCP Server - Quick Reference Guide

## File Locations

### Core Implementation
| Component | File | Lines | Purpose |
|-----------|------|-------|---------|
| Server Class Header | `mcp_server.h` | 1-40 | Class definition, public API |
| Server Class Implementation | `mcp_server.cc` | 1-120 | Core logic, lifecycle management |
| Unit Tests | `mcp_server_unittest.cc` | 1-200 | 20 test cases |
| Build Config | `BUILD.gn` | 1-127 | GN build rules |

### Preferences
| Component | File | Lines | Purpose |
|-----------|------|-------|---------|
| Pref Constants | `chrome/common/pref_names.h` | 850-851 | Key definitions |
| Pref Registration | `chrome/browser/prefs/browser_prefs.cc` | 645-646 | Default values |

### UI Integration
| Component | File | Lines | Purpose |
|-----------|------|-------|---------|
| HTML Template | `chrome/browser/resources/settings/ai_page/ai_page.html` | 49-63 | UI markup |
| TypeScript Controller | `chrome/browser/resources/settings/ai_page/ai_page.ts` | 61-76, 238-243 | Logic & state |
| Localized Strings | `chrome/app/settings_strings.grdp` | 4529-4544 | i18n strings |
| String Registration | `chrome/browser/ui/webui/settings/settings_localized_strings_provider.cc` | 483-488 | C++ binding |

### Build Integration
| Component | File | Lines | Purpose |
|-----------|------|-------|---------|
| Browser Deps | `chrome/browser/BUILD.gn` | 1869 | Add mcp_server |
| Circular Includes | `chrome/browser/BUILD.gn` | 1577 | Allow circular dep |

---

## Key Functions

### MCPServer Class

```cpp
// Get singleton instance
MCPServer* server = MCPServer::GetInstance();

// Server lifecycle
server->Start();                    // Start on default port 9224
server->Start(8080);                // Start on custom port
server->Stop();                     // Stop the server
bool running = server->IsRunning(); // Check if running
int port = server->GetPort();       // Get current port

// Preferences
server->SetPrefService(pref_service);
bool enabled = server->IsEnabledInPrefs();
server->SetEnabledInPrefs(true);
server->SaveStateToPrefs();
```

### Preference Keys

```cpp
// In code
#include "chrome/common/pref_names.h"

prefs::kMCPServerEnabled  // "mcp_server.enabled"
prefs::kMCPServerPort     // "mcp_server.port"

// Usage
pref_service_->GetBoolean(prefs::kMCPServerEnabled);
pref_service_->GetInteger(prefs::kMCPServerPort);
pref_service_->SetBoolean(prefs::kMCPServerEnabled, true);
pref_service_->SetInteger(prefs::kMCPServerPort, 9224);
```

### UI Elements

```html
<!-- Toggle control (auto-binds to preference) -->
<settings-toggle-button
    pref="{{prefs.mcp_server.enabled}}"
    label="$i18n{mcpServerLabel}"
    sub-label="[[getMcpServerSubLabel_(...)]]">
</settings-toggle-button>

<!-- Connection info (shown when running) -->
<div hidden="[[!mcpServerStatus_.running]]">
  HTTP: http://localhost:[[mcpServerStatus_.port]]/mcp/tabs
  WebSocket: ws://127.0.0.1:[[mcpServerStatus_.port]]/ws
</div>
```

```typescript
// TypeScript property
declare private mcpServerStatus_: {running: boolean, port: number};

// Computed sublabel
private getMcpServerSubLabel_(running: boolean, port: number): string {
  if (running) {
    return loadTimeData.getStringF('mcpServerRunningOnPort', port);
  }
  return loadTimeData.getString('mcpServerStopped');
}
```

---

## String IDs

| ID | Key | Example Value |
|----|-----|---------------|
| `IDS_SETTINGS_MCP_SERVER_SECTION_TITLE` | `mcpServerSectionTitle` | "MCP Server (Model Control Protocol)" |
| `IDS_SETTINGS_MCP_SERVER_LABEL` | `mcpServerLabel` | "Enable MCP Server" |
| `IDS_SETTINGS_MCP_SERVER_STOPPED` | `mcpServerStopped` | "Stopped" |
| `IDS_SETTINGS_MCP_SERVER_RUNNING_ON_PORT` | `mcpServerRunningOnPort` | "Running on port 9224" |
| `IDS_SETTINGS_MCP_SERVER_CONNECTION_INFO` | `mcpServerConnectionInfo` | "Connection information:" |

---

## Testing Commands

```bash
# Build everything
autoninja -C out/Default chrome

# Build just MCP Server unit tests
autoninja -C out/Default chrome/browser/mcp_server:unit_tests

# Run unit tests
out/Default/chrome/browser/mcp_server:unit_tests

# Run specific test
out/Default/chrome/browser/mcp_server:unit_tests --gtest_filter=MCPServerTest.StartServer

# Run Chrome with MCP Server UI
./out/Default/Chromium.app/Contents/MacOS/Chromium

# Test with custom user data (clean profile)
./out/Default/Chromium.app/Contents/MacOS/Chromium --user-data-dir=/tmp/chrome-test
```

---

## Common Tasks

### Adding a New Preference

1. **Define constant** in `chrome/common/pref_names.h`:
   ```cpp
   inline constexpr char kMCPServerMyPref[] = "mcp_server.my_pref";
   ```

2. **Register** in `chrome/browser/prefs/browser_prefs.cc`:
   ```cpp
   registry->RegisterBooleanPref(prefs::kMCPServerMyPref, false);
   ```

3. **Use** in `mcp_server.cc`:
   ```cpp
   bool value = pref_service_->GetBoolean(prefs::kMCPServerMyPref);
   ```

### Adding a New UI String

1. **Add message** in `chrome/app/settings_strings.grdp`:
   ```xml
   <message name="IDS_SETTINGS_MCP_MY_STRING" desc="Description">
     My string text
   </message>
   ```

2. **Register** in `settings_localized_strings_provider.cc`:
   ```cpp
   {"myString", IDS_SETTINGS_MCP_MY_STRING},
   ```

3. **Use in HTML**:
   ```html
   $i18n{myString}
   ```

4. **Use in TypeScript**:
   ```typescript
   loadTimeData.getString('myString')
   ```

### Adding a New Method to MCPServer

1. **Declare** in `mcp_server.h`:
   ```cpp
   class MCPServer {
    public:
     void MyNewMethod();
   };
   ```

2. **Implement** in `mcp_server.cc`:
   ```cpp
   void MCPServer::MyNewMethod() {
     // Implementation
   }
   ```

3. **Add test** in `mcp_server_unittest.cc`:
   ```cpp
   TEST_F(MCPServerTest, MyNewMethod) {
     server_->MyNewMethod();
     // Assertions
   }
   ```

---

## Port Configuration

**Default Port:** 9224
**Valid Range:** 1024-65535 (unprivileged ports)
**Binding:** 127.0.0.1 (localhost only)

**Validation:**
```cpp
if (port < 1024 || port > 65535) {
  LOG(ERROR) << "Invalid port: " << port;
  return;
}
```

---

## Build Dependencies

```gn
deps = [
  "//base",                      # Chromium base library
  "//chrome/common:constants",   # Chrome constants
  "//components/prefs",           # Preferences system
  "//content/public/browser",     # Content layer APIs
]
```

**Test Dependencies:**
```gn
deps = [
  ":mcp_server",                         # The code under test
  "//base/test:test_support",            # Test utilities
  "//components/prefs:test_support",     # TestingPrefService
  "//content/test:test_support",         # Content test support
  "//testing/gtest",                     # Google Test framework
]
```

---

## URL Endpoints (Planned)

### Settings UI
- `chrome://settings/ai` - AI Innovations page with MCP Server toggle
- `chrome://flags#mcp-server` - Feature flag (future)

### API Endpoints (TODO: Week 2+)
- `http://localhost:9224/mcp/health` - Health check
- `http://localhost:9224/mcp/tabs` - Tab operations
- `ws://127.0.0.1:9224/ws` - WebSocket connection

---

## Debugging Tips

### Enable Verbose Logging
```cpp
#include "base/logging.h"

LOG(INFO) << "MCP Server starting on port " << port;
DLOG(INFO) << "Debug: " << variable;  // Only in debug builds
VLOG(1) << "Verbose log";             // Controlled by --v=1
```

### Check Preferences
```bash
# Chrome DevTools Console (chrome://settings)
chrome.settingsPrivate.getAllPrefs().filter(p => p.key.includes('mcp_server'))
```

### Inspect Settings UI
1. Open `chrome://settings/ai`
2. Right-click > Inspect
3. Check Console for JavaScript errors
4. Check Elements panel for DOM structure

### Test Preference Persistence
1. Enable MCP Server in UI
2. Close Chrome
3. Reopen Chrome
4. Navigate to `chrome://settings/ai`
5. Verify MCP Server is still enabled

---

## Implementation Checklist

### ✅ Week 1: Foundation (Complete)
- [x] Project structure
- [x] Feature flag
- [x] Settings UI in AI page
- [x] Preferences storage
- [x] Unit tests (20 tests)
- [x] Build configuration
- [x] Documentation

### 🔄 Week 2: HTTP Server (In Progress)
- [ ] HTTP server implementation
- [ ] WebSocket server
- [ ] API dispatcher/router
- [ ] Health check endpoint

### 📋 Week 3-6: Features (TODO)
- [ ] Tab management APIs
- [ ] UI interaction engine
- [ ] DOM query functionality
- [ ] Console log monitoring
- [ ] Network request tracing
- [ ] Integration tests
- [ ] API documentation
- [ ] Security review

---

## Useful Commands

```bash
# Format code
git cl format

# Run linter
git cl lint

# Create commit
git commit -m "Add MCP Server feature"

# Push for review
git cl upload

# Check build status
ninja -C out/Default -t query chrome/browser/mcp_server:mcp_server

# Clean build
rm -rf out/Default
gn gen out/Default
autoninja -C out/Default chrome
```

---

**Last Updated:** January 11, 2026
**Status:** Week 1 Complete, Ready for Week 2 HTTP Server Implementation
