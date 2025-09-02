# Dappnet Settings Page Implementation

## Executive Summary

Create a settings page at `about://dappnet/config` for managing Dappnet components:
- Ethereum RPC endpoints configuration
- Local gateway process control  
- IPFS daemon management

The implementation uses Chrome's WebUI framework with Mojo IPC for frontend-backend communication.

---

## Stage 1: WebUI Foundation & URL Registration

### Goal
Establish the basic settings page accessible at `about://dappnet/config`.

### Files to Create

#### Frontend Structure
```
chrome/browser/resources/dappnet_settings/
├── dappnet_settings.html
├── dappnet_settings.js
├── dappnet_settings.css
└── dappnet_settings_api.js
```

#### Backend Structure
```
chrome/browser/ui/webui/dappnet/
├── dappnet_settings_ui.h
├── dappnet_settings_ui.cc
├── dappnet_settings_handler.h
└── dappnet_settings_handler.cc
```

### Implementation Steps

1. **Create WebUI Controller** (`dappnet_settings_ui.cc`)
```cpp
class DappnetSettingsUI : public content::WebUIController {
 public:
  explicit DappnetSettingsUI(content::WebUI* web_ui);
  ~DappnetSettingsUI() override;
};
```

2. **Register URL Handler** in `chrome_web_ui_controller_factory.cc`
```cpp
if (url.host() == "dappnet" && url.path() == "/config") {
  return &NewWebUI<DappnetSettingsUI>;
}
```

3. **Create Basic HTML Template** (`dappnet_settings.html`)
```html
<!DOCTYPE html>
<html>
<head>
  <link rel="stylesheet" href="chrome://resources/css/chrome_shared.css">
  <link rel="stylesheet" href="dappnet_settings.css">
</head>
<body>
  <h1>Dappnet Settings</h1>
  <div id="rpc-section"></div>
  <div id="gateway-section"></div>
  <div id="ipfs-section"></div>
  <script src="dappnet_settings.js"></script>
</body>
</html>
```

4. **Add to Build System** (`BUILD.gn`)
```gn
source_set("dappnet_webui") {
  sources = [
    "dappnet_settings_ui.cc",
    "dappnet_settings_ui.h",
    "dappnet_settings_handler.cc",
    "dappnet_settings_handler.h",
  ]
  deps = [
    "//content/public/browser",
    "//ui/webui",
  ]
}
```

5. **Register Resources** in `resources.grd`
```xml
<include name="IDR_DAPPNET_SETTINGS_HTML" 
         file="dappnet_settings/dappnet_settings.html" type="BINDATA" />
<include name="IDR_DAPPNET_SETTINGS_JS" 
         file="dappnet_settings/dappnet_settings.js" type="BINDATA" />
<include name="IDR_DAPPNET_SETTINGS_CSS" 
         file="dappnet_settings/dappnet_settings.css" type="BINDATA" />
```

### Verification
- Navigate to `about://dappnet/config` 
- Should display basic HTML page with title

---

## Stage 2: Mojo Interface & Message Handler

### Goal
Establish communication between frontend JavaScript and backend C++.

### Files to Create

#### Mojo Definition
```
chrome/browser/dappnet/mojom/
└── dappnet_settings.mojom
```

### Implementation Steps

1. **Define Mojo Interface** (`dappnet_settings.mojom`)
```mojom
module dappnet.mojom;

struct RpcEndpoint {
  string id;
  string url;
  string name;
  int32 chain_id;
  bool is_default;
};

struct GatewayStatus {
  bool is_running;
  int32 port;
  int32 pid;
  string error_message;
};

struct IpfsStatus {
  bool is_running;
  int32 api_port;
  int32 gateway_port;
  int32 peer_count;
};

interface DappnetSettingsHandler {
  // RPC Management
  GetRpcEndpoints() => (array<RpcEndpoint> endpoints);
  AddRpcEndpoint(RpcEndpoint endpoint) => (bool success, string error);
  UpdateRpcEndpoint(string id, RpcEndpoint endpoint) => (bool success);
  RemoveRpcEndpoint(string id) => (bool success);
  TestRpcConnection(string url) => (bool connected, string error);
  SetDefaultRpc(string id) => (bool success);
  
  // Gateway Control
  GetGatewayStatus() => (GatewayStatus status);
  StartGateway() => (bool success, string error);
  StopGateway() => (bool success);
  RestartGateway() => (bool success, string error);
  
  // IPFS Control
  GetIpfsStatus() => (IpfsStatus status);
  StartIpfs() => (bool success, string error);
  StopIpfs() => (bool success);
  RestartIpfs() => (bool success, string error);
};
```

