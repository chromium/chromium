// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_handler.h"

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/devtools/browser_devtools_agent_host.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/render_frame_devtools_agent_host.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace protocol {

namespace {

constexpr net::NetworkTrafficAnnotationTag
    kSettingsProxyConfigTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("devtools_proxy_config", R"(
      semantics {
        sender: "Proxy Configuration over Developer Tools"
        description:
          "Used to fetch HTTP/HTTPS/SOCKS5/PAC proxy configuration when "
          "proxy is configured by DevTools. It is equivalent to the one "
          "configured via the --proxy-server command line flag. "
          "When proxy implies automatic configuration, it can send network "
          "requests in the scope of this annotation."
        trigger:
          "Whenever a network request is made when the system proxy settings "
          "are used, and they indicate to use a proxy server."
        data:
          "Proxy configuration."
        destination: OTHER
        destination_other: "The proxy server specified in the configuration."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with '--remote-debugging-*' switches "
          "and does not explicitly send this data over Chrome remote debugging."
        policy_exception_justification:
          "Not implemented, only used in DevTools and is behind a switch."
      })");

static const char kNotAllowedError[] = "Not allowed";
static const char kMethod[] = "method";
static const char kResumeMethod[] = "Runtime.runIfWaitingForDebugger";

static const char kInitializerScript[] = R"(
  (function() {
    const bindingName = "%s";
    const binding = window[bindingName];
    delete window[bindingName];
    if (window.self === window.top) {
      window[bindingName] = {
        onmessage: () => {},
        send: binding
      };
    }
  })();
)";

std::unique_ptr<Target::TargetInfo> CreateInfo(DevToolsAgentHost* host) {
  std::unique_ptr<Target::TargetInfo> target_info =
      Target::TargetInfo::Create()
          .SetTargetId(host->GetId())
          .SetTitle(host->GetTitle())
          .SetUrl(host->GetURL().spec())
          .SetType(host->GetType())
          .SetAttached(host->IsAttached())
          .SetCanAccessOpener(host->CanAccessOpener())
          .Build();
  if (!host->GetOpenerId().empty())
    target_info->SetOpenerId(host->GetOpenerId());
  if (!host->GetOpenerFrameId().empty())
    target_info->SetOpenerFrameId(host->GetOpenerFrameId());
  if (host->GetBrowserContext())
    target_info->SetBrowserContextId(host->GetBrowserContext()->UniqueId());
  return target_info;
}

static std::string TerminationStatusToString(base::TerminationStatus status) {
  switch (status) {
    case base::TERMINATION_STATUS_NORMAL_TERMINATION:
      return "normal";
    case base::TERMINATION_STATUS_ABNORMAL_TERMINATION:
      return "abnormal";
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED:
      return "killed";
    case base::TERMINATION_STATUS_PROCESS_CRASHED:
      return "crashed";
    case base::TERMINATION_STATUS_STILL_RUNNING:
      return "still running";
#if defined(OS_CHROMEOS) || BUILDFLAG(IS_LACROS)
    // Used for the case when oom-killer kills a process on ChromeOS.
    case base::TERMINATION_STATUS_PROCESS_WAS_KILLED_BY_OOM:
      return "oom killed";
#endif
#if defined(OS_ANDROID)
    // On Android processes are spawned from the system Zygote and we do not get
    // the termination status.  We can't know if the termination was a crash or
    // an oom kill for sure: but we can use status of the strong process
    // bindings as a hint.
    case base::TERMINATION_STATUS_OOM_PROTECTED:
      return "oom protected";
#endif
    case base::TERMINATION_STATUS_LAUNCH_FAILED:
      return "failed to launch";
    case base::TERMINATION_STATUS_OOM:
      return "oom";
#if defined(OS_WIN)
    case base::TERMINATION_STATUS_INTEGRITY_FAILURE:
      return "integrity failure";
#endif
    case base::TERMINATION_STATUS_MAX_ENUM:
      break;
  }
  NOTREACHED() << "Unknown Termination Status.";
  return "unknown";
}

class BrowserToPageConnector;

base::LazyInstance<base::flat_map<DevToolsAgentHost*,
                                  std::unique_ptr<BrowserToPageConnector>>>::
    Leaky g_browser_to_page_connectors;

class BrowserToPageConnector {
 public:
  class BrowserConnectorHostClient : public DevToolsAgentHostClient {
   public:
    BrowserConnectorHostClient(BrowserToPageConnector* connector,
                               DevToolsAgentHost* host)
        : connector_(connector) {
      // TODO(dgozman): handle return value of AttachClient.
      host->AttachClient(this);
    }
    void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                                 base::span<const uint8_t> message) override {
      connector_->DispatchProtocolMessage(agent_host, message);
    }
    void AgentHostClosed(DevToolsAgentHost* agent_host) override {
      connector_->AgentHostClosed(agent_host);
    }

   private:
    BrowserToPageConnector* connector_;
    DISALLOW_COPY_AND_ASSIGN(BrowserConnectorHostClient);
  };

  BrowserToPageConnector(const std::string& binding_name,
                         DevToolsAgentHost* page_host)
      : binding_name_(binding_name), page_host_(page_host) {
    browser_host_ = BrowserDevToolsAgentHost::CreateForDiscovery();
    browser_host_client_ =
        std::make_unique<BrowserConnectorHostClient>(this, browser_host_.get());
    page_host_client_ =
        std::make_unique<BrowserConnectorHostClient>(this, page_host_.get());

    SendProtocolMessageToPage("Page.enable", std::make_unique<base::Value>());
    SendProtocolMessageToPage("Runtime.enable",
                              std::make_unique<base::Value>());

    std::unique_ptr<base::DictionaryValue> add_binding_params =
        std::make_unique<base::DictionaryValue>();
    add_binding_params->SetString("name", binding_name);
    SendProtocolMessageToPage("Runtime.addBinding",
                              std::move(add_binding_params));

    std::string initializer_script =
        base::StringPrintf(kInitializerScript, binding_name.c_str());

    std::unique_ptr<base::DictionaryValue> params =
        std::make_unique<base::DictionaryValue>();
    params->SetString("scriptSource", initializer_script);
    SendProtocolMessageToPage("Page.addScriptToEvaluateOnLoad",
                              std::move(params));

    std::unique_ptr<base::DictionaryValue> evaluate_params =
        std::make_unique<base::DictionaryValue>();
    evaluate_params->SetString("expression", initializer_script);
    SendProtocolMessageToPage("Runtime.evaluate", std::move(evaluate_params));
    g_browser_to_page_connectors.Get()[page_host_.get()].reset(this);
  }

 private:
  void SendProtocolMessageToPage(const char* method,
                                 std::unique_ptr<base::Value> params) {
    base::DictionaryValue message;
    message.SetInteger("id", page_message_id_++);
    message.SetString("method", method);
    message.Set("params", std::move(params));
    std::string json_message;
    base::JSONWriter::Write(message, &json_message);
    page_host_->DispatchProtocolMessage(
        page_host_client_.get(), base::as_bytes(base::make_span(json_message)));
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) {
    base::StringPiece message_sp(reinterpret_cast<const char*>(message.data()),
                                 message.size());
    if (agent_host == page_host_.get()) {
      std::unique_ptr<base::Value> value =
          base::JSONReader::ReadDeprecated(message_sp);
      if (!value || !value->is_dict())
        return;
      // Make sure this is a binding call.
      base::Value* method = value->FindKey("method");
      if (!method || !method->is_string() ||
          method->GetString() != "Runtime.bindingCalled")
        return;
      base::Value* params = value->FindKey("params");
      if (!params || !params->is_dict())
        return;
      base::Value* name = params->FindKey("name");
      if (!name || !name->is_string() || name->GetString() != binding_name_)
        return;
      base::Value* payload = params->FindKey("payload");
      if (!payload || !payload->is_string())
        return;
      const std::string& payload_str = payload->GetString();
      browser_host_->DispatchProtocolMessage(
          browser_host_client_.get(),
          base::as_bytes(base::make_span(payload_str)));
      return;
    }
    DCHECK(agent_host == browser_host_.get());

    std::string encoded;
    base::Base64Encode(message_sp, &encoded);
    std::string eval_code =
        "try { window." + binding_name_ + ".onmessage(atob(\"";
    std::string eval_suffix = "\")); } catch(e) { console.error(e); }";
    eval_code.reserve(eval_code.size() + encoded.size() + eval_suffix.size());
    eval_code.append(encoded);
    eval_code.append(eval_suffix);

    auto params = std::make_unique<base::DictionaryValue>();
    params->SetString("expression", std::move(eval_code));
    SendProtocolMessageToPage("Runtime.evaluate", std::move(params));
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) {
    if (agent_host == browser_host_.get()) {
      page_host_->DetachClient(page_host_client_.get());
    } else {
      DCHECK(agent_host == page_host_.get());
      browser_host_->DetachClient(browser_host_client_.get());
    }
    g_browser_to_page_connectors.Get().erase(page_host_.get());
  }

  std::string binding_name_;
  scoped_refptr<DevToolsAgentHost> browser_host_;
  scoped_refptr<DevToolsAgentHost> page_host_;
  std::unique_ptr<BrowserConnectorHostClient> browser_host_client_;
  std::unique_ptr<BrowserConnectorHostClient> page_host_client_;
  int page_message_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(BrowserToPageConnector);
};

}  // namespace

