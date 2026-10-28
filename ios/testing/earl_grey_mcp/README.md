# Dynamic Model Context Protocol (MCP) Driver for iOS (`ios/chrome` & `ios/web`)

This directory (`//ios/testing/earl_grey_mcp/`) implements a unified Model
Context Protocol (MCP) driver target (`ios_mcp_driver`) that wraps an iOS
Simulator build of either Chrome for iOS or `ios_web_shell` and dynamically
exposes its testing interface—including test-side `*EarlGreyImpl` /
`*EarlGreyUI` helpers, core matcher/action classes, and feature `*AppInterface`
classes (85 total classes and 1,266 methods across 13 domains at the time of
writing)—as strongly-typed MCP tools.

The driver automatically determines its execution mode from the target name:

- **XCUITest Runner Targets (`chrome_eg_mcp`, `ios_web_shell_eg_mcp`)**: Boot an
  XCUITest runner harness (`*_eg_mcp_module-Runner.app`) paired over eDO with a
  test-host application (`*_eg2tests.app`), providing access to all test
  helpers, EarlGrey synchronization, and out-of-process system alerts.
- **Standalone Targets with Clean eDO Side-Car (`chrome_mcp`,
  `ios_web_shell_mcp`)**: Boot a standalone application (`*_mcp.app`) in ~1
  second via `simctl` alongside a companion side-car process. The application
  runs near-production hooks (`tests_fake_hook.mm`) without EarlGrey, without
  runloop swizzling, and without test-harness fakes, communicating with the
  side-car over a minimal `EDOHostService` loopback channel.

## 1. Design Goals & Constraints

1. **Zero Hard-Coded EarlGrey Interface Knowledge**: The MCP server must not
   hard-code Chrome's or `ios_web_shell`'s EarlGrey classes, selectors,
   matchers, or actions. Any newly added or modified bridged `*AppInterface` or
   `*EarlGreyImpl` method in the codebase becomes available automatically.
2. **Hybrid Build-Time + Run-Time Interface Inference**:
   - **Build-Time Header Metadata (`extract_eg_metadata.py`)**: Scans bridged
     EarlGrey and `*AppInterface` header files (`.h`) across `//ios/` along with
     core EarlGrey 2 framework and enum headers to extract class/method
     docstrings from header comments (`//` and `/** ... */`), `#pragma mark`
     categories, parameter names, backticked or `@param` parameter explanations,
     return value descriptions, and concrete C++/Objective-C/enum types into
     `eg_metadata.json`. The extractor merges protocol and category declarations
     into their target classes, infers logical subcategories for uncategorized
     third-party EarlGrey headers, and excludes internal eDO backing headers
     that sit behind a paired `*_earl_grey.h` test-side wrapper (e.g.,
     `ChromeEarlGreyAppInterface` behind `ChromeEarlGreyImpl`),
     test-runner/swizzler plumbing, and internal singleton/observer selectors.
   - **Run-Time Objective-C Reflection & Dispatch**: Uses `<objc/runtime.h>`,
     `EDOClientService`, `NSMethodSignature`, and `NSInvocation` to discover and
     invoke selectors dynamically at run time without generating C++ or
     Objective-C glue code.
3. **Full Test-Side and App-Side Access (XCUITest Runner Bridge)**: Runs inside
   a persistent `XCTestCase` (`EGMCPServerTestCase`) hosted by an XCUITest
   runner bundle (`ios_chrome_eg_mcp_module-Runner.app` or
   `ios_web_shell_eg_mcp_module-Runner.app`) paired with the host application
   (`ios_chrome_eg2tests.app` or `ios_web_shell_eg2tests.app`) over
   `eDistantObject` (eDO). This provides access to both test-process helpers
   (e.g., `ChromeEarlGreyImpl`, `ShellEarlGreyImpl`, `EarlGreyImpl`,
   `AppLaunchManager`) and app-process `*AppInterface` helpers (e.g.,
   `ChromeMatchersAppInterface`, `ShellMatchersAppInterface`).
