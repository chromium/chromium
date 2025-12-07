// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/ios_smart_tab_grouping_request_wrapper.h"

#import "base/barrier_closure.h"
#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/optimization_guide/proto/features/tab_organization.pb.h"
#import "ios/chrome/browser/intelligence/persist_tab_context/model/persist_tab_context_browser_agent.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

@implementation IosSmartTabGroupingRequestWrapper {
  // Browser agent responsible for persisting and retrieving tab context data.
  raw_ptr<PersistTabContextBrowserAgent> _persistTabContextBrowserAgent;

  // The list of web states in the current browser window.
  raw_ptr<WebStateList> _webStateList;

  // Vector holding all PageContextWrappers to keep them alive until their async
  // work is done. Used by `populateRequestFieldsAsyncFromWebStates`.
  std::vector<PageContextWrapper*> _pageContexts;

  // The callback to execute once all async work is complete, which
  // relinquishes ownership of the IosSmartTabGroupingRequest proto to the
  // callback's handler.
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>)>
      _completionCallback;

  // Unique pointer to the IosSmartTabGroupingRequest proto.
  std::unique_ptr<optimization_guide::proto::IosSmartTabGroupingRequest>
      _request;
}

- (instancetype)initWithWebStateList:(WebStateList*)webStateList
       persistTabContextBrowserAgent:
           (PersistTabContextBrowserAgent*)persistTabContextBrowserAgent
                  completionCallback:
                      (base::OnceCallback<void(
                           std::unique_ptr<optimization_guide::proto::
                                               IosSmartTabGroupingRequest>)>)
                          completionCallback {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _persistTabContextBrowserAgent = persistTabContextBrowserAgent;
    _completionCallback = std::move(completionCallback);

    _request = std::make_unique<
        optimization_guide::proto::IosSmartTabGroupingRequest>();
  }
  return self;
}

- (void)populateRequestFieldsAsyncFromPersistence {
  if (!_persistTabContextBrowserAgent) {
    LOG(ERROR) << "PersistTabContextBrowserAgent not found.";
    if (_completionCallback) {
      std::move(_completionCallback).Run(nullptr);
    }
    return;
  }

  if (_webStateList->count() == 0) {
    if (_completionCallback) {
      std::move(_completionCallback).Run(std::move(_request));
    }
    return;
  }

  _request->clear_tabs();

  std::vector<std::string> webStateIDs;
  webStateIDs.reserve(_webStateList->count());

  for (int index = 0; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    if (!webState) {
      continue;
    }

    std::string uniqueID =
        base::NumberToString(webState->GetUniqueIdentifier().identifier());
    webStateIDs.push_back(uniqueID);

    optimization_guide::proto::Tab* tab = _request->add_tabs();
    tab->set_tab_id(webState->GetUniqueIdentifier().identifier());
    tab->set_title(base::UTF16ToUTF8(webState->GetTitle()));
    tab->set_url(webState->GetVisibleURL().spec());
  }

  __weak __typeof(self) weakSelf = self;
  _persistTabContextBrowserAgent->GetMultipleContextsAsync(
      webStateIDs,
      base::BindOnce(
          ^(PersistTabContextBrowserAgent::PageContextMap pageContextMap) {
            if (weakSelf) {
              [weakSelf onPageContextsRetrieved:std::move(pageContextMap)];
            }
          }));
}

- (void)populateRequestFieldsAsyncFromWebStates {
  int webStateCount = _webStateList->count();
  if (webStateCount == 0) {
    if (_completionCallback) {
      std::move(_completionCallback).Run(std::move(_request));
    }
    return;
  }

  _request->clear_tabs();
  _pageContexts.clear();

  __weak IosSmartTabGroupingRequestWrapper* weakSelf = self;

  // The number of times the barrier closure needs to be called before the final
  // completion block is executed. This is the number of web states plus one
  // to account for the main loop finishing.
  int requiredCallbackCount = webStateCount + 1;

  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback. The BarrierClosure will wait
  // until the `barrier` callback is itself run `requiredCallbackCount` times.
  base::RepeatingClosure barrier =
      base::BarrierClosure(requiredCallbackCount, base::BindOnce(^{
                             [weakSelf completedAsyncRequest];
                           }));

  for (int index = 0; index < webStateCount; ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    if (!webState) {
      barrier.Run();
      continue;
    }

    optimization_guide::proto::Tab* tab = _request->add_tabs();

    tab->set_tab_id(webState->GetUniqueIdentifier().identifier());
    tab->set_title(base::UTF16ToUTF8(webState->GetTitle()));
    tab->set_url(webState->GetVisibleURL().spec());

    if (!webState->IsRealized()) {
      barrier.Run();
      continue;
    }

    void (^pageContextCompletion_block)(PageContextWrapperCallbackResponse) = ^(
        PageContextWrapperCallbackResponse response) {
      IosSmartTabGroupingRequestWrapper* strongSelf = weakSelf;
      if (!strongSelf) {
        barrier.Run();
        return;
      }
      if (response.has_value()) {
        [strongSelf asyncWorkCompleteForPageContext:std::move(response.value())
                                      associatedTab:tab];
      }
      barrier.Run();
    };

    base::OnceCallback<void(PageContextWrapperCallbackResponse)>
        completionCallback = base::BindOnce(pageContextCompletion_block);

    PageContextWrapper* pageContextWrapper = [[PageContextWrapper alloc]
          initWithWebState:webState
        completionCallback:std::move(completionCallback)];

    [pageContextWrapper setShouldGetSnapshot:NO];
    [pageContextWrapper setShouldForceUpdateMissingSnapshots:NO];
    [pageContextWrapper setShouldGetAnnotatedPageContent:NO];
    [pageContextWrapper setShouldGetInnerText:YES];

    _pageContexts.push_back(pageContextWrapper);
    [pageContextWrapper populatePageContextFieldsAsync];
  }

  barrier.Run();
}

#pragma mark - Private

// Called when all asynchronous operations to populate the request proto are
// complete. This method invokes the completion callback with the populated
// request.
- (void)completedAsyncRequest {
  if (_completionCallback) {
    std::move(_completionCallback).Run(std::move(_request));
  }
  _pageContexts.clear();
}

// Called when a single PageContextWrapper has finished its asynchronous
// work. Attaches the retrieved PageContext to its associated Tab proto.
- (void)asyncWorkCompleteForPageContext:
            (std::unique_ptr<optimization_guide::proto::PageContext>)pageContext
                          associatedTab:
                              (optimization_guide::proto::Tab*)associatedTab {
  if (associatedTab && pageContext) {
    associatedTab->set_allocated_page_context(pageContext.release());
  }
}

// Callback function executed when page contexts are retrieved from the
// PersistTabContextBrowserAgent. It matches the retrieved contexts to the
// corresponding tabs in the request proto.
- (void)onPageContextsRetrieved:
    (PersistTabContextBrowserAgent::PageContextMap)pageContextMap {
  for (int i = 0; i < _request->tabs_size(); ++i) {
    optimization_guide::proto::Tab* tab = _request->mutable_tabs(i);
    std::string uniqueID = base::NumberToString(tab->tab_id());

    auto it = pageContextMap.find(uniqueID);
    if (it != pageContextMap.end()) {
      std::optional<std::unique_ptr<optimization_guide::proto::PageContext>>&
          optionalPageContext = it->second;
      if (optionalPageContext.has_value()) {
        tab->set_allocated_page_context(optionalPageContext->release());
      }
    }
  }
  [self completedAsyncRequest];
}

@end