2. **Implement Message Handler** (`dappnet_settings_handler.cc`)
```cpp
class DappnetSettingsHandler : public dappnet::mojom::DappnetSettingsHandler {
 public:
  explicit DappnetSettingsHandler(
      mojo::PendingReceiver<dappnet::mojom::DappnetSettingsHandler> receiver);
  
  // RPC Management
  void GetRpcEndpoints(GetRpcEndpointsCallback callback) override;
  void AddRpcEndpoint(dappnet::mojom::RpcEndpointPtr endpoint,
                      AddRpcEndpointCallback callback) override;
  // ... other methods
  
 private:
  mojo::Receiver<dappnet::mojom::DappnetSettingsHandler> receiver_;
};
```

3. **Connect Handler in WebUI** (`dappnet_settings_ui.cc`)
```cpp
DappnetSettingsUI::DappnetSettingsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the data source
  content::WebUIDataSource* source = content::WebUIDataSource::Create("dappnet");
  source->AddResourcePath("config", IDR_DAPPNET_SETTINGS_HTML);
  
  // Bind the handler
  web_ui->AddMessageHandler(std::make_unique<DappnetSettingsHandler>());
}
```

4. **Create JavaScript API Bridge** (`dappnet_settings_api.js`)
```javascript
class DappnetSettingsApi {
  constructor() {
    this.handler = dappnet.mojom.DappnetSettingsHandler.getRemote();
  }
  
  async getRpcEndpoints() {
    const {endpoints} = await this.handler.getRpcEndpoints();
    return endpoints;
  }
  
  async addRpcEndpoint(endpoint) {
    const {success, error} = await this.handler.addRpcEndpoint(endpoint);
    if (!success) throw new Error(error);
    return success;
  }
  // ... other methods
}
```

### Verification
- Open DevTools console on settings page
- Test API calls: `api = new DappnetSettingsApi(); await api.getRpcEndpoints()`

---

## Stage 3: RPC Endpoint Management UI

### Goal
Implement full CRUD operations for Ethereum RPC endpoints.

### Implementation Steps

1. **Create RPC List Component** (`dappnet_settings.js`)
```javascript
class RpcEndpointList extends HTMLElement {
  constructor() {
    super();
    this.api = new DappnetSettingsApi();
    this.endpoints = [];
  }
  
  connectedCallback() {
    this.render();
    this.loadEndpoints();
  }
  
  async loadEndpoints() {
    this.endpoints = await this.api.getRpcEndpoints();
    this.render();
  }
  
  render() {
    this.innerHTML = `
      <div class="rpc-header">
        <h2>Ethereum RPC Endpoints</h2>
        <button id="add-rpc">Add Endpoint</button>
      </div>
      <div class="rpc-list">
        ${this.endpoints.map(ep => this.renderEndpoint(ep)).join('')}
      </div>
    `;
    this.attachEventListeners();
  }
  
  renderEndpoint(endpoint) {
    return `
      <div class="rpc-item" data-id="${endpoint.id}">
        <div class="rpc-info">
          <span class="rpc-name">${endpoint.name}</span>
          <span class="rpc-url">${endpoint.url}</span>
          <span class="rpc-chain">Chain ID: ${endpoint.chainId}</span>
          ${endpoint.isDefault ? '<span class="default-badge">Default</span>' : ''}
        </div>
        <div class="rpc-actions">
          <button class="test-btn" data-url="${endpoint.url}">Test</button>
          <button class="edit-btn" data-id="${endpoint.id}">Edit</button>
          <button class="delete-btn" data-id="${endpoint.id}">Delete</button>
          ${!endpoint.isDefault ? 
            `<button class="default-btn" data-id="${endpoint.id}">Set Default</button>` : ''}
        </div>
      </div>
    `;
  }
}

customElements.define('rpc-endpoint-list', RpcEndpointList);
```

2. **Add RPC Dialog Component**
```javascript
class AddRpcDialog extends HTMLElement {
  show() {
    this.innerHTML = `
      <dialog id="add-rpc-dialog">
        <form>
          <label>Name: <input name="name" required></label>
          <label>URL: <input name="url" type="url" required></label>
          <label>Chain ID: <input name="chainId" type="number" required></label>
          <button type="submit">Add</button>
          <button type="button" id="cancel">Cancel</button>
        </form>
      </dialog>
    `;
    
    const dialog = this.querySelector('dialog');
    dialog.showModal();
    
    this.querySelector('form').addEventListener('submit', async (e) => {
      e.preventDefault();
      const formData = new FormData(e.target);
      await this.api.addRpcEndpoint({
        id: crypto.randomUUID(),
        name: formData.get('name'),
        url: formData.get('url'),
        chainId: parseInt(formData.get('chainId')),
        isDefault: false
      });
      dialog.close();
      this.dispatchEvent(new Event('endpoint-added'));
    });
  }
}
```

