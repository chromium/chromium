// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/proto_wrappers/tab_organization_request_wrapper.h"

#import <memory>
#import <vector>

#import "base/barrier_closure.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/chrome/browser/intelligence/proto_wrappers/page_context_wrapper.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@implementation TabOrganizationRequestWrapper {
  raw_ptr<WebStateList> _webStateList;

  // Vector holding all PageContextWrappers to keep them alive until their async
  // work is done.
  std::vector<PageContextWrapper*> _page_contexts;

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

  // The callback to execute once all async work is complete, whichs
  // relinquishes ownership of the TabOrganizationRequest proto to the
  // callback's handler.
  base::OnceCallback<void(
      std::unique_ptr<optimization_guide::proto::TabOrganizationRequest>)>
      _completion_callback;

  // Unique pointer to the TabOrganizationRequest proto.
  std::unique_ptr<optimization_guide::proto::TabOrganizationRequest> _request;

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
}

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

- (instancetype)
               initWithWebStateList:(WebStateList*)webStateList
    allowReorganizingExistingGroups:(bool)allowReorganizingExistingGroups
                   groupingStrategy:
                       (optimization_guide::proto::
                            TabOrganizationRequest_TabOrganizationModelStrategy)
                           strategy
                 completionCallback:
                     (base::OnceCallback<
                         void(std::unique_ptr<optimization_guide::proto::
                                                  TabOrganizationRequest>)>)
                         completionCallback {
  self = [super init];
  if (self) {
    _webStateList = webStateList;
    _completion_callback = std::move(completionCallback);

    // Create the TabOrganizationRequest proto/object.
    _request =
        std::make_unique<optimization_guide::proto::TabOrganizationRequest>();

    // Set initial fields.
    _request->set_model_strategy(strategy);
    _request->set_allow_reorganizing_existing_groups(
        allowReorganizingExistingGroups);
    _request->set_active_tab_id(
        _webStateList->GetActiveWebState()->GetUniqueIdentifier().identifier());
  }
  return self;
}

- (void)populateRequestFieldsAsync {
  __weak TabOrganizationRequestWrapper* weakSelf = self;

  // Use a `BarrierClosure` to ensure all async tasks are completed before
  // executing the overall completion callback. The BarrierClosure will wait
  // until the `barrier` callback is itself run once for every WebState.
  base::RepeatingClosure barrier = base::BarrierClosure(
      _webStateList->count(), base::BindOnce(^{
        [weakSelf asyncWorkCompletedForTabOrganizationRequest];
      }));

  // Adds information from each open tab to the request, and for each of them
  // creates a PageContextWrapper to complete any async work necessary for the
  // given WebState.
  for (int index = 0; index < _webStateList->count(); ++index) {
    web::WebState* webState = _webStateList->GetWebStateAt(index);
    ::optimization_guide::proto::Tab* tab = _request->add_tabs();

    tab->set_tab_id(webState->GetUniqueIdentifier().identifier());
    tab->set_title(base::UTF16ToUTF8(webState->GetTitle()));
    tab->set_url(webState->GetVisibleURL().spec());

    PageContextWrapper* pageContextWrapper = [[PageContextWrapper alloc]
          initWithWebState:webState
        completionCallback:base::BindOnce(^(
                               std::unique_ptr<
                                   optimization_guide::proto::PageContext>
                                   page_context) {
          [weakSelf asyncWorkCompleteForPageContext:std::move(page_context)
                                      associatedTab:tab];
          barrier.Run();
        })];

    // Begin populating the PageContext proto fields. Once completed,
    // `completionCallback` will be executed.
    [pageContextWrapper populatePageContextFieldsAsync];

    // Hold references to each PageContextWrapper to keep them alive during
    // their async work.
    _page_contexts.push_back(pageContextWrapper);
  }
}

#pragma mark - Private

// All async tasks are complete, execute the overall completion callback.
// Relinquish ownership to the callback handler.
- (void)asyncWorkCompletedForTabOrganizationRequest {
  std::move(_completion_callback).Run(std::move(_request));
}

// All async work for a given PageContext is complete, set the PageContext for
// its associated tab.
- (void)asyncWorkCompleteForPageContext:
            (std::unique_ptr<optimization_guide::proto::PageContext>)
                page_context
                          associatedTab:
                              (optimization_guide::proto::Tab*)associated_tab {
  associated_tab->set_allocated_page_context(page_context.release());
}

#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end