4. **Hierarchical Individual-Tool Mapping with Build-Time Breadth Caps**: Every
   Objective-C method maps 1-to-1 to an individual, strongly-typed MCP tool with
   a strict JSON Schema (`additionalProperties: false`). Tools are organized in
   a hierarchy (`Domain -> Class -> #pragma mark Category -> Method Tool`) with
   strict build-time breadth caps at each level so no hierarchy query or mounted
   category ever overflows an agent's context window.
5. **High-Fidelity Standalone Execution (Clean eDO Side-Car)**: Supports driving
   near-production builds (`chrome_mcp`, `ios_web_shell_mcp`) without test-host
   fakes (preserving real First Run Experience, real Search Engine Choice, real
   Keychain entitlements, and genuine App Group container sharing) and without
   EarlGrey's `GREYUIThreadExecutor` runloop swizzling. The companion side-car
   process provides crash isolation, capturing application exit signals and
   backtraces without terminating the agent's MCP session.

______________________________________________________________________

## 2. Architecture Overview

```
Mode 1: XCUITest Runner Targets (chrome_eg_mcp / ios_web_shell_eg_mcp)
======================================================================
+-----------------------------------------------------------------------+
| macOS Host Process                                                    |
|                                                                       |
|  MCP Client / Agent  <---(MCP JSON-RPC stdio)--->  ios_mcp_driver     |
|                                                    (--target *_eg_mcp)|
|                                                           ^           |
|                                       eg_metadata.json ---+           |
+-----------------------------------------------------------|-----------+
                                                            |
                                          HTTP JSON Bridge (127.0.0.1)
                                                            |
+-----------------------------------------------------------v-----------+
| iOS Simulator                                                         |
|                                                                       |
|  +-----------------------------------------------------------------+  |
|  | XCUITest Runner Bundle (*_eg_mcp_module-Runner.app)             |  |
|  |  - EGMCPServerTestCase / EGMCPHTTPServer                        |  |
|  |  - EGMCPRuntimeDispatcher (NSInvocation + C++ Value Marshaler)  |  |
|  |  - EGMCPObjectStore (Typed Handles: "handle:<Type>:<id>")       |  |
|  |  - Shared Test-Side: EarlGreyImpl, AppLaunchManager             |  |
|  |  - Target Test-Side: *EarlGreyImpl / *EarlGreyUI helpers        |  |
|  |  - EDOClientService (Dynamic Remote Class Proxies)              |  |
|  +-----------------------------------------------------------------+  |
|                                   ^                                   |
|                                   | eDO IPC (hostPort)                |
|                                   v                                   |
|  +-----------------------------------------------------------------+  |
|  | Host Application Process (*_eg2tests.app)                       |  |
|  |  - EGMCPAppIntrospector (Dumps loaded app-side ObjC classes)    |  |
|  |  - Shared AppFramework: GREYMatchersAppInterface,               |  |
|  |    GREYActionsAppInterface, GREYElementInteraction              |  |
|  |  - Target App-Side: Bridged *AppInterface classes               |  |
|  |  - eg_tests_hook.mm (Test stubs, disabled FRE, swizzled loops)  |  |
|  +-----------------------------------------------------------------+  |
+-----------------------------------------------------------------------+

Mode 2: Standalone Clean eDO Targets (chrome_mcp / ios_web_shell_mcp)
=====================================================================
+-----------------------------------------------------------------------+
| macOS Host Process / Simulator Companion                              |
|                                                                       |
|  MCP Client / Agent  <---(MCP JSON-RPC stdio)--->  ios_mcp_driver     |
|                                                    (--target *_mcp)   |
|                                                           ^           |
|                                       eg_metadata.json ---+           |
|  +-----------------------------------------------------------------+  |
|  | Companion Side-Car Process                                      |  |
|  |  - EGMCPHTTPServer & EGMCPRuntimeDispatcher                     |  |
|  |  - Crash & Exit Watchdog (survives app crashes)                 |  |
|  |  - eDO Client Channel                                           |  |
|  +-----------------------------------------------------------------+  |
+-----------------------------------------------------------|-----------+
                                                            |
                                        Clean eDO IPC (127.0.0.1:port)
                                                            |
+-----------------------------------------------------------v-----------+
| iOS Simulator                                                         |
|                                                                       |
|  +-----------------------------------------------------------------+  |
|  | Standalone Application Process (*_mcp.app)                      |  |
|  |  - EDOHostService (Registers EGMCPAppIntrospector on loopback)  |  |
|  |  - Target App-Side: Bridged *AppInterface classes               |  |
|  |  - Near-production hooks (real FRE, real search choice, etc.)   |  |
|  |  - Zero EarlGrey dependencies; unswizzled native run loops      |  |
|  +-----------------------------------------------------------------+  |
+-----------------------------------------------------------------------+
```

