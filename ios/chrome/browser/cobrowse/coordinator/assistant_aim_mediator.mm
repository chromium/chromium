// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/coordinator/assistant_aim_mediator.h"

#import <algorithm>

#import "base/strings/string_util.h"
#import "base/strings/sys_string_conversions.h"
#import "base/uuid.h"
#import "components/contextual_tasks/public/contextual_task.h"
#import "components/contextual_tasks/public/contextual_tasks_service.h"
#import "components/contextual_tasks/public/features.h"
#import "components/search_engines/util.h"
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/model/aim_cobrowse_java_script_feature.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_item.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/lens_overlay/model/lens_overlay_url_utils.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/web_state_policy_decider_bridge.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "net/base/apple/url_conversions.h"
#import "third_party/lens_server_proto/aim_communication.pb.h"
#import "url/gurl.h"

namespace {

}  // namespace

@interface AssistantAIMMediator () <CRWWebStatePolicyDecider>
@end

@implementation AssistantAIMMediator {
  std::unique_ptr<web::WebState> _webState;
  std::unique_ptr<web::WebStatePolicyDeciderBridge> _policyDeciderBridge;
  __weak id<AssistantAIMConsumer> _consumer;
  CobrowseContext* _context;
  id<AssistantContainerCommands> _containerHandler;
  raw_ptr<contextual_tasks::ContextualTasksService> _contextualTasksService;
  raw_ptr<UrlLoadingBrowserAgent> _urlLoader;
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
    _webState = std::move(webState);
    _policyDeciderBridge = std::make_unique<web::WebStatePolicyDeciderBridge>(
        _webState.get(), self);
    _webState->SetUserAgentOverride(
        web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE) + " " +
        contextual_tasks::GetContextualTasksUserAgentSuffix());
    _context = context;
    _containerHandler = containerHandler;
    _contextualTasksService = contextualTasksService;
    _urlLoader = URLLoader;
  }
  return self;
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
  _webState.reset();
  _urlLoader = nullptr;
}

#pragma mark - CRWWebStatePolicyDecider

- (void)shouldAllowRequest:(NSURLRequest*)request
               requestInfo:(web::WebStatePolicyDecider::RequestInfo)requestInfo
           decisionHandler:(PolicyDecisionHandler)decisionHandler {
  GURL URL = net::GURLWithNSURL(request.URL);

  // 1. Allow Google redirection or main-frame navigation to Google/AIM domains.
  if (lens::IsGoogleRedirection(URL, requestInfo)) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  if (IsAimURL(URL) || IsAimZeroStateURL(URL)) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
    return;
  }

  // 2. Block renderer-initiated third-party main-frame navigations and open in
  // the main browser instead.
  if (requestInfo.target_frame_is_main) {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Cancel());
    // Filter out about:blank initialization navigations to prevent spawning
    // empty tabs in the main browser upon loading the Assistant AIM sheet.
    if (URL.is_valid() && !URL.IsAboutBlank()) {
      BOOL openInNewTab = requestInfo.target_window_is_cross_origin;
      UrlLoadParams params = openInNewTab ? UrlLoadParams::InNewTab(URL)
                                          : UrlLoadParams::InCurrentTab(URL);
      _urlLoader->Load(params);
    }
  } else {
    decisionHandler(web::WebStatePolicyDecider::PolicyDecision::Allow());
  }
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

  // Execute the script in the page via the JavaScriptFeature.
  AimCobrowseJavaScriptFeature::GetInstance()->PostMessage(_webState.get(),
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

@end
