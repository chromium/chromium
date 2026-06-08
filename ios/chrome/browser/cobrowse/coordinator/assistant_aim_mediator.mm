// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import <algorithm>

#import "base/check.h"
#import "base/logging.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/timer/timer.h"
#import "base/uuid.h"
#import "components/contextual_tasks/public/contextual_task.h"
#import "components/contextual_tasks/public/contextual_tasks_service.h"
#import "components/contextual_tasks/public/features.h"
#import "components/google/core/common/google_util.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/debugger/aim_srp_message_logger.h"
#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"
#import "ios/chrome/browser/cobrowse/model/assistant_aim_tab_helper.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_item.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_url_utils.h"
#import "ios/chrome/browser/shared/public/commands/open_new_tab_command.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/public/features/system_flags.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager_observer_bridge.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "ios/web/public/web_state_delegate_bridge.h"
#import "net/base/apple/url_conversions.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"
#import "url/gurl.h"

@interface AssistantAIMMediator () <CRWWebStatePolicyDecider,
                                    CRWWebFramesManagerObserver,
                                    CRWWebStateDelegate>
@end

@implementation AssistantAIMMediator {
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;
  std::unique_ptr<web::WebStateDelegateBridge> _webStateDelegateBridge;
  __weak id<AssistantAIMConsumer> _consumer;
  CobrowseContext* _context;
  id<AssistantContainerCommands> _containerHandler;
  raw_ptr<contextual_tasks::ContextualTasksService> _contextualTasksService;
  raw_ptr<UrlLoadingBrowserAgent> _urlLoader;
  // Bridge to observe WebFramesManager and detect when the main frame becomes
  // available.
  std::unique_ptr<web::WebFramesManagerObserverBridge>
      _webFramesManagerObserverBridge;
  // Periodic timer used to repeat sending the handshake ping until a response
  // is received.
  base::RepeatingTimer _handshakeTimer;
  // The capabilities of the AIM page, if the handshake has completed.
  std::optional<std::vector<lens::FeatureCapability>> _capabilities;
  // Logger for AIM SRP messages.
  AimSRPMessageLogger* _logger;
}

@synthesize consumer = _consumer;

- (instancetype)initWithWebState:(std::unique_ptr<web::WebState>)webState
                         context:(CobrowseContext*)context
                containerHandler:
                    (id<AssistantContainerCommands>)containerHandler
          contextualTasksService:
              (contextual_tasks::ContextualTasksService*)contextualTasksService
                       URLLoader:(UrlLoadingBrowserAgent*)URLLoader {
  self = [super init];
  if (self) {
    DCHECK(webState);
    DCHECK(!webState->GetDelegate());
    _webState = std::move(webState);
    _policyDeciderBridge = std::make_unique<web::WebStatePolicyDeciderBridge>(
        _webState.get(), self);
    _webState->SetUserAgentOverride(
        web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE) + " " +
        contextual_tasks::GetContextualTasksUserAgentSuffix());
    _webStateDelegateBridge =
        std::make_unique<web::WebStateDelegateBridge>(self);
    _webState->SetDelegate(_webStateDelegateBridge.get());
    _context = context;
    _containerHandler = containerHandler;
    _contextualTasksService = contextualTasksService;
    _urlLoader = URLLoader;

    _webFramesManagerObserverBridge =
        std::make_unique<web::WebFramesManagerObserverBridge>(self);
    AimCobrowseJavaScriptFeature::GetInstance()
        ->GetWebFramesManager(_webState.get())
        ->AddObserver(_webFramesManagerObserverBridge.get());

    __weak __typeof(self) weakSelf = self;
    AssistantAimTabHelper::FromWebState(_webState.get())
        ->SetMessageCallback(
            base::BindRepeating(^(const lens::AimToClientMessage& message) {
              [weakSelf handleWebMessage:message];
            }));

    if (experimental_flags::IsOmniboxDebuggingEnabled()) {
      _logger = [[AimSRPMessageLogger alloc] init];
    }
  }
  return self;
}