### 2.1 Shared Concepts: Dynamic Dispatch & Object Handle Model

Both `--target chrome` and `--target web` share the same host-to-simulator
bridge (`EGMCPServerTestCase`, `EGMCPHTTPServer`, `EGMCPRuntimeDispatcher`,
`EGMCPObjectStore`, and `EGMCPAppIntrospector`) as well as the core EarlGrey 2
and `//ios/testing/earl_grey/` tool surface:

- **Loopback HTTP-JSON Bridge (`EGMCPHTTPServer` & `EGMCPServerTestCase`)**:
  `EGMCPServerTestCase/testRunMCPServerLoop` starts `EGMCPHTTPServer` (backed by
  `net::EmbeddedTestServer`) on a loopback TCP port (`127.0.0.1`) inside the
  XCUITest runner process and exposes 6 endpoints:
  - `GET /health`: Answered immediately on the HTTP IO thread (without waiting
    for the main run loop) to report bridge readiness and bound port
    (`{"ok": true, "status": "ready", "port": <port>}`).
  - `GET /introspect`: Dispatched on the main run loop to query both local
    XCUITest runner classes (`testClasses`) and loaded host-application classes
    over eDO (`appClasses` via `EGMCPAppIntrospector`), returning their class
    and instance selectors and Objective-C runtime type encodings.
  - `POST /invoke`: Dispatched on the main run loop via `EGMCPRuntimeDispatcher`
    to execute a selector on a class, singleton helper, remote eDO class proxy,
    or retained object handle
    (`{"target", "selector", "args", "paramTypes"?, "returnType"?}`), marshaling
    C++/Objective-C arguments and returning the JSON result or a newly stored
    `"handle:<TypeTag>:<id>"` handle.
  - `POST /release_handle`: Releases a single retained Objective-C object handle
    (`{"handle": "handle:<TypeTag>:<id>"}`) from `EGMCPObjectStore` and returns
    whether it was found (`{"ok": true, "released": <bool>}`).
  - `POST /release_all_handles`: Releases all retained Objective-C objects in
    `EGMCPObjectStore` and resets the handle store.
  - `POST /shutdown`: Clears all retained handles, stops accepting new requests,
    and exits `EGMCPHTTPServer`'s run loop so `testRunMCPServerLoop` completes
    gracefully.
- **Shared Core EarlGrey & `//ios/testing/earl_grey/` Surface
  (`testing_earlgrey` domain)**: Every session exposes core EarlGrey 2
  interaction, matcher, and action classes (`EarlGreyImpl`,
  `GREYElementInteraction`, `GREYMatchers`, `GREYActions`), `ElementSelector`,
  and `AppLaunchManager` at startup from build-time metadata (and merges any
  additional runtime selectors on connection) so agents have immediate access to
  element selection, visibility/hierarchy matchers, tap/scroll/text actions, and
  app relaunch controls regardless of target.
- **Test-Side Class Resolution**: Classes linked into the XCUITest runner bundle
  (`EarlGreyImpl`, `AppLaunchManager`, and target-specific `*EarlGreyImpl` /
  `*EarlGreyUI` helpers) are resolved directly via `NSClassFromString()`.
  Singleton and `BaseEGTestHelperImpl` instances are obtained generically by
  checking whether the class responds to
  `@selector(invokedFromFile:lineNumber:)`, `@selector(sharedInstance)`, or
  `@selector(sharedManager)`.
