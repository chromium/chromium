# MCP Server Documentation

## Overview

MCP (Model Control Protocol) Server provides HTTP and WebSocket APIs for programmatic browser control, enabling AI agents to interact with Chromium.

**Current Status:** Week 1 Complete ✅ | Week 2 In Progress 🔄

---

## Documentation Index

### Quick Start
- **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)** - Developer quick reference
  - File locations with line numbers
  - Common tasks and commands
  - Testing procedures
  - Quick code examples

### Implementation Guides
- **[IMPLEMENTATION.md](IMPLEMENTATION.md)** - Complete technical guide
  - Architecture overview
  - File structure
  - Core components
  - API documentation
  - Build configuration
  - Security considerations

- **[UI_GUIDE.md](UI_GUIDE.md)** - Settings UI deep dive
  - Polymer components
  - TypeScript controller
  - Data binding patterns
  - Localization (i18n)
  - Event flow
  - Debugging UI issues

### Change History
- **[CHANGES.md](CHANGES.md)** - Detailed change log
  - All files modified
  - Code snippets for each change
  - Statistics
  - Rollback procedure
  - Commit message template

---

## Quick Links

### Settings UI
- **Location:** `chrome://settings/ai`
- **Section:** MCP Server (Model Control Protocol)
- **Feature Flag:** `chrome://flags#mcp-server` (future)

### Key Files
| Component | File | Purpose |
|-----------|------|---------|
| Core Server | `chrome/browser/mcp_server/mcp_server.{h,cc}` | Singleton server class |
| UI Template | `chrome/browser/resources/settings/ai_page/ai_page.html` | Polymer HTML |
| UI Controller | `chrome/browser/resources/settings/ai_page/ai_page.ts` | TypeScript logic |
| Preferences | `chrome/common/pref_names.h` | Pref constants |
| Strings | `chrome/app/settings_strings.grdp` | Localized text |
| Unit Tests | `chrome/browser/mcp_server/mcp_server_unittest.cc` | Test cases |

### API Endpoints (Planned)
```
HTTP:      http://localhost:9224/mcp/*
WebSocket: ws://127.0.0.1:9224/ws
```

---

## Getting Started

### 1. Build Chrome

```bash
# Full build
autoninja -C out/Default chrome

# Incremental build (after changes)
autoninja -C out/Default chrome
```

### 2. Test Settings UI

```bash
# Run Chrome
./out/Default/Chromium.app/Contents/MacOS/Chromium

# Navigate to
chrome://settings/ai
```

### 3. Run Unit Tests

```bash
# Build tests
autoninja -C out/Default chrome/browser/mcp_server:unit_tests

# Run tests
out/Default/chrome/browser/mcp_server:unit_tests
```

---

## Documentation by Role

### For UI Developers
Start with **[UI_GUIDE.md](UI_GUIDE.md)**:
- Polymer component structure
- Data binding patterns
- TypeScript implementation
- Localization workflow
- Debugging techniques

Key sections:
- HTML Template Implementation
- TypeScript Controller Implementation
- Polymer Data Binding
- Localization (i18n)
- Testing the UI

### For Backend Developers
Start with **[IMPLEMENTATION.md](IMPLEMENTATION.md)**:
- C++ class structure
- Preference system
- Build configuration
- API architecture (future)

Key sections:
- Core Components
- Preferences Storage
- Build Configuration
- Security Considerations

### For New Contributors
Start with **[QUICK_REFERENCE.md](QUICK_REFERENCE.md)**:
- File locations table
- Key functions
- Common tasks
- Testing commands

Then read **[CHANGES.md](CHANGES.md)** to understand what was modified.

---

## Implementation Status

### ✅ Week 1: Foundation (Complete)
- [x] Project structure
- [x] Feature flag preparation
- [x] Settings UI in AI page
- [x] Preferences storage (enabled, port)
- [x] Unit tests (20 tests)
- [x] Build configuration
- [x] Comprehensive documentation

**Deliverables:**
- Core `MCPServer` class with lifecycle management
- Settings toggle in `chrome://settings/ai`
- Preference persistence across sessions
- Full test coverage
- 4 documentation files (2500+ lines)

### 🔄 Week 2: HTTP Server (In Progress)
- [ ] Implement HTTP server on port 9224
- [ ] WebSocket server setup
- [ ] API dispatcher/router
- [ ] Health check endpoint (`/mcp/health`)
- [ ] Basic request/response handling

**Deliverables:**
- Running HTTP server
- WebSocket connection support
- API routing infrastructure
- Health check endpoint

### 📋 Week 3-6: Features (Planned)

**Week 3: Tab Management**
- Tab listing (`GET /mcp/tabs`)
- Tab creation (`POST /mcp/tabs`)
- Tab navigation
- Tab closing
- Multi-window support

**Week 4: UI Interactions**
- Click, type, scroll actions
- DOM query API
- Screenshot capture
- Element inspection

**Week 5: Monitoring**
- Console log collection
- Network request tracing
- Real-time event streaming via WebSocket

**Week 6: Polish & Release**
- Integration tests
- API documentation
- Security audit
- Performance optimization
- Release preparation