- (NSArray<AimSRPDebuggerEvent*>*)debugEvents {
  return _logger.events;
}

- (void)setConsumer:(id<AssistantAIMConsumer>)consumer {
  if (_consumer == consumer) {
    return;
  }
  _consumer = consumer;
  if (_consumer && _webState) {
    [_consumer setWebStateView:_webState->GetView()];
    [self loadAIMURL];
  }
}

- (void)disconnect {
  _policyDeciderBridge.reset();
  _handshakeTimer.Stop();
  if (_webFramesManagerObserverBridge) {
    AimCobrowseJavaScriptFeature::GetInstance()
        ->GetWebFramesManager(_webState.get())
        ->RemoveObserver(_webFramesManagerObserverBridge.get());
    _webFramesManagerObserverBridge.reset();
  }
  _webState.reset();
  _urlLoader = nullptr;
  _capabilities = std::nullopt;
  _logger = nil;
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  GURL URL = net::GURLWithNSURL(request.URL);

  // 1. Allow main-frame navigation to Google/AIM domains.
  if (IsAimURL(URL) || IsAimZeroStateURL(URL)) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // 2. Allow Google redirection to authorized search or home page paths.
  if (lens::IsGoogleRedirection(URL, requestInfo)) {
    if (google_util::IsGoogleSearchUrl(URL) ||
        google_util::IsGoogleHomePageUrl(URL)) {
      decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
      return;
    }
  }

  // 3. Block renderer-initiated third-party main-frame navigations and open in
  // the main browser instead.
  if (requestInfo.target_frame_is_main) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    // Filter out about:blank initialization navigations to prevent spawning
    // empty tabs in the main browser upon loading the Assistant AIM sheet.
    if (URL.is_valid() && !URL.IsAboutBlank()) {
      UrlLoadParams params = UrlLoadParams::InNewTab(URL);
      _urlLoader->Load(params);
      [_containerHandler
          animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized
                                   duration:kSheetDetentAnimationDuration
                                      curve:UIViewAnimationCurveEaseInOut];
    }
  } else {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
  }
}

#pragma mark - CRWWebStateDelegate

- (web::WebState*)webState:(web::WebState*)webState
    createNewWebStateForURL:(const GURL&)URL
                  openerURL:(const GURL&)openerURL
            initiatedByUser:(BOOL)initiatedByUser {
  // Cobrowse is not supported in incognito, so new tabs are always opened in
  // regular mode.
  OpenNewTabCommand* command = [OpenNewTabCommand commandWithURLFromChrome:URL
                                                               inIncognito:NO];
  command.openerWebState = webState->GetWeakPtr();

  [self.sceneHandler openURLInNewTab:command];

  [_containerHandler
      animateAssistantContainerToDetent:AssistantContainerDetent::kMinimized
                               duration:kSheetDetentAnimationDuration
                                  curve:UIViewAnimationCurveEaseInOut];

  return nullptr;
}

#pragma mark - Private helpers

// Loads the URL defined in the cobrowse context.
- (void)loadAIMURL {
  AssistantContainerDetent detent;
  if (IsAssistantAimMinimizedStateEnabled()) {
    detent = AssistantContainerDetent::kMinimized;
  } else {
    detent = AssistantContainerDetent::kMedium;
  }
  [_containerHandler
      animateAssistantContainerToDetent:detent
                               duration:kSheetDetentAnimationDuration
                                  curve:UIViewAnimationCurveEaseInOut];
  web::NavigationManager::WebLoadParams params(_context.url);
  _webState->GetNavigationManager()->LoadURLWithParams(params);
}