- **App-Side Class Resolution via `EDOClientService`**: In EarlGrey 2,
  `GREY_STUB_CLASS_IN_APP_MAIN_QUEUE` is a wrapper around:
  ```objc
  [EDOClientService
      classObjectWithName:className
                     port:GREYTestApplicationDistantObject.sharedInstance.hostPort]
  ```
  Given any class name discovered in the host app by `EGMCPAppIntrospector`,
  `EGMCPRuntimeDispatcher` dynamically fetches its remote `EDOObject` class
  proxy. The proxy responds to `-methodSignatureForSelector:` and forwards
  `NSInvocation` calls to the main thread of the host application process.
- **Generic C++ Value-Type Marshaling**: Test-side `*EarlGreyImpl` methods
  occasionally accept Chromium C++ value types (`const GURL&`,
  `base::TimeDelta`, `const std::string&`). Objective-C++ runtime type encodings
  preserve these struct names in braces (`r^{GURL=...}`, `{TimeDelta=q}`,
  `r^{basic_string...}`). `EGMCPRuntimeDispatcher` inspects these struct tags at
  runtime to construct stack `GURL`, `base::TimeDelta` (in seconds), or
  `std::string` instances from JSON strings or numbers.
- **Typed Object Handle Registry (`EGMCPObjectStore`) & `usable_with` Hints**:
  When a method returns a non-JSON Objective-C object (such as
  `id<GREYMatcher>`, `id<GREYAction>`, `GREYElementInteraction*`,
  `ElementSelector*`, or target-specific opaque objects), `EGMCPObjectStore`
  retains it and returns a typed handle string `"handle:<TypeTag>:<id>"` (e.g.,
  `"handle:GREYMatcher:1"`). The MCP server enriches handle responses with a
  `usable_with` hint listing primary instance methods (`as_target_handle`) and
  consuming tools (`as_argument`) for `<TypeTag>`. Subsequent tool calls pass
  this handle string to parameters expecting that type, and the tool JSON Schema
  enforces the type tag via regex `pattern`.
- **Automatic `NSError**` Handling**: Parameters with Objective-C encoding `^@`
  (`NSError**`) are omitted from required tool inputs; the dispatcher
  automatically passes a local `NSError*` pointer and surfaces any resulting
  error in the tool response.

### 2.2 Chrome for iOS (`--target chrome`, `//ios/chrome/`)

When targeting full Chrome for iOS (the default target), the MCP server wraps
Chrome's EarlGrey 2 test harness and feature helpers across `//ios/chrome/`:

- **Harness & Runner Pairing**: Pairs `ios_chrome_eg_mcp_module-Runner.app` with
  `ios_chrome_eg2tests.app` (`ChromeTestCase` + `net::EmbeddedTestServer`).
- **Test-Side Helpers (`*EarlGreyImpl` / `*EarlGreyUI`)**: Exposes
  `ChromeEarlGreyImpl`, `ChromeEarlGreyUIImpl`, and feature-specific test-side
  `*EarlGreyImpl` / `*EarlGreyUI` helpers while automatically excluding their
  internal `*_earl_grey_app_interface.h` eDO backing headers when a paired
  `*_earl_grey.h` wrapper exists.
- **App-Side Matchers, Actions, and Feature `*AppInterface`s**:
  - **Core Chrome Matchers & Actions (`chrome_earlgrey` domain)**: Exposes
    `ChromeMatchersAppInterface` and `ChromeActionsAppInterface` for browser UI
    elements (omnibox, toolbar, tab grid, settings cells, context menus).
  - **Feature `*AppInterface` & Helper Classes**: Exposes app-process and
    feature helpers grouped into 10 functional feature domains (70 feature
    classes at the time of writing; e.g., `auth_and_ntp`,
    `autofill_and_passwords`, `settings_and_policy`).

### 2.3 `ios_web_shell` (`--target web`, `//ios/web/shell/test/`)

When targeting **`ios_web_shell`** (`//ios/web/shell/`), the MCP server wraps
the minimal `//ios/web`-layer browser harness used to test `WKWebView`
integration, navigation, context menus, and page state in isolation from Chrome
UI:

- **Harness & Runner Pairing**: Pairs `ios_web_shell_eg_mcp_module-Runner.app`
  with `ios_web_shell_eg2tests.app` (`WebShellTestCase` +
  `net::EmbeddedTestServer`).