// Throttle is owned externally by the navigation subsystem.
class TargetHandler::Throttle : public content::NavigationThrottle {
 public:
  ~Throttle() override;
  void Clear();
  // content::NavigationThrottle implementation:
  const char* GetNameForLogging() override;

 protected:
  Throttle(base::WeakPtr<protocol::TargetHandler> target_handler,
           content::NavigationHandle* navigation_handle);
  void SetThrottledAgentHost(DevToolsAgentHost* agent_host);

  bool is_deferring_ = false;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  base::WeakPtr<protocol::TargetHandler> target_handler_;

 private:
  void CleanupPointers();

  DISALLOW_COPY_AND_ASSIGN(Throttle);
};

class TargetHandler::ResponseThrottle : public TargetHandler::Throttle {
 public:
  ResponseThrottle(base::WeakPtr<protocol::TargetHandler> target_handler,
                   content::NavigationHandle* navigation_handle)
      : Throttle(target_handler, navigation_handle) {}
  ~ResponseThrottle() override = default;

 private:
  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillProcessResponse() override { return MaybeThrottle(); }

  ThrottleCheckResult WillFailRequest() override { return MaybeThrottle(); }

  ThrottleCheckResult MaybeThrottle() {
    if (target_handler_) {
      NavigationRequest* request = NavigationRequest::From(navigation_handle());
      SetThrottledAgentHost(
          target_handler_->auto_attacher_.AutoAttachToFrame(request));
    }
    is_deferring_ = !!agent_host_;
    return is_deferring_ ? DEFER : PROCEED;
  }
};