// Fetches history items.
- (void)fetchHistoryItemsWithCompletion:
    (void (^)(const std::vector<AssistantAIMHistoryItem>&))completion {
  if (!_contextualTasksService) {
    if (completion) {
      completion({});
    }
    return;
  }

  _contextualTasksService->GetTasks(base::BindOnce(^(
      std::vector<contextual_tasks::ContextualTask> tasks) {
    std::sort(tasks.begin(), tasks.end(), [](const auto& a, const auto& b) {
      auto thread_a = a.GetThread();
      auto thread_b = b.GetThread();
      base::Time time_a = thread_a ? thread_a->last_turn_time : base::Time();
      base::Time time_b = thread_b ? thread_b->last_turn_time : base::Time();
      return time_a > time_b;
    });
    std::vector<AssistantAIMHistoryItem> items;
    for (const auto& task : tasks) {
      AssistantAIMHistoryItem item;
      item.task_id = task.GetTaskId().AsLowercaseString();
      item.title = task.GetTitle();
      items.push_back(item);
    }
    if (completion) {
      completion(items);
    }
  }));
}

// Loads a history thread with the given task ID.
- (void)loadHistoryThreadWithTaskId:(NSString*)taskId {
  if (!_contextualTasksService) {
    return;
  }

  base::Uuid uuid = base::Uuid::ParseLowercase(base::SysNSStringToUTF8(taskId));
  if (!uuid.is_valid()) {
    return;
  }
  NSString* localeIdentifier = [NSLocale currentLocale].localeIdentifier;
  std::string locale = base::SysNSStringToUTF8(localeIdentifier);
  base::ReplaceChars(locale, "_", "-", &locale);

  base::WeakPtr<web::WebState> weakWebState = _webState->GetWeakPtr();
  _contextualTasksService->GetThreadUrlFromTaskId(
      uuid, locale,
      omnibox::ChromeAimEntryPoint::IOS_CHROME_OMNIBOX_SEARCH_ENTRY_POINT,
      base::BindOnce(^(GURL url) {
        if (url.is_valid() && weakWebState) {
          web::NavigationManager::WebLoadParams params(url);
          weakWebState->GetNavigationManager()->LoadURLWithParams(params);
        }
      }));
}

#pragma mark - ComposeboxURLLoader

// Sends the query message to the page via the JavaScriptFeature.
- (void)prepareLoadWithClientToAimMessage:
    (const lens::ClientToAimMessage&)message {
  if (!_webState || !message.has_submit_query()) {
    return;
  }

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [_logger logClientToAimMessage:message];
  }

  // Execute the script in the page via the JavaScriptFeature.
  AimCobrowseJavaScriptFeature::GetInstance()->SendNativeToWeb(_webState.get(),
                                                               message);

  [self.delegate assistantAIMMediatorDidLoadQuery:self];
}

- (void)loadURLParams:(const UrlLoadParams&)URLLoadParams {
  // NO-OP
}

#pragma mark - AssistantAIMMutator

- (void)didTapHistory {
  __weak AssistantAIMMediator* weakSelf = self;
  [self fetchHistoryItemsWithCompletion:^(
            const std::vector<AssistantAIMHistoryItem>& items) {
    [weakSelf.consumer displayHistoryWithItems:items];
  }];
}

- (void)didSelectHistoryTaskWithId:(NSString*)taskId {
  [self loadHistoryThreadWithTaskId:taskId];
}

#pragma mark - CRWWebFramesManagerObserver

- (void)webFramesManager:(web::WebFramesManager*)webFramesManager
    frameBecameAvailable:(web::WebFrame*)webFrame {
  if (webFrame->IsMainFrame()) {
    // Stop the handshake timer if the user navigated away from an AIM page.
    const GURL URL = _webState->GetLastCommittedURL();
    if (!IsAimURL(URL) && !IsAimZeroStateURL(URL)) {
      _handshakeTimer.Stop();
      _capabilities = std::nullopt;
      return;
    }
    // Initiate the web-to-native handshake if it has not been established yet.
    AssistantAimTabHelper* tabHelper =
        AssistantAimTabHelper::FromWebState(_webState.get());
    if (tabHelper && !tabHelper->IsHandshakeReceived()) {
      _capabilities = std::nullopt;
      if (!_handshakeTimer.IsRunning()) {
        __weak __typeof(self) weakSelf = self;
        _handshakeTimer.Start(FROM_HERE, base::Seconds(1),
                              base::BindRepeating(^{
                                [weakSelf sendHandshakePing];
                              }));
        [self sendHandshakePing];  // Send immediately
      }
    }
  }
}