3. **Implement Data Persistence** (`dappnet_settings_handler.cc`)
```cpp
void DappnetSettingsHandler::GetRpcEndpoints(GetRpcEndpointsCallback callback) {
  PrefService* prefs = profile_->GetPrefs();
  const base::Value::List& endpoints = prefs->GetList("dappnet.rpc_endpoints");
  
  std::vector<dappnet::mojom::RpcEndpointPtr> result;
  for (const auto& value : endpoints) {
    auto endpoint = dappnet::mojom::RpcEndpoint::New();
    endpoint->id = value.GetDict().FindString("id").value_or("");
    endpoint->url = value.GetDict().FindString("url").value_or("");
    endpoint->name = value.GetDict().FindString("name").value_or("");
    endpoint->chain_id = value.GetDict().FindInt("chain_id").value_or(1);
    endpoint->is_default = value.GetDict().FindBool("is_default").value_or(false);
    result.push_back(std::move(endpoint));
  }
  
  std::move(callback).Run(std::move(result));
}

void DappnetSettingsHandler::AddRpcEndpoint(
    dappnet::mojom::RpcEndpointPtr endpoint,
    AddRpcEndpointCallback callback) {
  // Validate URL
  GURL url(endpoint->url);
  if (!url.is_valid() || (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsWSOrWSS())) {
    std::move(callback).Run(false, "Invalid URL");
    return;
  }
  
  // Save to preferences
  PrefService* prefs = profile_->GetPrefs();
  ListPrefUpdate update(prefs, "dappnet.rpc_endpoints");
  base::Value::List& list = update->GetList();
  
  base::Value::Dict new_endpoint;
  new_endpoint.Set("id", endpoint->id);
  new_endpoint.Set("url", endpoint->url);
  new_endpoint.Set("name", endpoint->name);
  new_endpoint.Set("chain_id", endpoint->chain_id);
  new_endpoint.Set("is_default", endpoint->is_default);
  
  list.Append(std::move(new_endpoint));
  std::move(callback).Run(true, "");
}
```

### Verification
- Add new RPC endpoint
- Edit existing endpoint
- Delete endpoint
- Set default endpoint
- Test connection to endpoint

---

## Stage 4: Process Control Implementation

### Goal
Implement start/stop/restart functionality for local gateway and IPFS.

### Files to Create
```
chrome/browser/dappnet/
├── local_gateway_controller.h
├── local_gateway_controller.cc
├── ipfs_controller.h
└── ipfs_controller.cc
```

### Implementation Steps

1. **Create Process Controller Base Class**
```cpp
class ProcessController {
 public:
  virtual ~ProcessController() = default;
  
  bool Start();
  bool Stop();
  bool Restart();
  bool IsRunning() const;
  int GetPid() const;
  
 protected:
  virtual base::CommandLine GetCommandLine() = 0;
  virtual bool VerifyStartup() = 0;
  
 private:
  base::Process process_;
  int port_;
};
```

2. **Implement Gateway Controller** (`local_gateway_controller.cc`)
```cpp
class LocalGatewayController : public ProcessController {
 public:
  LocalGatewayController() {
    port_ = GetGatewayPort();
  }
  
 protected:
  base::CommandLine GetCommandLine() override {
    base::FilePath exe_path = GetGatewayExecutablePath();
    base::CommandLine cmd(exe_path);
    cmd.AppendSwitchASCII("--port", base::NumberToString(port_));
    cmd.AppendSwitchASCII("--rpc", GetCurrentRpcUrl());
    return cmd;
  }
  
  bool VerifyStartup() override {
    // Poll http://localhost:port/health
    for (int i = 0; i < 30; i++) {
      if (CheckHealthEndpoint()) return true;
      base::PlatformThread::Sleep(base::Milliseconds(100));
    }
    return false;
  }
  
 private:
  base::FilePath GetGatewayExecutablePath() {
    base::FilePath exe_dir;
    base::PathService::Get(base::DIR_EXE, &exe_dir);
    return exe_dir.AppendASCII("local-gateway");
  }
};
```