class TargetHandler::RequestThrottle : public TargetHandler::Throttle {
 public:
  RequestThrottle(base::WeakPtr<protocol::TargetHandler> target_handler,
                  content::NavigationHandle* navigation_handle,
                  DevToolsAgentHost* throttled_agent_host)
      : Throttle(target_handler, navigation_handle) {
    SetThrottledAgentHost(throttled_agent_host);
  }
  ~RequestThrottle() override = default;

 private:
  // content::NavigationThrottle implementation:
  ThrottleCheckResult WillStartRequest() override {
    is_deferring_ = !!agent_host_;
    return is_deferring_ ? DEFER : PROCEED;
  }
};

class TargetHandler::Session : public DevToolsAgentHostClient {
 public:
  static std::string Attach(TargetHandler* handler,
                            DevToolsAgentHost* agent_host,
                            bool waiting_for_debugger,
                            bool flatten_protocol) {
    std::string id = base::UnguessableToken::Create().ToString();
    // We don't support or allow the non-flattened protocol when in binary mode.
    // So, we coerce the setting to true, as the non-flattened mode is
    // deprecated anyway.
    if (handler->root_session_->GetClient()->UsesBinaryProtocol())
      flatten_protocol = true;
    Session* session = new Session(handler, agent_host, id, flatten_protocol);
    handler->attached_sessions_[id].reset(session);
    DevToolsAgentHostImpl* agent_host_impl =
        static_cast<DevToolsAgentHostImpl*>(agent_host);
    if (flatten_protocol) {
      DevToolsSession* devtools_session =
          handler->root_session_->AttachChildSession(id, agent_host_impl,
                                                     session);
      if (waiting_for_debugger && devtools_session) {
        devtools_session->SetRuntimeResumeCallback(base::BindOnce(
            &Session::ResumeIfThrottled, base::Unretained(session)));
        session->devtools_session_ = devtools_session;
      }
    } else {
      agent_host_impl->AttachClient(session);
    }
    handler->frontend_->AttachedToTarget(id, CreateInfo(agent_host),
                                         waiting_for_debugger);
    return id;
  }

  ~Session() override {
    if (!agent_host_)
      return;
    if (flatten_protocol_)
      handler_->root_session_->DetachChildSession(id_);
    agent_host_->DetachClient(this);
  }

  void Detach(bool host_closed) {
    handler_->frontend_->DetachedFromTarget(id_, agent_host_->GetId());
    if (flatten_protocol_)
      handler_->root_session_->DetachChildSession(id_);
    if (host_closed)
      handler_->auto_attacher_.AgentHostClosed(agent_host_.get());
    else
      agent_host_->DetachClient(this);
    handler_->auto_attached_sessions_.erase(agent_host_.get());
    devtools_session_ = nullptr;
    agent_host_ = nullptr;
    handler_->attached_sessions_.erase(id_);
  }

