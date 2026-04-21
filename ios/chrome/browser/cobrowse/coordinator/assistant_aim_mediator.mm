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
#import "ios/chrome/browser/assistant/coordinator/assistant_container_commands.h"
#import "ios/chrome/browser/assistant/ui/assistant_container_detent.h"
#import "ios/chrome/browser/cobrowse/model/cobrowse_context.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_consumer.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_history_item.h"
#import "ios/chrome/browser/cobrowse/ui/assistant_aim_ui_constants.h"
#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"
#import "net/base/url_util.h"
#import "url/gurl.h"

namespace {

}  // namespace

@implementation AssistantAIMMediator {
  std::unique_ptr<web::WebState> _webState;
  __weak id<AssistantAIMConsumer> _consumer;
  CobrowseContext* _context;
  id<AssistantContainerCommands> _containerHandler;
  raw_ptr<contextual_tasks::ContextualTasksService> _contextualTasksService;
}

@synthesize consumer = _consumer;

- (instancetype)
          initWithWebState:(std::unique_ptr<web::WebState>)webState
                   context:(CobrowseContext*)context
          containerHandler:(id<AssistantContainerCommands>)containerHandler
    contextualTasksService:
        (contextual_tasks::ContextualTasksService*)contextualTasksService {
  self = [super init];
  if (self) {
    _webState = std::move(webState);
    _webState->SetUserAgentOverride(
        web::GetWebClient()->GetUserAgent(web::UserAgentType::MOBILE) + " " +
        contextual_tasks::GetContextualTasksUserAgentSuffix());
    _context = context;
    _containerHandler = containerHandler;
    _contextualTasksService = contextualTasksService;
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
  _webState.reset();
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

// Prepares the load for a given query text by appending it to the base URL.
- (void)prepareLoadForQueryText:(NSString*)queryText
             clientToAimMessage:(const lens::ClientToAimMessage&)message {
  if (!_webState || queryText.length == 0) {
    return;
  }

  GURL url = net::AppendOrReplaceQueryParameter(
      _context.url, "q", base::SysNSStringToUTF8(queryText));
  web::NavigationManager::WebLoadParams params{url};
  _webState->GetNavigationManager()->LoadURLWithParams(params);

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
