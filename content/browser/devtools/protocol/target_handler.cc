// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/target_handler.h"

#include "base/base64.h"
#include "base/containers/flat_map.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/stringprintf.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "build/build_config.h"
#include "content/browser/devtools/browser_devtools_agent_host.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/devtools_manager.h"
#include "content/browser/devtools/target_registry.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host_client.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"

namespace content {
namespace protocol {

namespace {

const char kNotAllowedError[] = "Not allowed.";

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
          .Build();
  if (!host->GetOpenerId().empty())
    target_info->SetOpenerId(host->GetOpenerId());
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
#if defined(OS_CHROMEOS)
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
                                 const std::string& message) override {
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
    page_host_->DispatchProtocolMessage(page_host_client_.get(), json_message);
  }

  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) {
    if (agent_host == page_host_.get()) {
      std::unique_ptr<base::Value> value = base::JSONReader::Read(message);
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
      browser_host_->DispatchProtocolMessage(browser_host_client_.get(),
                                             payload->GetString());
      return;
    }
    DCHECK(agent_host == browser_host_.get());

    std::string encoded;
    base::Base64Encode(message, &encoded);
    std::string eval_code = "window." + binding_name_ + ".onmessage(atob(\"";
    std::string eval_suffix = "\"))";
    eval_code.reserve(eval_code.size() + encoded.size() + eval_suffix.size());
    eval_code.append(encoded);
    eval_code.append(eval_suffix);

    std::unique_ptr<base::DictionaryValue> params =
        std::make_unique<base::DictionaryValue>();
    params->SetString("expression", eval_code);
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
  Throttle(base::WeakPtr<protocol::TargetHandler> target_handler,
           content::NavigationHandle* navigation_handle);
  ~Throttle() override;
  void Clear();
  // content::NavigationThrottle implementation:
  NavigationThrottle::ThrottleCheckResult WillProcessResponse() override;
  NavigationThrottle::ThrottleCheckResult WillFailRequest() override;
  const char* GetNameForLogging() override;

 private:
  NavigationThrottle::ThrottleCheckResult MaybeAttach();
  void CleanupPointers();

  base::WeakPtr<protocol::TargetHandler> target_handler_;
  scoped_refptr<DevToolsAgentHost> agent_host_;

  DISALLOW_COPY_AND_ASSIGN(Throttle);
};

class TargetHandler::Session : public DevToolsAgentHostClient {
 public:
  static std::string Attach(TargetHandler* handler,
                            DevToolsAgentHost* agent_host,
                            bool waiting_for_debugger,
                            bool flatten_protocol) {
    std::string id = base::UnguessableToken::Create().ToString();
    Session* session = new Session(handler, agent_host, id, flatten_protocol);
    handler->attached_sessions_[id].reset(session);
    DevToolsAgentHostImpl* agent_host_impl =
        static_cast<DevToolsAgentHostImpl*>(agent_host);
    if (flatten_protocol) {
      handler->target_registry_->AttachSubtargetSession(
          id, agent_host_impl, session,
          base::BindOnce(&Session::ResumeIfThrottled,
                         base::Unretained(session)));
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
    if (handler_->target_registry_)
      handler_->target_registry_->DetachSubtargetSession(id_);
    agent_host_->DetachClient(this);
  }

  void Detach(bool host_closed) {
    handler_->frontend_->DetachedFromTarget(id_, agent_host_->GetId());
    if (host_closed)
      handler_->auto_attacher_.AgentHostClosed(agent_host_.get());
    else {
      if (handler_->target_registry_)
        handler_->target_registry_->DetachSubtargetSession(id_);
      agent_host_->DetachClient(this);
    }
    handler_->auto_attached_sessions_.erase(agent_host_.get());
    agent_host_ = nullptr;
    handler_->attached_sessions_.erase(id_);
  }

  void SetThrottle(Throttle* throttle) { throttle_ = throttle; }

  void ResumeIfThrottled() {
    if (throttle_)
      throttle_->Clear();
  }

  void SendMessageToAgentHost(const std::string& message) {
    if (throttle_) {
      std::unique_ptr<base::Value> value = base::JSONReader::Read(message);
      if (TargetRegistry::IsRuntimeResumeCommand(value.get()))
        ResumeIfThrottled();
    }

    agent_host_->DispatchProtocolMessage(this, message);
  }

  bool IsAttachedTo(const std::string& target_id) {
    return agent_host_->GetId() == target_id;
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
                               const std::string& message) override {
    DCHECK(agent_host == agent_host_.get());
    if (flatten_protocol_) {
      handler_->target_registry_->SendMessageToClient(id_, message);
      return;
    }

    handler_->frontend_->ReceivedMessageFromTarget(id_, message,
                                                   agent_host_->GetId());
  }

  void AgentHostClosed(DevToolsAgentHost* agent_host) override {
    DCHECK(agent_host == agent_host_.get());
    Detach(true);
  }

  TargetHandler* handler_;
  scoped_refptr<DevToolsAgentHost> agent_host_;
  std::string id_;
  bool flatten_protocol_;
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

NavigationThrottle::ThrottleCheckResult
TargetHandler::Throttle::WillProcessResponse() {
  return MaybeAttach();
}

NavigationThrottle::ThrottleCheckResult
TargetHandler::Throttle::WillFailRequest() {
  return MaybeAttach();
}

NavigationThrottle::ThrottleCheckResult TargetHandler::Throttle::MaybeAttach() {
  if (!target_handler_)
    return PROCEED;
  agent_host_ = target_handler_->auto_attacher_.AutoAttachToFrame(
      static_cast<NavigationHandleImpl*>(navigation_handle()));
  if (!agent_host_.get())
    return PROCEED;
  target_handler_->auto_attached_sessions_[agent_host_.get()]->SetThrottle(
      this);
  return DEFER;
}

const char* TargetHandler::Throttle::GetNameForLogging() {
  return "DevToolsTargetNavigationThrottle";
}

void TargetHandler::Throttle::Clear() {
  CleanupPointers();
  if (agent_host_) {
    agent_host_ = nullptr;
    Resume();
  }
}

TargetHandler::TargetHandler(AccessMode access_mode,
                             const std::string& owner_target_id,
                             TargetRegistry* target_registry)
    : DevToolsDomainHandler(Target::Metainfo::domainName),
      auto_attacher_(
          base::Bind(&TargetHandler::AutoAttach, base::Unretained(this)),
          base::Bind(&TargetHandler::AutoDetach, base::Unretained(this))),
      discover_(false),
      access_mode_(access_mode),
      owner_target_id_(owner_target_id),
      target_registry_(target_registry),
      weak_factory_(this) {}

TargetHandler::~TargetHandler() {
}

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
  SetAutoAttach(false, false, false);
  SetDiscoverTargets(false);
  auto_attached_sessions_.clear();
  attached_sessions_.clear();
  return Response::OK();
}

void TargetHandler::DidCommitNavigation() {
  auto_attacher_.UpdateServiceWorkers();
}

std::unique_ptr<NavigationThrottle> TargetHandler::CreateThrottleForNavigation(
    NavigationHandle* navigation_handle) {
  if (!auto_attacher_.ShouldThrottleFramesNavigation())
    return nullptr;
  return std::make_unique<Throttle>(weak_factory_.GetWeakPtr(),
                                    navigation_handle);
}

void TargetHandler::ClearThrottles() {
  base::flat_set<Throttle*> copy(throttles_);
  for (Throttle* throttle : copy)
    throttle->Clear();
  throttles_.clear();
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

Response TargetHandler::FindSession(Maybe<std::string> session_id,
                                    Maybe<std::string> target_id,
                                    Session** session,
                                    bool fall_through) {
  *session = nullptr;
  fall_through &= access_mode_ != AccessMode::kBrowser;
  if (session_id.isJust()) {
    auto it = attached_sessions_.find(session_id.fromJust());
    if (it == attached_sessions_.end()) {
      if (fall_through)
        return Response::FallThrough();
      return Response::InvalidParams("No session with given id");
    }
    *session = it->second.get();
    return Response::OK();
  }
  if (target_id.isJust()) {
    for (auto& it : attached_sessions_) {
      if (it.second->IsAttachedTo(target_id.fromJust())) {
        if (*session)
          return Response::Error("Multiple sessions attached, specify id.");
        *session = it.second.get();
      }
    }
    if (!*session) {
      if (fall_through)
        return Response::FallThrough();
      return Response::InvalidParams("No session for given target id");
    }
    return Response::OK();
  }
  if (fall_through)
    return Response::FallThrough();
  return Response::InvalidParams("Session id must be specified");
}

// ----------------- Protocol ----------------------

Response TargetHandler::SetDiscoverTargets(bool discover) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  if (discover_ == discover)
    return Response::OK();
  discover_ = discover;
  if (discover_) {
    DevToolsAgentHost::AddObserver(this);
  } else {
    DevToolsAgentHost::RemoveObserver(this);
    reported_hosts_.clear();
  }
  return Response::OK();
}

Response TargetHandler::SetAutoAttach(bool auto_attach,
                                      bool wait_for_debugger_on_start,
                                      Maybe<bool> flatten) {
  if (flatten.fromMaybe(false) && !target_registry_) {
    return Response::InvalidParams(
        "Will only provide flatten access for browser endpoint");
  }
  flatten_auto_attach_ = flatten.fromMaybe(false);
  auto_attacher_.SetAutoAttach(auto_attach, wait_for_debugger_on_start);
  if (!auto_attacher_.ShouldThrottleFramesNavigation())
    ClearThrottles();
  return access_mode_ == AccessMode::kBrowser ? Response::OK()
                                              : Response::FallThrough();
}

Response TargetHandler::SetRemoteLocations(
    std::unique_ptr<protocol::Array<Target::RemoteLocation>>) {
  return Response::Error("Not supported");
}

Response TargetHandler::AttachToTarget(const std::string& target_id,
                                       Maybe<bool> flatten,
                                       std::string* out_session_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  if (flatten.fromMaybe(false) && !target_registry_) {
    return Response::InvalidParams(
        "Will only provide flatten access for browser endpoint");
  }
  *out_session_id =
      Session::Attach(this, agent_host.get(), false, flatten.fromMaybe(false));
  return Response::OK();
}

Response TargetHandler::AttachToBrowserTarget(std::string* out_session_id) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::Error(kNotAllowedError);
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::CreateForBrowser(
          nullptr, DevToolsAgentHost::CreateServerSocketCallback());
  *out_session_id = Session::Attach(this, agent_host.get(), false, true);
  return Response::OK();
}

Response TargetHandler::DetachFromTarget(Maybe<std::string> session_id,
                                         Maybe<std::string> target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  Session* session = nullptr;
  Response response =
      FindSession(std::move(session_id), std::move(target_id), &session, false);
  if (!response.isSuccess())
    return response;
  session->Detach(false);
  return Response::OK();
}

Response TargetHandler::SendMessageToTarget(const std::string& message,
                                            Maybe<std::string> session_id,
                                            Maybe<std::string> target_id) {
  Session* session = nullptr;
  Response response =
      FindSession(std::move(session_id), std::move(target_id), &session, true);
  if (!response.isSuccess())
    return response;
  if (session->flatten_protocol_) {
    return Response::Error(
        "When using flat protocol, messages are routed to the target "
        "via the sessionId attribute.");
  }
  session->SendMessageToAgentHost(message);
  return Response::OK();
}

Response TargetHandler::GetTargetInfo(
    Maybe<std::string> maybe_target_id,
    std::unique_ptr<Target::TargetInfo>* target_info) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  const std::string& target_id =
      maybe_target_id.isJust() ? maybe_target_id.fromJust() : owner_target_id_;
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *target_info = CreateInfo(agent_host.get());
  return Response::OK();
}