  bool IsWaitingForDebuggerOnStart() const {
    return devtools_session_ &&
           devtools_session_->IsWaitingForDebuggerOnStart();
  }

  void ResumeSendingMessagesToAgent() const {
    if (devtools_session_)
      devtools_session_->ResumeSendingMessagesToAgent();
  }

  void SetThrottle(Throttle* throttle) { throttle_ = throttle; }

  void ResumeIfThrottled() {
    if (throttle_)
      throttle_->Clear();
  }

  void SendMessageToAgentHost(base::span<const uint8_t> message) {
    // This method is only used in the non-flat mode, it's the implementation
    // for Target.SendMessageToTarget. And since the binary mode implies
    // flatten_protocol_ (we force the flag to true), we can assume in this
    // method that |message| is JSON.
    DCHECK(!flatten_protocol_);

    if (throttle_) {
      base::Optional<base::Value> value =
          base::JSONReader::Read(base::StringPiece(
              reinterpret_cast<const char*>(message.data()), message.size()));
      const std::string* method;
      if (value.has_value() && (method = value->FindStringKey(kMethod)) &&
          *method == kResumeMethod) {
        throttle_->Clear();
      }
    }

    agent_host_->DispatchProtocolMessage(this, message);
  }

  bool IsAttachedTo(const std::string& target_id) {
    return agent_host_->GetId() == target_id;
  }

  bool UsesBinaryProtocol() override {
    return handler_->root_session_->GetClient()->UsesBinaryProtocol();
  }

 private:
  friend class TargetHandler;

  Session(TargetHandler* handler,
          DevToolsAgentHost* agent_host,
          const std::string& id,
          bool flatten_protocol)
      : handler_(handler),
        agent_host_(agent_host),
        id_(id),
        flatten_protocol_(flatten_protocol) {}

  // DevToolsAgentHostClient implementation.
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               base::span<const uint8_t> message) override {
    DCHECK(agent_host == agent_host_.get());
    if (flatten_protocol_) {
      // TODO(johannes): It's not clear that this check is useful, but
      // a similar check has been in the code ever since the flattened protocol
      // was introduced. Try a DCHECK instead and possibly remove the check.
      if (!handler_->root_session_->HasChildSession(id_))
        return;
      GetRootClient()->DispatchProtocolMessage(
          handler_->root_session_->GetAgentHost(), message);
      return;
    }
    // TODO(johannes): For now, We need to copy here because
    // ReceivedMessageFromTarget is generated code and we're using const
    // std::string& for such parameters. Perhaps we should switch this to
    // base::StringPiece?
    std::string message_copy(message.begin(), message.end());
    handler_->frontend_->ReceivedMessageFromTarget(id_, message_copy,
                                                   agent_host_->GetId());
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    DCHECK(agent_host == agent_host_.get());
    Detach(true);
  }

  bool MayAttachToURL(const GURL& url, bool is_webui) override {
    return GetRootClient()->MayAttachToURL(url, is_webui);
  }

  bool MayAttachToBrowser() override {
    return GetRootClient()->MayAttachToBrowser();
  }

  bool MayReadLocalFiles() override {
    return GetRootClient()->MayReadLocalFiles();
  }

  bool MayWriteLocalFiles() override {
    return GetRootClient()->MayWriteLocalFiles();
  }

  content::DevToolsAgentHostClient* GetRootClient() {
    return handler_->root_session_->GetClient();
  }

  TargetHandler* handler_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string id_;
  bool flatten_protocol_;
  DevToolsSession* devtools_session_ = nullptr;
  Throttle* throttle_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

TargetHandler::Throttle::Throttle(
    base::WeakPtr<protocol::TargetHandler> target_handler,
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle),
      target_handler_(target_handler) {
  target_handler->throttles_.insert(this);
}

TargetHandler::Throttle::~Throttle() {
  CleanupPointers();
}

void TargetHandler::Throttle::CleanupPointers() {
  if (target_handler_ && agent_host_) {
    auto it = target_handler_->auto_attached_sessions_.find(agent_host_.get());
    if (it != target_handler_->auto_attached_sessions_.end())
      it->second->SetThrottle(nullptr);
  }
  if (target_handler_) {
    target_handler_->throttles_.erase(this);
    target_handler_ = nullptr;
  }
}

void TargetHandler::Throttle::SetThrottledAgentHost(
    DevToolsAgentHost* agent_host) {
  agent_host_ = agent_host;
  if (agent_host_) {
    target_handler_->auto_attached_sessions_[agent_host_.get()]->SetThrottle(
        this);
  }
}