#pragma mark - Private

- (void)sendHandshakePing {
  if (!_webState) {
    _handshakeTimer.Stop();
    return;
  }

  // Do not send a handshake ping if the user is no longer on an AIM page.
  const GURL URL = _webState->GetLastCommittedURL();
  if (!IsAimURL(URL) && !IsAimZeroStateURL(URL)) {
    _handshakeTimer.Stop();
    return;
  }

  lens::ClientToAimMessage handshake_ping;
  handshake_ping.mutable_handshake_ping()->add_capabilities(
      lens::FeatureCapability::DEFAULT);

  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [_logger logClientToAimMessage:handshake_ping];
  }

  AimCobrowseJavaScriptFeature::GetInstance()->SendNativeToWeb(_webState.get(),
                                                               handshake_ping);
}

- (void)handleWebMessage:(const lens::AimToClientMessage&)message {
  if (experimental_flags::IsOmniboxDebuggingEnabled()) {
    [_logger logAimToClientMessage:message];
  }

  if (message.has_handshake_response()) {
    _handshakeTimer.Stop();
    // Store the server capabilities.
    std::vector<lens::FeatureCapability> feature_capabilities;
    for (int capability : message.handshake_response().capabilities()) {
      feature_capabilities.push_back(
          static_cast<lens::FeatureCapability>(capability));
    }
    _capabilities = std::move(feature_capabilities);

    if (VLOG_IS_ON(1)) {
      std::vector<std::string> capability_names;
      for (lens::FeatureCapability capability : *_capabilities) {
        capability_names.push_back(
            std::string(lens::FeatureCapability_Name(capability)));
      }
      VLOG(1) << "AimCobrowse: Received HandshakeResponse with capabilities: ["
              << base::JoinString(capability_names, ", ") << "]";
    }
  } else if (message.has_hide_input()) {
    VLOG(1) << "AimCobrowse: Received HideInput";
  } else if (message.has_restore_input()) {
    VLOG(1) << "AimCobrowse: Received RestoreInput";
  } else if (message.has_enter_basic_mode()) {
    VLOG(1) << "AimCobrowse: Received EnterBasicMode";
  } else if (message.has_exit_basic_mode()) {
    VLOG(1) << "AimCobrowse: Received ExitBasicMode";
  } else if (message.has_update_thread_context_library()) {
    VLOG(1) << "AimCobrowse: Received UpdateThreadContextLibrary";
  } else if (message.has_notify_zero_state_rendered()) {
    VLOG(1) << "AimCobrowse: Received NotifyZeroStateRendered";
  } else if (message.has_set_chrome_desktop_input_plate_configuration()) {
    VLOG(1) << "AimCobrowse: Received SetChromeDesktopInputPlateConfiguration";
  } else if (message.has_inject_input()) {
    VLOG(1) << "AimCobrowse: Received InjectInput";
  } else if (message.has_remove_injected_input()) {
    VLOG(1) << "AimCobrowse: Received RemoveInjectedInput";
  } else if (message.has_unlock_input()) {
    VLOG(1) << "AimCobrowse: Received UnlockInput";
  } else if (message.has_lock_input()) {
    VLOG(1) << "AimCobrowse: Received LockInput";
  }
}

- (BOOL)supportsCapability:(lens::FeatureCapability)capability {
  if (!_capabilities.has_value()) {
    return NO;
  }
  return std::find(_capabilities->begin(), _capabilities->end(), capability) !=
         _capabilities->end();
}

- (const std::optional<std::vector<lens::FeatureCapability>>&)capabilities {
  return _capabilities;
}

@end