Response TargetHandler::ActivateTarget(const std::string& target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  // TODO(dgozman): only allow reported hosts.
  scoped_refptr<DevToolsAgentHost> agent_host(
      DevToolsAgentHost::GetForId(target_id));
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  agent_host->Activate();
  return Response::OK();
}

Response TargetHandler::CloseTarget(const std::string& target_id,
                                    bool* out_success) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  scoped_refptr<DevToolsAgentHost> agent_host =
      DevToolsAgentHost::GetForId(target_id);
  if (!agent_host)
    return Response::InvalidParams("No target with given id found");
  *out_success = agent_host->Close();
  return Response::OK();
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
    return Response::Error(base::StringPrintf(
        "Target with id %s is already granted remote debugging bindings.",
        target_id.c_str()));
  }
  if (!agent_host->GetWebContents()) {
    return Response::Error(
        "RemoteDebuggingBinding can be granted only to page targets");
  }

  new BrowserToPageConnector(binding_name.fromMaybe("cdp"), agent_host.get());
  return Response::OK();
}

Response TargetHandler::CreateTarget(const std::string& url,
                                     Maybe<int> width,
                                     Maybe<int> height,
                                     Maybe<std::string> context_id,
                                     Maybe<bool> enable_begin_frame_control,
                                     std::string* out_target_id) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Error("Not supported");
  scoped_refptr<content::DevToolsAgentHost> agent_host =
      delegate->CreateNewTarget(GURL(url));
  if (!agent_host)
    return Response::Error("Not supported");
  *out_target_id = agent_host->GetId();
  return Response::OK();
}