3. **Implement Process Start/Stop** (`process_controller.cc`)
```cpp
bool ProcessController::Start() {
  if (IsRunning()) return true;
  
  base::LaunchOptions options;
  options.start_hidden = true;
  
  process_ = base::LaunchProcess(GetCommandLine(), options);
  if (!process_.IsValid()) return false;
  
  if (!VerifyStartup()) {
    Stop();
    return false;
  }
  
  return true;
}

bool ProcessController::Stop() {
  if (!process_.IsValid()) return true;
  
  // Graceful shutdown
  process_.Terminate(0, true);
  
  // Wait up to 5 seconds
  int exit_code;
  if (!process_.WaitForExitWithTimeout(base::Seconds(5), &exit_code)) {
    // Force kill
    process_.Terminate(1, false);
    process_.WaitForExit(&exit_code);
  }
  
  process_.Close();
  return true;
}

bool ProcessController::Restart() {
  Stop();
  base::PlatformThread::Sleep(base::Milliseconds(500));
  return Start();
}
```

4. **Create UI Status Components** (`dappnet_settings.js`)
```javascript
class ProcessStatus extends HTMLElement {
  constructor() {
    super();
    this.processType = this.getAttribute('process-type'); // 'gateway' or 'ipfs'
  }
  
  async connectedCallback() {
    this.render();
    await this.updateStatus();
    // Poll status every 5 seconds
    this.interval = setInterval(() => this.updateStatus(), 5000);
  }
  
  disconnectedCallback() {
    clearInterval(this.interval);
  }
  
  async updateStatus() {
    const status = this.processType === 'gateway' 
      ? await this.api.getGatewayStatus()
      : await this.api.getIpfsStatus();
    
    this.querySelector('.status-indicator').className = 
      `status-indicator ${status.isRunning ? 'running' : 'stopped'}`;
    this.querySelector('.status-text').textContent = 
      status.isRunning ? 'Running' : 'Stopped';
    this.querySelector('.port-info').textContent = 
      `Port: ${status.port || 'N/A'}`;
    this.querySelector('.pid-info').textContent = 
      `PID: ${status.pid || 'N/A'}`;
  }
  
  render() {
    this.innerHTML = `
      <div class="process-status">
        <h3>${this.processType === 'gateway' ? 'Local Gateway' : 'IPFS Daemon'}</h3>
        <div class="status-row">
          <span class="status-indicator"></span>
          <span class="status-text">Unknown</span>
        </div>
        <div class="status-details">
          <span class="port-info"></span>
          <span class="pid-info"></span>
        </div>
        <div class="control-buttons">
          <button class="start-btn">Start</button>
          <button class="stop-btn">Stop</button>
          <button class="restart-btn">Restart</button>
        </div>
      </div>
    `;
    this.attachEventListeners();
  }
  
  attachEventListeners() {
    this.querySelector('.start-btn').addEventListener('click', async () => {
      const result = this.processType === 'gateway'
        ? await this.api.startGateway()
        : await this.api.startIpfs();
      await this.updateStatus();
    });
    
    this.querySelector('.restart-btn').addEventListener('click', async () => {
      const result = this.processType === 'gateway'
        ? await this.api.restartGateway()
        : await this.api.restartIpfs();
      await this.updateStatus();
    });
  }
}

customElements.define('process-status', ProcessStatus);
```

### Verification
- Start/stop gateway process
- Start/stop IPFS daemon
- Restart processes
- Verify status updates in UI

---

## Stage 5: Testing & Polish

### Goal
Add comprehensive tests and error handling.

### Test Files to Create
```
chrome/test/data/webui/dappnet/
├── dappnet_settings_test.js
└── dappnet_settings_browsertest.cc
```

### Implementation Steps

