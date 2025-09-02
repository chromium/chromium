# Dappnet Settings Page Implementation Specification

## Overview
Implementation plan for a dedicated settings/configuration page for Dappnet functionality within Chromium, accessible via `about://dappnet/config`.

## URL Schema
- **Primary URL**: `about://dappnet/config`
- **Handler**: Custom protocol handler registered in Chrome's about: scheme
- **Navigation**: Should be accessible from browser settings and omnibox

## Page Architecture

### 1. Frontend Components

#### 1.1 Main Settings Container
- **Technology**: WebUI (Chrome's internal UI framework)
- **Location**: `chrome/browser/resources/dappnet_settings/`
- **Files**:
  - `dappnet_settings.html` - Main HTML structure
  - `dappnet_settings.js` - JavaScript controller
  - `dappnet_settings.css` - Styling
  - `dappnet_settings_api.js` - Browser API bridge

#### 1.2 UI Sections

##### Ethereum RPC Configuration
- **Component**: `<ethereum-rpc-list>`
- **Features**:
  - Dynamic list of RPC endpoints
  - Add/Edit/Delete functionality
  - URL validation
  - Connection testing
  - Default endpoint selection
- **Data Structure**:
  ```javascript
  {
    endpoints: [
      {
        id: string,
        url: string,
        name: string,
        chainId: number,
        isDefault: boolean,
        isActive: boolean
      }
    ]
  }
  ```

##### Local Gateway Status
- **Component**: `<local-gateway-status>`
- **Display Elements**:
  - Running status indicator (green/red)
  - Port number display
  - Process PID
  - Restart button
  - Start/Stop toggle
  - Last restart timestamp
  - Error messages if any

##### IPFS Status
- **Component**: `<ipfs-status>`
- **Display Elements**:
  - Running status indicator
  - API port display
  - Gateway port display
  - Restart button
  - Start/Stop toggle
  - Peer count
  - Bandwidth usage stats

### 2. Backend Implementation

#### 2.1 Browser Process Components

##### WebUI Handler
- **Location**: `chrome/browser/ui/webui/dappnet/`
- **Files**:
  - `dappnet_settings_ui.h/.cc` - WebUI controller
  - `dappnet_settings_handler.h/.cc` - Message handler
  - `dappnet_settings_source.h/.cc` - Resource provider

##### Service Integration
- **Location**: `chrome/browser/dappnet/`
- **Components**:
  - `dappnet_service.h/.cc` - Main service coordinator
  - `ethereum_rpc_manager.h/.cc` - RPC endpoint management
  - `local_gateway_controller.h/.cc` - Gateway process control
  - `ipfs_controller.h/.cc` - IPFS daemon control

#### 2.2 Mojo Interfaces
- **Location**: `chrome/browser/dappnet/mojom/`
- **Definition**: `dappnet_settings.mojom`
```mojom
interface DappnetSettings {
  // RPC Management
  GetRpcEndpoints() => (array<RpcEndpoint> endpoints);
  AddRpcEndpoint(RpcEndpoint endpoint) => (bool success);
  UpdateRpcEndpoint(string id, RpcEndpoint endpoint) => (bool success);
  RemoveRpcEndpoint(string id) => (bool success);
  TestRpcConnection(string url) => (bool connected, string error);
  
  // Gateway Control
  GetGatewayStatus() => (GatewayStatus status);
  RestartGateway() => (bool success);
  StopGateway() => (bool success);
  StartGateway() => (bool success);
  
  // IPFS Control
  GetIpfsStatus() => (IpfsStatus status);
  RestartIpfs() => (bool success);
  StopIpfs() => (bool success);
  StartIpfs() => (bool success);
};
```

### 3. Data Persistence

#### 3.1 Preferences Storage
- **Location**: User profile preferences
- **Keys**:
  - `dappnet.rpc_endpoints` - JSON array of RPC configurations
  - `dappnet.default_rpc` - Default RPC endpoint ID
  - `dappnet.gateway.auto_start` - Auto-start gateway on browser launch
  - `dappnet.ipfs.auto_start` - Auto-start IPFS on browser launch
  - `dappnet.gateway.port` - Gateway port configuration
  - `dappnet.ipfs.api_port` - IPFS API port
  - `dappnet.ipfs.gateway_port` - IPFS gateway port

#### 3.2 Database Schema
- **Location**: Profile directory SQLite database
- **Tables**:
  - `dappnet_rpc_endpoints` - Persistent RPC endpoint storage
  - `dappnet_connection_history` - Connection logs and metrics

### 4. Process Management

#### 4.1 Local Gateway Process
- **Binary**: `local-gateway` executable
- **Launch**: Via `base::LaunchProcess()`
- **Monitoring**: Process handle tracking
- **Communication**: IPC via named pipes or sockets
- **Restart Logic**:
  1. Send SIGTERM to existing process
  2. Wait for graceful shutdown (max 5s)
  3. Force kill if necessary
  4. Launch new instance
  5. Verify startup success

#### 4.2 IPFS Process
- **Binary**: `ipfs` daemon
- **Launch**: Similar to gateway
- **Configuration**: Pass config via command line args
- **Health Checks**: Periodic API endpoint polling

### 5. Security Considerations

#### 5.1 Input Validation
- URL validation for RPC endpoints
- Port range validation (1024-65535)
- Sanitization of user inputs
- CORS policy enforcement

#### 5.2 Process Isolation
- Run gateway and IPFS in sandbox
- Limited filesystem access
- Network namespace isolation where possible

#### 5.3 Permission Model
- No special permissions required for viewing
- Admin confirmation for process restart
- Rate limiting on restart operations

### 6. Build Integration

#### 6.1 GN Build Files
```gn
# chrome/browser/resources/dappnet_settings/BUILD.gn
js_library("dappnet_settings") {
  deps = [
    "//ui/webui/resources/js:cr",
    "//chrome/browser/resources:strings",
  ]
}

# chrome/browser/ui/webui/dappnet/BUILD.gn
source_set("dappnet_webui") {
  sources = [
    "dappnet_settings_ui.cc",
    "dappnet_settings_ui.h",
    "dappnet_settings_handler.cc",
    "dappnet_settings_handler.h",
  ]
  deps = [
    "//chrome/browser/dappnet",
    "//content/public/browser",
  ]
}
```

#### 6.2 Resource Registration
- Add to `chrome/browser/resources/resources.grd`
- Register in `chrome/browser/browser_resources.grd`
- Add URL mapping in `chrome/browser/ui/webui/chrome_web_ui_controller_factory.cc`

### 7. Testing Strategy

#### 7.1 Unit Tests
- `dappnet_settings_handler_unittest.cc` - Handler logic
- `ethereum_rpc_manager_unittest.cc` - RPC management
- `process_controller_unittest.cc` - Process lifecycle

#### 7.2 Browser Tests
- `dappnet_settings_browsertest.cc` - Full integration
- Test scenarios:
  - Add/remove RPC endpoints
  - Process restart functionality
  - Error handling
  - Persistence across restarts

#### 7.3 WebUI Tests
- JavaScript unit tests for UI components
- Mojo interface mocking
- UI interaction tests

### 8. Implementation Phases

#### Phase 1: Basic UI Structure (Week 1-2)
- Create WebUI scaffolding
- Implement basic HTML/CSS layout
- Set up Mojo interface definitions
- Register about://dappnet/config handler

#### Phase 2: RPC Management (Week 2-3)
- Implement RPC endpoint CRUD operations
- Add persistence layer
- Create UI components for RPC list
- Add connection testing

#### Phase 3: Process Control (Week 3-4)
- Implement gateway controller
- Implement IPFS controller
- Add process monitoring
- Create restart functionality

#### Phase 4: Polish & Testing (Week 4-5)
- Error handling improvements
- Add loading states
- Comprehensive testing
- Performance optimization

### 9. Dependencies

#### External Libraries
- None required (uses Chrome's existing infrastructure)

#### Internal Dependencies
- `//base` - Process management, threading
- `//content/public/browser` - WebUI framework
- `//ui/webui/resources` - UI components
- `//mojo/public` - IPC interfaces

### 10. Migration Considerations

#### From Existing Settings
- If settings exist elsewhere, provide migration path
- Preserve user configurations
- One-time migration on first access

#### Backwards Compatibility
- Maintain support for command-line flags
- Gradual deprecation of old configuration methods

### 11. Performance Metrics

#### Monitoring Points
- Page load time
- Process restart duration
- RPC connection test latency
- Memory usage of settings page

#### Success Criteria
- Page loads in < 500ms
- Process restart completes in < 3s
- RPC test completes in < 2s
- Memory footprint < 10MB

### 12. Documentation Requirements

#### User Documentation
- Help center article on Dappnet settings
- Tooltips for each setting
- Error message explanations

#### Developer Documentation
- API documentation for extensions
- Architecture overview in docs/
- Code comments for complex logic