Response TargetHandler::GetTargets(
    std::unique_ptr<protocol::Array<Target::TargetInfo>>* target_infos) {
  if (access_mode_ == AccessMode::kAutoAttachOnly)
    return Response::Error(kNotAllowedError);
  *target_infos = protocol::Array<Target::TargetInfo>::create();
  for (const auto& host : DevToolsAgentHost::GetOrCreateAll())
    (*target_infos)->addItem(CreateInfo(host.get()));
  return Response::OK();
}

// -------------- DevToolsAgentHostObserver -----------------

bool TargetHandler::ShouldForceDevToolsAgentHostCreation() {
  return true;
}

void TargetHandler::DevToolsAgentHostCreated(DevToolsAgentHost* host) {
  // If we start discovering late, all existing agent hosts will be reported,
  // but we could have already attached to some.
  if (reported_hosts_.find(host) != reported_hosts_.end())
    return;
  frontend_->TargetCreated(CreateInfo(host));
  reported_hosts_.insert(host);
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

protocol::Response TargetHandler::CreateBrowserContext(
    std::string* out_context_id) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::Error(kNotAllowedError);
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Error("Browser context management is not supported.");
  BrowserContext* context = delegate->CreateBrowserContext();
  if (!context)
    return Response::Error("Failed to create browser context.");
  *out_context_id = context->UniqueId();
  return Response::OK();
}