1. **Unit Tests** (`dappnet_settings_handler_unittest.cc`)
```cpp
class DappnetSettingsHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    profile_ = std::make_unique<TestingProfile>();
    handler_ = std::make_unique<DappnetSettingsHandler>(profile_.get());
  }
  
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<DappnetSettingsHandler> handler_;
};

TEST_F(DappnetSettingsHandlerTest, AddValidRpcEndpoint) {
  auto endpoint = dappnet::mojom::RpcEndpoint::New();
  endpoint->url = "https://mainnet.infura.io/v3/key";
  endpoint->name = "Infura Mainnet";
  endpoint->chain_id = 1;
  
  base::RunLoop run_loop;
  handler_->AddRpcEndpoint(
      std::move(endpoint),
      base::BindOnce([](bool success, const std::string& error) {
        EXPECT_TRUE(success);
        EXPECT_TRUE(error.empty());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}

TEST_F(DappnetSettingsHandlerTest, RejectInvalidUrl) {
  auto endpoint = dappnet::mojom::RpcEndpoint::New();
  endpoint->url = "not-a-valid-url";
  
  base::RunLoop run_loop;
  handler_->AddRpcEndpoint(
      std::move(endpoint),
      base::BindOnce([](bool success, const std::string& error) {
        EXPECT_FALSE(success);
        EXPECT_FALSE(error.empty());
      }).Then(run_loop.QuitClosure()));
  run_loop.Run();
}
```

2. **Browser Tests** (`dappnet_settings_browsertest.cc`)
```cpp
class DappnetSettingsBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL("about://dappnet/config")));
  }
};

IN_PROC_BROWSER_TEST_F(DappnetSettingsBrowserTest, PageLoads) {
  content::WebContents* web_contents = 
      browser()->tab_strip_model()->GetActiveWebContents();
  
  EXPECT_EQ(GURL("about://dappnet/config"), web_contents->GetURL());
  
  // Check page title
  std::u16string title;
  EXPECT_TRUE(content::ExecuteScriptAndExtractString(
      web_contents,
      "window.domAutomationController.send(document.title)",
      &title));
  EXPECT_EQ(u"Dappnet Settings", title);
}

IN_PROC_BROWSER_TEST_F(DappnetSettingsBrowserTest, AddRpcEndpoint) {
  content::WebContents* web_contents = 
      browser()->tab_strip_model()->GetActiveWebContents();
  
  // Click add button
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      "document.querySelector('#add-rpc').click()"));
  
  // Fill form
  EXPECT_TRUE(content::ExecuteScript(
      web_contents,
      R"(
        document.querySelector('input[name="name"]').value = 'Test RPC';
        document.querySelector('input[name="url"]').value = 'https://test.rpc';
        document.querySelector('input[name="chainId"]').value = '1';
        document.querySelector('form').submit();
      )"));
  
  // Verify endpoint was added
  bool has_endpoint;
  EXPECT_TRUE(content::ExecuteScriptAndExtractBool(
      web_contents,
      "window.domAutomationController.send("
      "  document.querySelector('.rpc-name').textContent === 'Test RPC'"
      ")",
      &has_endpoint));
  EXPECT_TRUE(has_endpoint);
}
```

3. **Error Handling** (`dappnet_settings.js`)
```javascript
class ErrorHandler {
  static show(message, type = 'error') {
    const notification = document.createElement('div');
    notification.className = `notification ${type}`;
    notification.textContent = message;
    document.body.appendChild(notification);
    
    setTimeout(() => notification.remove(), 5000);
  }
  
  static async wrap(promise, errorMessage) {
    try {
      return await promise;
    } catch (error) {
      console.error(error);
      this.show(errorMessage || error.message);
      throw error;
    }
  }
}

// Usage in components
async addEndpoint(data) {
  await ErrorHandler.wrap(
    this.api.addRpcEndpoint(data),
    'Failed to add RPC endpoint'
  );
}
```

4. **Loading States**
```javascript
class LoadingIndicator {
  static show(element) {
    element.classList.add('loading');
    element.disabled = true;
    element.dataset.originalText = element.textContent;
    element.textContent = 'Loading...';
  }
  
  static hide(element) {
    element.classList.remove('loading');
    element.disabled = false;
    element.textContent = element.dataset.originalText;
  }
  
  static async wrap(element, promise) {
    this.show(element);
    try {
      return await promise;
    } finally {
      this.hide(element);
    }
  }
}
```

### Verification
- Run unit tests: `out/Default/unit_tests --gtest_filter="DappnetSettings*"`
- Run browser tests: `out/Default/browser_tests --gtest_filter="DappnetSettings*"`
- Test error scenarios (invalid URLs, process failures)
- Verify loading states appear/disappear correctly

---

## Stage 6: Security & Performance

### Goal
Implement security measures and optimize performance.

### Implementation Steps