- **Test-Side & App-Side `web_shell_earlgrey` Domain**: Exposes
  `ShellEarlGreyImpl` for navigation and web-state synchronization (while
  excluding its internal `ShellEarlGreyAppInterface` backing class), alongside
  `ShellMatchersAppInterface` and `ShellActionsAppInterface` for `WKWebView` and
  shell toolbar matchers/actions returning typed `"handle:GREYMatcher:<id>"` and
  `"handle:GREYAction:<id>"` handles.

______________________________________________________________________

## 3. Hierarchical Tool Model & Build-Time Validation

Exposing all bridged EarlGrey methods (1,266 methods across 85 classes and 13
enums at the time of writing) simultaneously in `tools/list` would exceed LLM
context limits, while a single untyped `invoke(class, selector, args)` tool is
error-prone. Instead, every Objective-C method is mapped 1-to-1 to a
**strongly-typed leaf tool** (`<ClassName>__<SelectorWithUnderscores>`) inside a
**capped hierarchy**:

1. **Level 1 — Functional Domains (≤ 20 domains)**: Grouped by feature area
   (e.g., `testing_earlgrey`, `chrome_earlgrey`, `web_shell_earlgrey`, and
   Chrome feature domains, plus `Core EarlGrey Framework` merged from runtime
   introspection; 13 static domains at the time of writing).
2. **Level 2 — Interface Classes (≤ 25 classes per domain)**: Individual bridged
   `*AppInterface` and EarlGrey helper classes within each domain.
3. **Level 3 — `#pragma mark` Categories (≤ 35 categories per class)**:
   Extracted from `#pragma mark - <Category>` directives in the class header.
4. **Level 4 — Individual Method Tools (≤ 35 methods per category)**:
   Strongly-typed per-selector tools with strict JSON Schemas generated from
   header comments and runtime type encodings.

### 3.1 Build-Time Breadth Caps & Docstring Enforcement

During the build, `extract_eg_metadata.py` validates two invariants across all
bridged EarlGrey headers:

1. **Hierarchy Breadth Caps**: If any domain, class, or `#pragma mark` category
   exceeds its maximum child count, the build fails with an actionable error
   message instructing the developer how to rebalance the hierarchy (for
   example, by adding `#pragma mark - <Category>` sections to subdivide an
   oversized header or merging closely related interfaces).
2. **Docstring & Parameter Completeness (Also Checked in
   `//ios/PRESUBMIT.py`)**: Every bridged EarlGrey method declaration must have
   a preceding comment block that documents:
   - Its **functionality** (what the method does),
   - All **arguments** (each parameter other than `error:(NSError**)` referenced
     in backticks `` `<paramName>` `` or `@param <paramName>`), and
   - Its **return value** (for non-`void` methods).

### 3.2 Progressive Tool Mounting, Starter Kit, & Session Lifecycle Tools

At startup, the unified `ios_mcp_driver` exposes 8 built-in discovery and
lifecycle tools (which operate uniformly across both `--target chrome` and
`--target web`) alongside a pre-mounted, target-aware **Core Starter Kit** of 14
universal element selection, interaction, matcher, action, and target navigation
tools:

- `eg_browse_hierarchy(domain?, class_name?, include_categories?)`
- `eg_list_category_tools(class_name, category?, mount?)`
- `eg_search_tools(query?, queries?, domain?, class_name?, role?, returns_type?, accepts_type?, auto_mount_top?, limit?)`
- `eg_mount_tools(tool_names?, class_name?, category?, unmount_others?, keep_starter_tools?)`
- `eg_session_status()`
- `eg_release_handle(handle)`
- `eg_release_all_handles()`
- `eg_shutdown()`

Discovery and mounting features include:

- **Soft-AND Search, Stemming, Synonyms, & `did_you_mean` Suggestions**:
  `eg_search_tools` ranks methods using IDF-weighted concept coverage, English
  stemming (reducing inflected words to their root stem, e.g., matching
  `tapping`, `tapped`, or `taps` to `tap`), compound synonym expansion (e.g.,
  `omnibox` ↔ `address bar`, `ntp` ↔ `new tab page`), and batch `queries`
  interleaved round-robin. Common class and tool aliases (e.g., `ChromeEarlGrey`
  → `ChromeEarlGreyImpl`) are resolved automatically, and unknown class or tool
  names return ranked `did_you_mean` suggestions.