---

## Code Statistics

### Lines of Code (Week 1)
| Component | LOC | Files |
|-----------|-----|-------|
| C++ Implementation | ~200 | 2 files |
| Unit Tests | ~200 | 1 file |
| TypeScript | ~30 | 1 file |
| HTML | ~15 | 1 file |
| Build Config | ~10 | 2 files |
| Strings/i18n | ~20 | 1 file |
| **Total Code** | **~475** | **8 files** |
| Documentation | ~2500 | 4 files |

### Test Coverage
- **Unit Tests:** 20 tests
- **Categories:** Lifecycle (7), Preferences (7), Validation (4), Lifecycle (2)
- **Coverage:** Core functionality, edge cases, error handling

---

## File Locations

### Source Code
```
chrome/browser/mcp_server/
├── mcp_server.h                   # Core class header
├── mcp_server.cc                  # Core class implementation
├── mcp_server_unittest.cc         # Unit tests
└── BUILD.gn                       # Build configuration

chrome/common/
└── pref_names.h                   # Preference constants

chrome/browser/prefs/
└── browser_prefs.cc               # Preference registration

chrome/browser/resources/settings/ai_page/
├── ai_page.html                   # UI template
└── ai_page.ts                     # UI controller

chrome/app/
└── settings_strings.grdp          # Localized strings

chrome/browser/ui/webui/settings/
└── settings_localized_strings_provider.cc  # String registration
```

### Documentation
```
docs/mcp_server/
├── README.md                      # This file
├── IMPLEMENTATION.md              # Technical implementation guide
├── UI_GUIDE.md                    # UI developer guide
├── QUICK_REFERENCE.md             # Quick reference
└── CHANGES.md                     # Change log
```

---

## Development Workflow

### Making Changes

1. **Modify code**
   ```bash
   # Edit files
   vim chrome/browser/mcp_server/mcp_server.cc
   ```

2. **Build**
   ```bash
   autoninja -C out/Default chrome
   ```

3. **Test**
   ```bash
   # Run unit tests
   out/Default/chrome/browser/mcp_server:unit_tests

   # Manual testing
   ./out/Default/Chromium.app/Contents/MacOS/Chromium
   ```

4. **Format code**
   ```bash
   git cl format
   ```

5. **Commit**
   ```bash
   git commit -m "Add feature X"
   ```

### Adding New Features

See detailed guides:
- **C++ changes:** [IMPLEMENTATION.md](IMPLEMENTATION.md#development-workflow)
- **UI changes:** [UI_GUIDE.md](UI_GUIDE.md#future-enhancements)
- **Quick tasks:** [QUICK_REFERENCE.md](QUICK_REFERENCE.md#common-tasks)

---

## Testing

### Unit Tests
```bash
# Build and run all tests
autoninja -C out/Default chrome/browser/mcp_server:unit_tests
out/Default/chrome/browser/mcp_server:unit_tests

# Run specific test
out/Default/chrome/browser/mcp_server:unit_tests \
  --gtest_filter=MCPServerTest.StartServer
```

### Manual UI Testing
1. Build Chrome
2. Navigate to `chrome://settings/ai`
3. Test MCP Server toggle
4. Verify preference persistence

### Integration Testing (Future)
- HTTP endpoint tests
- WebSocket connection tests
- End-to-end browser automation tests

---

## Troubleshooting

### Build Issues
See [IMPLEMENTATION.md - Troubleshooting](IMPLEMENTATION.md#troubleshooting)

### UI Issues
See [UI_GUIDE.md - Common Issues](UI_GUIDE.md#common-issues--debugging)

### Quick Fixes
See [QUICK_REFERENCE.md - Debugging Tips](QUICK_REFERENCE.md#debugging-tips)

---

## Contributing

### Code Style
- Follow [Chromium C++ Style Guide](https://chromium.googlesource.com/chromium/src/+/main/styleguide/c++/c++.md)
- Use `clang-format` for formatting
- Run `git cl format` before committing

### Testing Requirements
- Add unit tests for new C++ functionality
- Test UI changes manually in Chrome
- Verify preference persistence

### Documentation
- Update relevant .md files
- Add code comments for complex logic
- Keep CHANGES.md updated

---

## Resources

### Chromium Documentation
- [Chromium Development](https://www.chromium.org/developers/)
- [WebUI Guidelines](https://chromium.googlesource.com/chromium/src/+/main/docs/webui_explainer.md)
- [Settings Pages](https://chromium.googlesource.com/chromium/src/+/main/chrome/browser/resources/settings/README.md)

### Related Code
- Settings AI Page: `chrome/browser/resources/settings/ai_page/`
- Preferences: `chrome/common/pref_names.h`
- WebUI: `chrome/browser/ui/webui/`

---

## Contact & Support

For questions or issues:
1. Check documentation in this directory
2. Review IMPLEMENTATION.md troubleshooting section
3. File bug in Chromium issue tracker

---

**Last Updated:** January 11, 2026
**Maintainers:** MCP Server Team
**Status:** Week 1 Complete, Week 2 Starting