1. **Input Validation** (`dappnet_settings_handler.cc`)
```cpp
bool ValidateRpcUrl(const std::string& url_string) {
  GURL url(url_string);
  
  // Must be valid URL
  if (!url.is_valid()) return false;
  
  // Must be HTTP(S) or WS(S)
  if (!url.SchemeIsHTTPOrHTTPS() && !url.SchemeIsWSOrWSS()) 
    return false;
  
  // Cannot be localhost (security)
  if (net::IsLocalhost(url)) return false;
  
  // Port range check
  int port = url.IntPort();
  if (port != -1 && (port < 1024 || port > 65535)) 
    return false;
  
  return true;
}

bool ValidatePortNumber(int port) {
  return port >= 1024 && port <= 65535;
}
```

2. **Rate Limiting**
```cpp
class RateLimiter {
 public:
  bool ShouldAllow(const std::string& action) {
    auto now = base::Time::Now();
    auto& last_time = last_action_time_[action];
    
    if (now - last_time < base::Seconds(1)) {
      return false;  // Rate limit: 1 action per second
    }
    
    last_time = now;
    return true;
  }
  
 private:
  std::map<std::string, base::Time> last_action_time_;
};

// Usage
void DappnetSettingsHandler::RestartGateway(RestartGatewayCallback callback) {
  if (!rate_limiter_.ShouldAllow("restart_gateway")) {
    std::move(callback).Run(false, "Too many requests. Please wait.");
    return;
  }
  // ... actual restart logic
}
```

3. **Process Sandboxing** (`local_gateway_controller.cc`)
```cpp
base::LaunchOptions GetSandboxedLaunchOptions() {
  base::LaunchOptions options;
  
#if defined(OS_LINUX)
  // Use namespace sandbox
  options.allow_new_privs = false;
  options.kill_on_parent_death = true;
  
  // Restrict capabilities
  options.capabilities = CAP_NET_BIND_SERVICE;
#endif
  
#if defined(OS_MAC)
  // Use sandbox profile
  options.sandbox_profile = GetSandboxProfile();
#endif
  
  return options;
}
```

4. **Performance Monitoring**
```cpp
class PerformanceMonitor {
 public:
  void RecordPageLoad(base::TimeDelta load_time) {
    UMA_HISTOGRAM_TIMES("Dappnet.Settings.PageLoadTime", load_time);
  }
  
  void RecordProcessRestart(base::TimeDelta restart_time) {
    UMA_HISTOGRAM_TIMES("Dappnet.Settings.ProcessRestartTime", restart_time);
  }
  
  void RecordRpcTest(base::TimeDelta test_time) {
    UMA_HISTOGRAM_TIMES("Dappnet.Settings.RpcTestTime", test_time);
  }
};
```

### Verification
- Test with malicious input (XSS attempts, invalid URLs)
- Verify rate limiting works
- Check process runs with limited permissions
- Monitor performance metrics in chrome://histograms

---

## Final Integration Checklist

### Code Review Points
- [ ] All user input is validated
- [ ] No hardcoded secrets or keys
- [ ] Proper error handling throughout
- [ ] Memory leaks checked with ASAN
- [ ] Thread safety verified

### Documentation
- [ ] Code comments for complex logic
- [ ] API documentation for Mojo interfaces
- [ ] User-facing help text and tooltips
- [ ] Architecture diagram in docs/

### Build System
- [ ] Added to all necessary BUILD.gn files
- [ ] Resources registered in .grd files
- [ ] Compiles on all platforms (Win/Mac/Linux)
- [ ] No missing dependencies

### Testing
- [ ] Unit tests pass
- [ ] Browser tests pass
- [ ] Manual testing completed
- [ ] Edge cases covered

### Performance
- [ ] Page loads < 500ms
- [ ] Process restart < 3s
- [ ] RPC connection test < 2s
- [ ] Memory usage < 10MB

### Security
- [ ] Input validation implemented
- [ ] XSS protection verified
- [ ] Process sandboxing enabled
- [ ] No privilege escalation possible

---

## Appendix: File Structure Summary

```
chrome/
├── browser/
│   ├── resources/
│   │   └── dappnet_settings/
│   │       ├── dappnet_settings.html
│   │       ├── dappnet_settings.js
│   │       ├── dappnet_settings.css
│   │       └── dappnet_settings_api.js
│   ├── ui/
│   │   └── webui/
│   │       └── dappnet/
│   │           ├── dappnet_settings_ui.h/cc
│   │           └── dappnet_settings_handler.h/cc
│   └── dappnet/
│       ├── mojom/
│       │   └── dappnet_settings.mojom
│       ├── local_gateway_controller.h/cc
│       └── ipfs_controller.h/cc
└── test/
    └── data/
        └── webui/
            └── dappnet/
                ├── dappnet_settings_test.js
                └── dappnet_settings_browsertest.cc
```