const char* TargetHandler::Throttle::GetNameForLogging() {
  return "DevToolsTargetNavigationThrottle";
}

void TargetHandler::Throttle::Clear() {
  CleanupPointers();
  agent_host_ = nullptr;
  if (is_deferring_)
    Resume();
  is_deferring_ = false;
}

TargetHandler::TargetHandler(AccessMode access_mode,
                             const std::string& owner_target_id,
                             DevToolsRendererChannel* renderer_channel,
                             DevToolsSession* root_session)
    : DevToolsDomainHandler(Target::Metainfo::domainName),
      auto_attacher_(this, renderer_channel),
      discover_(false),
      access_mode_(access_mode),
      owner_target_id_(owner_target_id),
      root_session_(root_session) {}

TargetHandler::~TargetHandler() = default;

// static
std::vector<TargetHandler*> TargetHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<TargetHandler>(Target::Metainfo::domainName);
}

void TargetHandler::Wire(UberDispatcher* dispatcher) {
  frontend_.reset(new Target::Frontend(dispatcher->channel()));
  Target::Dispatcher::wire(dispatcher, this);
}

void TargetHandler::SetRenderer(int process_host_id,
                                RenderFrameHostImpl* frame_host) {
  auto_attacher_.SetRenderFrameHost(frame_host);
}

Response TargetHandler::Disable() {
  SetAutoAttachInternal(false, false, false, base::DoNothing());
  SetDiscoverTargets(false);
  auto_attached_sessions_.clear();
  attached_sessions_.clear();

  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Success();

  if (dispose_on_detach_context_ids_.size()) {
    for (auto* context : delegate->GetBrowserContexts()) {
      if (!dispose_on_detach_context_ids_.contains(context->UniqueId()))
        continue;
      delegate->DisposeBrowserContext(
          context,
          base::BindOnce([](bool success, const std::string& error) {}));
    }
    dispose_on_detach_context_ids_.clear();
  }
  contexts_with_overridden_proxy_.clear();
  return Response::Success();
}

void TargetHandler::DidFinishNavigation() {
  auto_attacher_.UpdateServiceWorkers();
}

std::unique_ptr<NavigationThrottle> TargetHandler::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  if (!auto_attacher_.ShouldThrottleFramesNavigation())
    return nullptr;
  if (access_mode_ == AccessMode::kBrowser) {
    FrameTreeNode* frame_tree_node =
        NavigationRequest::From(navigation_handle)->frame_tree_node();
    // Top-level target handler is expected to create throttles only for new
    // pages.
    DCHECK(!frame_tree_node->parent());
    DevToolsAgentHost* host =
        RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
    // For new pages create Throttle only if the session is still paused.
    if (!host)
      return nullptr;
    auto it = auto_attached_sessions_.find(host);
    if (it == auto_attached_sessions_.end())
      return nullptr;
    if (!it->second->IsWaitingForDebuggerOnStart())
      return nullptr;
    // The target already exists and we attached to it, resume message
    // sending without waiting for the navigation to commit.
    it->second->ResumeSendingMessagesToAgent();

    // window.open() navigations are throttled on the renderer side and the main
    // request will not be sent until runIfWaitingForDebugger is received from
    // the client, so there is no need to throttle the navigation in the
    // browser.
    //
    // New window navigations (such as ctrl+click) should be throttled before
    // the main request is sent to apply user agent and other overrides.
    FrameTreeNode* opener = frame_tree_node->opener();
    if (opener)
      return nullptr;
    return std::make_unique<RequestThrottle>(weak_factory_.GetWeakPtr(),
                                             navigation_handle, host);
  }
  return std::make_unique<ResponseThrottle>(weak_factory_.GetWeakPtr(),
                                            navigation_handle);
}

void TargetHandler::UpdatePortals() {
  auto_attacher_.UpdatePortals();
}

void TargetHandler::ClearThrottles() {
  base::flat_set<Throttle*> copy(throttles_);
  for (Throttle* throttle : copy)
    throttle->Clear();
  throttles_.clear();
}

void TargetHandler::SetAutoAttachInternal(bool auto_attach,
                                          bool wait_for_debugger_on_start,
                                          bool flatten,
                                          base::OnceClosure callback) {
  flatten_auto_attach_ = flatten;
  auto_attacher_.SetAutoAttach(auto_attach, wait_for_debugger_on_start,
                               std::move(callback));
  if (!auto_attacher_.ShouldThrottleFramesNavigation())
    ClearThrottles();

  UpdateAgentHostObserver();
}