protocol::Response TargetHandler::GetBrowserContexts(
    std::unique_ptr<protocol::Array<protocol::String>>* browser_context_ids) {
  if (access_mode_ != AccessMode::kBrowser)
    return Response::Error(kNotAllowedError);
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate)
    return Response::Error("Browser context management is not supported.");
  std::vector<content::BrowserContext*> contexts =
      delegate->GetBrowserContexts();
  *browser_context_ids = std::make_unique<protocol::Array<protocol::String>>();
  for (auto* context : contexts)
    (*browser_context_ids)->addItem(context->UniqueId());
  return Response::OK();
}

void TargetHandler::DisposeBrowserContext(
    const std::string& context_id,
    std::unique_ptr<DisposeBrowserContextCallback> callback) {
  if (access_mode_ != AccessMode::kBrowser) {
    callback->sendFailure(Response::Error(kNotAllowedError));
    return;
  }
  DevToolsManagerDelegate* delegate =
      DevToolsManager::GetInstance()->delegate();
  if (!delegate) {
    callback->sendFailure(
        Response::Error("Browser context management is not supported."));
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
        Response::Error("Failed to find context with id " + context_id));
    return;
  }
  delegate->DisposeBrowserContext(
      *context_it,
      base::BindOnce(
          [](std::unique_ptr<DisposeBrowserContextCallback> callback,
             bool success, const std::string& error) {
            if (success)
              callback->sendSuccess();
            else
              callback->sendFailure(Response::Error(error));
          },
          std::move(callback)));
}

}  // namespace protocol
}  // namespace content