- **Role & Handle Type-Flow Filters + Compact Signatures**: `eg_search_tools`
  supports filtering by functional `role` (`matcher`, `action`, `interaction`,
  `wait_or_assert`, `helper`), `returns_type`, and `accepts_type`. Browse,
  category-list, and search results include compact parameter signatures
  (`name:type`, `name:enum:<Enum>`, `name:handle:<TypeTag>`) so agents can
  inspect parameter types before mounting.
- **One-Step & Progressive Mounting**: When an agent calls `eg_mount_tools` (by
  `tool_names`, by `class_name` alone for classes with ≤ 35 methods, or by
  `(class_name, category)`), or passes `auto_mount_top` to `eg_search_tools` /
  `mount=True` to `eg_list_category_tools`, the server registers those
  individual method tools into its active `tools/list` and emits
  `notifications/tools/list_changed`. By default, `unmount_others=True`
  preserves the pre-mounted Core Starter Kit unless `keep_starter_tools=False`
  is explicitly passed.

______________________________________________________________________

## 4. Building & Running

```bash
# Build the unified MCP driver wrapper and targets:
# 1. XCUITest Runner Targets (full EarlGrey helpers & system UI):
autoninja -C out/Debug-iphonesimulator ios_mcp_driver ios_chrome_eg_mcp_module
autoninja -C out/Debug-iphonesimulator ios_mcp_driver ios_web_shell_eg_mcp_module

# 2. Standalone Targets (clean eDO side-car, ~1-sec launch, high fidelity):
autoninja -C out/Debug-iphonesimulator ios_mcp_driver chrome_mcp
autoninja -C out/Debug-iphonesimulator ios_mcp_driver ios_web_shell_mcp

# Run as an MCP stdio server targeting XCUITest runner mode:
./out/Debug-iphonesimulator/bin/ios_mcp_driver --target chrome_eg_mcp --device "iPhone 16 Pro"
./out/Debug-iphonesimulator/bin/ios_mcp_driver --target ios_web_shell_eg_mcp --device "iPhone 16 Pro"

# Run as an MCP stdio server targeting Standalone Clean eDO mode:
./out/Debug-iphonesimulator/bin/ios_mcp_driver --target chrome_mcp --device "iPhone 16 Pro"
./out/Debug-iphonesimulator/bin/ios_mcp_driver --target ios_web_shell_mcp --device "iPhone 16 Pro"
```

### 4.1 Unit & End-to-End Tests

Unit tests verify the build-time header extractor, MCP catalog/stdio server, and
Objective-C++ runtime dispatcher/HTTP server, while end-to-end test suites
verify that an agent can start, actuate, and observe a live Chrome or
`ios_web_shell` instance on the iOS Simulator:

```bash
# 1. Python unit tests (extractor & MCP driver; also run in PRESUBMIT.py):
vpython3 -m unittest discover -s ios/testing/earl_grey_mcp/scripts -p '*_unittest.py'

# 2. C++ / Objective-C++ runtime dispatcher & HTTP bridge unit tests:
ios/tools/run_unittests.py --out-dir out/Debug-iphonesimulator \
    --target ios_eg_mcp_unittests

# 3. In-runner EarlGrey 2 end-to-end test (EGMCPServerTestCase):
ios/tools/run_egtests.py --out-dir out/Debug-iphonesimulator \
    --scheme ios_chrome_eg_mcp_module \
    --tests EGMCPServerTestCase/testEndToEndMCPAgentWorkflow
ios/tools/run_egtests.py --out-dir out/Debug-iphonesimulator \
    --scheme ios_web_shell_eg_mcp_module \
    --tests EGMCPServerTestCase/testEndToEndMCPAgentWorkflow

# 4. Host-driven Python MCP stdio end-to-end test:
python3 ios/testing/earl_grey_mcp/scripts/ios_mcp_driver_e2e_test.py \
    --target chrome_eg_mcp --out-dir out/Debug-iphonesimulator
python3 ios/testing/earl_grey_mcp/scripts/ios_mcp_driver_e2e_test.py \
    --target chrome_mcp --out-dir out/Debug-iphonesimulator
```