void TargetHandler::UpdateAgentHostObserver() {
  bool should_observe =
      discover_ || (access_mode_ == AccessMode::kBrowser &&
                    auto_attacher_.ShouldThrottleFramesNavigation());
  if (should_observe == observing_agent_hosts_)
    return;
  observing_agent_hosts_ = should_observe;
  if (should_observe)
    DevToolsAgentHost::AddObserver(this);
  else
    DevToolsAgentHost::RemoveObserver(this);
}

void TargetHandler::AutoAttach(DevToolsAgentHost* host,
                               bool waiting_for_debugger) {
  std::string session_id =
      Session::Attach(this, host, waiting_for_debugger, flatten_auto_attach_);
  auto_attached_sessions_[host] = attached_sessions_[session_id].get();
}

void TargetHandler::AutoDetach(DevToolsAgentHost* host) {
  auto it = auto_attached_sessions_.find(host);
  if (it == auto_attached_sessions_.end())
    return;
  it->second->Detach(false);
}

bool TargetHandler::ShouldThrottlePopups() const {
  return auto_attacher_.ShouldThrottleFramesNavigation();
}

Response TargetHandler::FindSession(Maybe<std::string> session_id,
                                    Maybe<std::string> target_id,
                                    Session** session) {
  *session = nullptr;
  if (session_id.isJust()) {
    auto it = attached_sessions_.find(session_id.fromJust());
    if (it == attached_sessions_.end())
      return Response::InvalidParams("No session with given id");
    *session = it->second.get();
    return Response::Success();
  }
  if (target_id.isJust()) {
    for (auto& it : attached_sessions_) {
      if (it.second->IsAttachedTo(target_id.fromJust())) {
        if (*session)
          return Response::ServerError(
              "Multiple sessions attached, specify id.");
        *session = it.second.get();
      }
    }
    if (!*session)
      return Response::InvalidParams("No session for given target id");
    return Response::Success();
  }
  return Response::InvalidParams("Session id must be specified");
}

// ----------------- Protocol ----------------------

Response TargetHandler::SetDiscoverTargets(bool discover) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  if (discover_ == discover)
    return Response::Success();
  discover_ = discover;
  UpdateAgentHostObserver();
  if (!discover_)
    reported_hosts_.clear();
  return Response::Success();
}

void TargetHandler::SetAutoAttach(
    bool auto_attach,
    bool wait_for_debugger_on_start,
    Maybe<bool> flatten,
    std::unique_ptr<SetAutoAttachCallback> callback) {
  if (access_mode_ == AccessMode::kBrowser && !flatten.fromMaybe(false)) {
    callback->sendFailure(Response::InvalidParams(
        "Only flatten protocol is supported with browser level auto-attach"));
    return;
  }
  SetAutoAttachInternal(
      auto_attach, wait_for_debugger_on_start, flatten.fromMaybe(false),
      base::BindOnce(&SetAutoAttachCallback::sendSuccess, std::move(callback)));
}

Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<Target::RemoteLocation>>) {
  return Response::ServerError("Not supported");
}

Response TargetHandler::AttachToTarget(const std::string& target_id,
                                       Maybe<bool> flatten,
                                       std::string* out_session_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *out_session_id =
      Session::Attach(this, agent_host.get(), false, flatten.fromMaybe(false));
  return Response::Success();
}

Response TargetHandler::AttachToBrowserTarget(std::string* out_session_id) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::ServerError(kNotAllowedError);
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::CreateForBrowser(
          nullptr, DevToolsAgentHost::CreateServerSocketCallback());
  *out_session_id = Session::Attach(this, agent_host.get(), false, true);
  return Response::Success();
}

Response TargetHandler::DetachFromTarget(Maybe<std::string> session_id,
                                         Maybe<std::string> target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  Session* session = nullptr;
  Response response =
      FindSession(std::move(session_id), std::move(target_id), &session);
  if (!response.IsSuccess())
    return response;
  session->Detach(false);
  return Response::Success();
}

Response TargetHandler::SendMessageToTarget(const std::string& message,
                                            Maybe<std::string> session_id,
                                            Maybe<std::string> target_id) {
  Session* session = nullptr;
  Response response =
      FindSession(std::move(session_id), std::move(target_id), &session);
  if (!response.IsSuccess())
    return response;
  if (session->flatten_protocol_) {
    return Response::ServerError(
        "When using flat protocol, messages are routed to the target "
        "via the sessionId attribute.");
  }
  session->SendMessageToAgentHost(base::as_bytes(base::make_span(message)));
  return Response::Success();
}

Response TargetHandler::GetTargetInfo(
    Maybe<std::string> maybe_target_id,
    std::unique_ptr<Target::TargetInfo>* target_info) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  const std::string& target_id =
      maybe_target_id.isJust() ? maybe_target_id.fromJust() : owner_target_id_;
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *target_info = CreateInfo(agent_host.get());
  return Response::Success();
}

Response TargetHandler::ActivateTarget(const std::string& target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  agent_host->Activate();
  return Response::Success();
}

Response TargetHandler::CloseTarget(const std::string& target_id,
                                    bool* out_success) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  if (!agent_host->Close())
    return Response::InvalidParams("Specified target doesn't support closing");
  *out_success = true;
  return Response::Success();
}

Response TargetHandler::ExposeDevToolsProtocol(
    const std::string& target_id,
    Maybe<std::string> binding_name) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::InvalidParams(kNotAllowedError);
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");

  if (g_browser_to_page_connectors.Get()[agent_host.get()]) {
    return Response::ServerError(base::StringPrintf(
        "Target with id %s is already granted remote debugging bindings.",
        target_id.c_str()));
  }
  if (!agent_host->GetWebContents()) {
    return Response::ServerError(
        "RemoteDebuggingBinding can be granted only to page targets");
  }

  new BrowserToPageConnector(binding_name.fromMaybe("cdp"), agent_host.get());
  return Response::Success();
}

Response TargetHandler::CreateTarget(const std::string& url,
                                     Maybe<int> width,
                                     Maybe<int> height,
                                     Maybe<std::string> context_id,
                                     Maybe<bool> enable_begin_frame_control,
                                     Maybe<bool> new_window,
                                     Maybe<bool> background,
                                     std::string* out_target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::ServerError("Not supported");
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      delegate->CreateNewTarget(GURL(url));
  if (!agent_host)
    return Response::ServerError("Not supported");
  *out_target_id = agent_host->GetId();
  return Response::Success();
}

Response TargetHandler::GetTargets(
    std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::ServerError(kNotAllowedError);
  *target_infos = std::make_unique<protocol::Array<Target::TargetInfo>>();
  for (const auto& host : DevToolsAgentHost::GetOrCreateAll())
    (*target_infos)->emplace_back(CreateInfo(host.get()));
  return Response::Success();
}

// -------------- DevToolsAgentHostObserver -----------------

bool TargetHandler::ShouldForceDevToolsAgentHostCreation() {
  return true;
}

static bool IsMainFrameHost(DevToolsAgentHost* host) {
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(host->GetWebContents());
  if (!web_contents)
    return false;
  FrameTreeNode* frame_tree_node = web_contents->GetFrameTree()->root();
  if (!frame_tree_node)
    return false;
  return host == RenderFrameDevToolsAgentHost::GetFor(frame_tree_node);
}

void TargetHandler::DevToolsAgentHostCreated(DevToolsAgentHost* host) {
  if (discover_) {
    // If we start discovering late, all existing agent hosts will be reported,
    // but we could have already attached to some.
    if (reported_hosts_.find(host) == reported_hosts_.end()) {
      frontend_->TargetCreated(CreateInfo(host));
      reported_hosts_.insert(host);
    }
  }
  // In the top level target handler auto-attach to pages as soon as they
  // are created, otherwise if they don't incur any network activity we'll
  // never get a chance to throttle them (and auto-attach there).
  if (access_mode_ == AccessMode::kBrowser &&
      auto_attacher_.ShouldThrottleFramesNavigation() &&
      IsMainFrameHost(host)) {
    auto_attacher_.AttachToAgentHost(host);
  }
}

void TargetHandler::DevToolsAgentHostNavigated(DevToolsAgentHost* host) {
  if (reported_hosts_.find(host) == reported_hosts_.end())
    return;
  frontend_->TargetInfoChanged(CreateInfo(host));
}

void TargetHandler::DevToolsAgentHostDestroyed(DevToolsAgentHost* host) {
  if (reported_hosts_.find(host) == reported_hosts_.end())
    return;
  frontend_->TargetDestroyed(host->GetId());
  reported_hosts_.erase(host);
}

void TargetHandler::DevToolsAgentHostAttached(DevToolsAgentHost* host) {
  if (reported_hosts_.find(host) == reported_hosts_.end())
    return;
  frontend_->TargetInfoChanged(CreateInfo(host));
}

void TargetHandler::DevToolsAgentHostDetached(DevToolsAgentHost* host) {
  if (reported_hosts_.find(host) == reported_hosts_.end())
    return;
  frontend_->TargetInfoChanged(CreateInfo(host));
}

void TargetHandler::DevToolsAgentHostCrashed(DevToolsAgentHost* host,
                                             base::TerminationStatus status) {
  if (reported_hosts_.find(host) == reported_hosts_.end())
    return;
  frontend_->TargetCrashed(host->GetId(), TerminationStatusToString(status),
                           host->GetWebContents()
                               ? host->GetWebContents()->GetCrashedErrorCode()
                               : 0);
}

// ----------------- More protocol methods -------------------

void TargetHandler::CreateBrowserContext(
    Maybe<bool> in_disposeOnDetach,
    Maybe<String> in_proxyServer,
    Maybe<String> in_proxyBypassList,
    std::unique_ptr<CreateBrowserContextCallback> callback) {
  if (access_mode_ != AccessMode::kBrowser) {
    callback->sendFailure(Response::ServerError(kNotAllowedError));
    return;
  }
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate) {
    callback->sendFailure(
        Response::ServerError("Browser context management is not supported."));
    return;
  }

  if (in_proxyServer.isJust()) {
    pending_proxy_config_ = net::ProxyConfig();
    pending_proxy_config_->proxy_rules().ParseFromString(
        in_proxyServer.fromJust());
    if (in_proxyBypassList.isJust()) {
      pending_proxy_config_->proxy_rules().bypass_rules.ParseFromString(
          in_proxyBypassList.fromJust());
    }
  }

  BrowserContext* context = delegate->CreateBrowserContext();
  if (!context) {
    callback->sendFailure(
        Response::ServerError("Failed to create browser context."));
    pending_proxy_config_.reset();
    return;
  }

  if (pending_proxy_config_) {
    contexts_with_overridden_proxy_[context->UniqueId()] =
        std::move(*pending_proxy_config_);
    pending_proxy_config_.reset();
  }

  if (in_disposeOnDetach.fromMaybe(false))
    dispose_on_detach_context_ids_.insert(context->UniqueId());
  callback->sendSuccess(context->UniqueId());
}

protocol::Response TargetHandler::GetBrowserContexts(
    std::unique_ptr<protocol::Array<protocol::String>>* browser_context_ids) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::ServerError(kNotAllowedError);
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::ServerError(
        "Browser context management is not supported.");
  std::vector<content::BrowserContext*> contexts =
      delegate->GetBrowserContexts();
  *browser_context_ids = std::make_unique<protocol::Array<protocol::String>>();
  for (auto* context : contexts)
    (*browser_context_ids)->emplace_back(context->UniqueId());
  return Response::Success();
}

void TargetHandler::DisposeBrowserContext(
    const std::string& context_id,
    std::unique_ptr<DisposeBrowserContextCallback> callback) {
  if (access_mode_ != AccessMode::kBrowser) {
    callback->sendFailure(Response::ServerError(kNotAllowedError));
    return;
  }
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate) {
    callback->sendFailure(
        Response::ServerError("Browser context management is not supported."));
    return;
  }
  std::vector<content::BrowserContext*> contexts =
      delegate->GetBrowserContexts();
  auto context_it =
      std::find_if(contexts.begin(), contexts.end(),
                   [&context_id](content::BrowserContext* context) {
                     return context->UniqueId() == context_id;
                   });
  if (context_it == contexts.end()) {
    callback->sendFailure(
        Response::ServerError("Failed to find context with id " + context_id));
    return;
  }
  dispose_on_detach_context_ids_.erase(context_id);
  delegate->DisposeBrowserContext(
      *context_it,
      base::BindOnce(
          [](std::unique_ptr<DisposeBrowserContextCallback> callback,
             bool success, const std::string& error) {
            if (success)
              callback->sendSuccess();
            else
              callback->sendFailure(Response::ServerError(error));
          },
          std::move(callback)));
}

void TargetHandler::ApplyNetworkContextParamsOverrides(
    BrowserContext* browser_context,
    network::mojom::NetworkContextParams* context_params) {
  // Under certain conditions, storage partition is created synchronously for
  // the browser context. Account for this use case.
  if (pending_proxy_config_) {
    context_params->initial_proxy_config =
        net::ProxyConfigWithAnnotation(std::move(*pending_proxy_config_),
                                       kSettingsProxyConfigTrafficAnnotation);
    pending_proxy_config_.reset();
    return;
  }
  auto it = contexts_with_overridden_proxy_.find(browser_context->UniqueId());
  if (it != contexts_with_overridden_proxy_.end()) {
    context_params->initial_proxy_config = net::ProxyConfigWithAnnotation(
        std::move(it->second), kSettingsProxyConfigTrafficAnnotation);
    contexts_with_overridden_proxy_.erase(browser_context->UniqueId());
  }
}

}  // namespace protocol
}  // namespace content
