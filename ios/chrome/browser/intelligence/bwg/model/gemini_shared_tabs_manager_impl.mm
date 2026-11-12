// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/gemini_shared_tabs_manager_impl.h"

#import <UIKit/UIKit.h>

#import <cstdint>

#import "base/apple/foundation_util.h"
#import "base/base64.h"
#import "base/check.h"
#import "base/functional/bind.h"
#import "base/functional/callback_helpers.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "components/optimization_guide/proto/features/common_quality_data.pb.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/intelligence/bwg/metrics/gemini_metrics.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_page_context.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_tab_helper.h"
#import "ios/chrome/browser/intelligence/bwg/model/gemini_utils.h"
#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_utils.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/snapshots/model/snapshot_browser_agent.h"
#import "ios/public/provider/chrome/browser/bwg/gemini_api.h"
#import "ios/web/public/web_state.h"

namespace {

// Returns `true` if `context` is eligible to be listed in the tab picker. A
// context is eligible if its computation state is either `kSuccess` or
// `kPending` (meaning it is not blocked or protected).
bool IsPageContextEligibleForTabPicker(GeminiPageContext* context) {
  ios::provider::GeminiPageContextComputationState computation_state =
      context.geminiPageContextComputationState;
  return computation_state ==
             ios::provider::GeminiPageContextComputationState::kSuccess ||
         computation_state ==
             ios::provider::GeminiPageContextComputationState::kPending;
}

// Encodes `snapshot` as a base64 PNG string on a background thread.
std::string EncodeSnapshotOnBackgroundThread(UIImage* snapshot) {
  @autoreleasepool {
    NSData* image_data = UIImagePNGRepresentation(snapshot);
    if (!image_data) {
      return std::string();
    }
    return base::Base64Encode(base::apple::NSDataToSpan(image_data));
  }
}

}  // namespace

GeminiSharedTabsManagerImpl::GeminiSharedTabsManagerImpl(
    WebStateList* web_state_list,
    PrefService* pref_service,
    PersistTabContextBrowserAgent* persist_tab_context_browser_agent,
    SnapshotBrowserAgent* snapshot_browser_agent,
    base::RepeatingCallback<bool()> is_gemini_invoked_callback,
    base::RepeatingCallback<bool()> is_tab_grid_visible_callback)
    : web_state_list_(web_state_list),
      persist_tab_context_browser_agent_(persist_tab_context_browser_agent),
      snapshot_browser_agent_(snapshot_browser_agent),
      is_gemini_invoked_callback_(std::move(is_gemini_invoked_callback)),
      is_tab_grid_visible_callback_(std::move(is_tab_grid_visible_callback)) {
  CHECK(web_state_list_);
  CHECK(pref_service);
  CHECK(is_gemini_invoked_callback_);
  CHECK(is_tab_grid_visible_callback_);

  web_state_list_observation_.Observe(web_state_list_);

  pref_change_registrar_.Init(pref_service);
  pref_change_registrar_.Add(
      prefs::kIOSBWGPageContentSetting,
      base::BindRepeating(
          &GeminiSharedTabsManagerImpl::OnPageContentPrefChanged,
          base::Unretained(this)));
}

GeminiSharedTabsManagerImpl::~GeminiSharedTabsManagerImpl() {
  Shutdown();
}

#pragma mark - Public

void GeminiSharedTabsManagerImpl::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  weak_factory_.InvalidateWeakPtrs();
  web_state_list_observation_.Reset();
  pref_change_registrar_.Reset();
  shared_tabs_.clear();
  is_gemini_invoked_callback_.Reset();
  is_tab_grid_visible_callback_.Reset();
  web_state_list_ = nullptr;
  persist_tab_context_browser_agent_ = nullptr;
  snapshot_browser_agent_ = nullptr;
}

#pragma mark - GeminiSharedTabsManager

NSArray<GeminiPageContext*>*
GeminiSharedTabsManagerImpl::GetInactiveSharedTabs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSMutableArray<GeminiPageContext*>* inactive_shared_tabs =
      [NSMutableArray array];
  web::WebStateID active_web_state_id = GetActiveWebStateID();

  for (const auto& [tab_id, context] : shared_tabs_) {
    if (tab_id != active_web_state_id) {
      [inactive_shared_tabs addObject:context];
    }
  }
  return inactive_shared_tabs;
}

void GeminiSharedTabsManagerImpl::SaveActivePageContextToSharedTabs(
    GeminiPageContext* active_page_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Guard against calls after `Shutdown()`.
  if (!web_state_list_) {
    return;
  }

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  // If the tab grid is visible, `active_page_context` is `kBlocked` and should
  // only be saved when no other tabs are attached. This prevents the current
  // tab from being erroneously shown as `kBlocked` when opening the Floaty on a
  // different attached tab.
  if (!active_web_state ||
      (is_tab_grid_visible_callback_.Run() && HasInactiveSharedTabs()) ||
      !IsGeminiMultiTabContextEnabled()) {
    return;
  }

  SetSharedPageContext(active_web_state->GetUniqueIdentifier(),
                       active_page_context);
}

void GeminiSharedTabsManagerImpl::UpdateSharedTabsForActiveWebState(
    web::WebState* active_web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shared_tabs_.empty() || !IsGeminiMultiTabContextEnabled()) {
    return;
  }

  if (!active_web_state) {
    shared_tabs_.clear();
    return;
  }

  web::WebStateID new_active_id = active_web_state->GetUniqueIdentifier();
  GeminiPageContext* active_context = GetSharedPageContext(new_active_id);
  if (!active_context ||
      active_context.geminiPageContextAttachmentState !=
          ios::provider::GeminiPageContextAttachmentState::kAttached) {
    shared_tabs_.clear();
  }
}

void GeminiSharedTabsManagerImpl::DetachTabWithID(NSString* tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int32_t identifier_value;
  if (!tab_id ||
      !base::StringToInt(base::SysNSStringToUTF8(tab_id), &identifier_value) ||
      !IsGeminiMultiTabContextEnabled()) {
    return;
  }

  web::WebStateID detached_tab_id =
      web::WebStateID::FromSerializedValue(identifier_value);

  web::WebStateID active_web_state_id = GetActiveWebStateID();
  CHECK(detached_tab_id != active_web_state_id);

  RemoveSharedPageContext(detached_tab_id);
  RecordGeminiTabDetached();

  GeminiPageContext* active_page_context =
      GetSharedPageContext(active_web_state_id);
  ios::provider::UpdateActivePageContext(active_page_context,
                                         GetInactiveSharedTabs());
}

void GeminiSharedTabsManagerImpl::UpdateLocalTabAttachmentState(
    NSString* tab_id,
    ios::provider::GeminiPageContextAttachmentState new_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  int32_t identifier_value;
  if (!tab_id ||
      !base::StringToInt(base::SysNSStringToUTF8(tab_id), &identifier_value)) {
    return;
  }

  web::WebStateID attached_tab_id =
      web::WebStateID::FromSerializedValue(identifier_value);

  if (GeminiPageContext* page_context = GetSharedPageContext(attached_tab_id)) {
    page_context.geminiPageContextAttachmentState = new_state;
    if (new_state ==
        ios::provider::GeminiPageContextAttachmentState::kAttached) {
      RecordGeminiActiveTabAttached();
    } else if (new_state ==
               ios::provider::GeminiPageContextAttachmentState::kDetached) {
      RecordGeminiActiveTabDetached();
    }
  }
}

NSUInteger GeminiSharedTabsManagerImpl::SharedTabsCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  NSUInteger count = 0;
  for (const auto& [tab_id, context] : shared_tabs_) {
    if (context.geminiPageContextAttachmentState ==
        ios::provider::GeminiPageContextAttachmentState::kAttached) {
      count++;
    }
  }
  return count;
}

void GeminiSharedTabsManagerImpl::OnTabPickerSelectionChanged(
    std::set<web::WebStateID> selected_tabs) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web::WebStateID active_web_state_id = GetActiveWebStateID();

  SharedTabsList new_attached_tabs;
  std::vector<web::WebStateID> tabs_to_fetch;

  // Preserve insertion order for already-shared tabs and update the active
  // tab's attachment state if it was eligible for the Tab Picker.
  for (const auto& [tab_id, context] : shared_tabs_) {
    if (tab_id == active_web_state_id) {
      if (IsPageContextEligibleForTabPicker(context)) {
        ios::provider::GeminiPageContextAttachmentState new_state =
            selected_tabs.contains(active_web_state_id)
                ? ios::provider::GeminiPageContextAttachmentState::kAttached
                : ios::provider::GeminiPageContextAttachmentState::kDetached;
        context.geminiPageContextAttachmentState = new_state;
      }
      new_attached_tabs.emplace_back(tab_id, context);
    } else if (selected_tabs.contains(tab_id)) {
      new_attached_tabs.emplace_back(tab_id, context);
    }
  }

  // Queue newly selected inactive tabs for page context fetch.
  for (web::WebStateID selected_tab : selected_tabs) {
    if (selected_tab != active_web_state_id &&
        !GetSharedPageContext(selected_tab)) {
      tabs_to_fetch.push_back(selected_tab);
    }
  }

  shared_tabs_ = std::move(new_attached_tabs);

  UpdateSharedTabContexts(tabs_to_fetch);

  GeminiPageContext* active_page_context =
      GetSharedPageContext(active_web_state_id);
  CHECK(active_page_context);
  ios::provider::UpdateActivePageContext(active_page_context,
                                         GetInactiveSharedTabs());
}

std::set<web::WebStateID>
GeminiSharedTabsManagerImpl::GetSelectedTabsForTabPicker() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::set<web::WebStateID> eligible_tabs;
  for (const auto& [tab_id, context] : shared_tabs_) {
    if (IsPageContextEligibleForTabPicker(context) &&
        context.geminiPageContextAttachmentState ==
            ios::provider::GeminiPageContextAttachmentState::kAttached) {
      eligible_tabs.insert(tab_id);
    }
  }
  return eligible_tabs;
}

#pragma mark - WebStateListObserver

void GeminiSharedTabsManagerImpl::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      OnWebStateRemoved(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      OnWebStateRemoved(replace_change.replaced_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert:
    case WebStateListChange::Type::kGroupCreate:
    case WebStateListChange::Type::kGroupVisualDataUpdate:
    case WebStateListChange::Type::kGroupMove:
    case WebStateListChange::Type::kGroupDelete:
      break;
  }

  if (status.active_web_state_change()) {
    OnActiveWebStateChanged(status.old_active_web_state);
  }
}

void GeminiSharedTabsManagerImpl::WebStateListDestroyed(
    WebStateList* web_state_list) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  web_state_list_observation_.Reset();
  web_state_list_ = nullptr;
}

#pragma mark - Private

void GeminiSharedTabsManagerImpl::OnWebStateRemoved(web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_state || shared_tabs_.empty() || !IsGeminiMultiTabContextEnabled()) {
    return;
  }
  RemoveSharedPageContext(web_state->GetUniqueIdentifier());
}

void GeminiSharedTabsManagerImpl::OnActiveWebStateChanged(
    web::WebState* old_active) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!old_active || shared_tabs_.empty() ||
      !IsGeminiMultiTabContextEnabled()) {
    return;
  }
  web::WebStateID old_active_id = old_active->GetUniqueIdentifier();
  GeminiPageContext* old_context = GetSharedPageContext(old_active_id);
  if (!old_context) {
    return;
  }
  if (old_context.geminiPageContextAttachmentState !=
      ios::provider::GeminiPageContextAttachmentState::kAttached) {
    RemoveSharedPageContext(old_active_id);
  } else if (HasInactiveSharedTabs()) {
    // We are switching tabs and there is more than one tab attached to the
    // conversation. Refetch the old active tab's page context to ensure it
    // reflects its most recent state (instead of the state when the Floaty
    // was last invoked).
    UpdateSharedTabContexts({old_active_id});
  }
}

void GeminiSharedTabsManagerImpl::OnPageContentPrefChanged() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pref_change_registrar_.prefs()->GetBoolean(
          prefs::kIOSBWGPageContentSetting)) {
    shared_tabs_.clear();
  }
}

bool GeminiSharedTabsManagerImpl::HasInactiveSharedTabs() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (shared_tabs_.size() > 1) {
    return true;
  }
  return !shared_tabs_.empty() &&
         shared_tabs_.begin()->first != GetActiveWebStateID();
}

web::WebStateID GeminiSharedTabsManagerImpl::GetActiveWebStateID() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `web_state_list_` may be null after `Shutdown()` or
  // `WebStateListDestroyed()`.
  if (!web_state_list_) {
    return web::WebStateID();
  }
  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  return active_web_state ? active_web_state->GetUniqueIdentifier()
                          : web::WebStateID();
}

void GeminiSharedTabsManagerImpl::UpdateSharedTabContexts(
    const std::vector<web::WebStateID>& tabs_to_fetch) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `web_state_list_` may be null after `Shutdown()`.
  if (!web_state_list_ || tabs_to_fetch.empty()) {
    return;
  }

  std::vector<std::string> tabs_to_fetch_str;

  for (web::WebStateID tab_to_fetch : tabs_to_fetch) {
    web::WebState* web_state = GetWebState(
        web_state_list_, WebStateSearchCriteria{.identifier = tab_to_fetch});
    if (!web_state) {
      continue;
    }

    GeminiPageContext* partial_context = CreatePartialPageContext(web_state);
    CHECK(partial_context);

    partial_context.geminiPageContextAttachmentState =
        ios::provider::GeminiPageContextAttachmentState::kAttached;
    SetSharedPageContext(tab_to_fetch, partial_context);

    tabs_to_fetch_str.push_back(
        base::NumberToString(tab_to_fetch.identifier()));
  }

  if (tabs_to_fetch_str.empty()) {
    return;
  }

  CHECK(persist_tab_context_browser_agent_);

  // Persisted tab contexts are read from disk on a background task runner to
  // avoid blocking the main thread on file I/O;
  // `OnPersistTabContextLookupComplete` is invoked asynchronously on the main
  // sequence once all lookups finish.
  persist_tab_context_browser_agent_->GetMultipleContextsAsync(
      tabs_to_fetch_str,
      base::BindOnce(
          &GeminiSharedTabsManagerImpl::OnPersistTabContextLookupComplete,
          weak_factory_.GetWeakPtr()));
}

GeminiPageContext* GeminiSharedTabsManagerImpl::CreatePartialPageContext(
    web::WebState* web_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
  if (tab_helper) {
    // If `web_state` is realized, get partial page context via `tab_helper`
    // to perform page context extraction eligibility checks.
    return tab_helper->GetPartialPageContext(/*is_background_tab=*/true);
  }

  // If `web_state` is unrealized, the tab has cached APC and we can bypass
  // page context extraction eligibility checks.
  return gemini::CreatePartialPageContextForWebState(web_state,
                                                     /*is_eligible=*/true);
}

void GeminiSharedTabsManagerImpl::OnPersistTabContextLookupComplete(
    PersistTabContextBrowserAgent::PageContextMap contexts_map) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [tab_id_str, proto_context] : contexts_map) {
    int32_t identifier;
    if (!base::StringToInt(tab_id_str, &identifier)) {
      continue;
    }
    web::WebStateID selected_tab =
        web::WebStateID::FromSerializedValue(identifier);

    if (proto_context.has_value()) {
      RetrieveCachedPageContextForTab(selected_tab,
                                      std::move(proto_context.value()));
    } else {
      GenerateFullPageContextForTab(selected_tab);
    }
  }
}

void GeminiSharedTabsManagerImpl::RetrieveCachedPageContextForTab(
    web::WebStateID selected_tab,
    std::unique_ptr<optimization_guide::proto::PageContext> proto_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The tab may have been un-shared or closed while the disk lookup was in
  // flight.
  if (!GetSharedPageContext(selected_tab)) {
    return;
  }

  // `snapshot_browser_agent_` may not be created in unit tests.
  if (!snapshot_browser_agent_) {
    CompleteCachedPageContextForTab(selected_tab, std::move(proto_context));
    return;
  }

  // Retrieve a color snapshot of the tab asynchronously (since uncached
  // snapshots must be decoded from disk off the main thread) and attach it to
  // the page context once available.
  // TODO(crbug.com/534752184): Move screenshot retrieval to
  // `PageContextWrapper`.
  snapshot_browser_agent_->RetrieveSnapshotWithID(
      SnapshotID(selected_tab), SnapshotKindColor,
      base::CallbackToBlock(base::BindOnce(
          &GeminiSharedTabsManagerImpl::OnSnapshotRetrievedForTab,
          weak_factory_.GetWeakPtr(), selected_tab, std::move(proto_context))));
}

void GeminiSharedTabsManagerImpl::OnSnapshotRetrievedForTab(
    web::WebStateID selected_tab,
    std::unique_ptr<optimization_guide::proto::PageContext> proto_context,
    UIImage* snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // The tab may have been un-shared or closed while snapshot retrieval was in
  // flight.
  if (!GetSharedPageContext(selected_tab)) {
    return;
  }

  if (!snapshot || !proto_context) {
    CompleteCachedPageContextForTab(selected_tab, std::move(proto_context));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      // Background task performs CPU-intensive PNG and Base64 image encoding
      // off the main thread to avoid UI jank.
      base::BindOnce(&EncodeSnapshotOnBackgroundThread, snapshot),
      // Reply task attaches the encoded screenshot to `proto_context` and
      // notifies the provider on the main sequence once encoding is complete.
      base::BindOnce(&GeminiSharedTabsManagerImpl::OnSnapshotEncodedForTab,
                     weak_factory_.GetWeakPtr(), selected_tab,
                     std::move(proto_context)));
}

void GeminiSharedTabsManagerImpl::OnSnapshotEncodedForTab(
    web::WebStateID selected_tab,
    std::unique_ptr<optimization_guide::proto::PageContext> proto_context,
    std::string encoded_snapshot) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (proto_context && !encoded_snapshot.empty()) {
    proto_context->set_tab_screenshot(std::move(encoded_snapshot));
  }
  CompleteCachedPageContextForTab(selected_tab, std::move(proto_context));
}

void GeminiSharedTabsManagerImpl::CompleteCachedPageContextForTab(
    web::WebStateID selected_tab,
    std::unique_ptr<optimization_guide::proto::PageContext> proto_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GeminiPageContext* partial_context = GetSharedPageContext(selected_tab);
  // The tab may have been un-shared or closed while background encoding was in
  // flight.
  if (!partial_context) {
    return;
  }

  GeminiPageContext* full_context = [[GeminiPageContext alloc] init];
  full_context.favicon = partial_context.favicon;
  full_context.geminiPageContextComputationState =
      ios::provider::GeminiPageContextComputationState::kSuccess;
  full_context.uniquePageContext = std::move(proto_context);
  OnFullPageContextAvailableForSharedTab(selected_tab, full_context);
}

void GeminiSharedTabsManagerImpl::GenerateFullPageContextForTab(
    web::WebStateID selected_tab) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // `web_state_list_` may be null after `Shutdown()`.
  if (!web_state_list_) {
    return;
  }
  web::WebState* web_state = GetWebState(
      web_state_list_, WebStateSearchCriteria{.identifier = selected_tab});
  if (!web_state) {
    return;
  }

  GeminiTabHelper* tab_helper = GeminiTabHelper::FromWebState(web_state);
  if (!tab_helper) {
    return;
  }

  // Page context extraction is asynchronous because it may need to wait for
  // the `WebState` to finish loading and executes JavaScript DOM extraction.
  tab_helper->GeneratePageContext(
      base::BindRepeating(
          &GeminiSharedTabsManagerImpl::OnFullPageContextAvailableForSharedTab,
          weak_factory_.GetWeakPtr(), selected_tab),
      /*is_background_tab=*/true);
}

void GeminiSharedTabsManagerImpl::OnFullPageContextAvailableForSharedTab(
    web::WebStateID web_state_id,
    GeminiPageContext* full_page_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GeminiPageContext* existing_context = GetSharedPageContext(web_state_id);
  // The tab was un-shared or closed before `full_page_context` became
  // available.
  if (!existing_context) {
    return;
  }

  full_page_context.geminiPageContextAttachmentState =
      existing_context.geminiPageContextAttachmentState;

  SetSharedPageContext(web_state_id, full_page_context);

  // The fetch is asynchronous and can complete after the floaty was dismissed,
  // e.g. when it was started by a tab switch. The provider is reset on
  // dismissal and only accepts page context while the floaty is invoked, so
  // only keep the refreshed context for the next invocation.
  if (!web_state_list_ || !is_gemini_invoked_callback_ ||
      !is_gemini_invoked_callback_.Run()) {
    return;
  }

  web::WebState* active_web_state = web_state_list_->GetActiveWebState();
  if (active_web_state) {
    GeminiPageContext* active_page_context =
        GetSharedPageContext(active_web_state->GetUniqueIdentifier());
    ios::provider::UpdateActivePageContext(active_page_context,
                                           GetInactiveSharedTabs());
  }
}

GeminiPageContext* GeminiSharedTabsManagerImpl::GetSharedPageContext(
    web::WebStateID tab_id) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& [id, context] : shared_tabs_) {
    if (id == tab_id) {
      return context;
    }
  }
  return nil;
}

void GeminiSharedTabsManagerImpl::SetSharedPageContext(
    web::WebStateID tab_id,
    GeminiPageContext* page_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& [id, existing_context] : shared_tabs_) {
    if (id == tab_id) {
      existing_context = page_context;
      return;
    }
  }
  shared_tabs_.emplace_back(tab_id, page_context);
}

void GeminiSharedTabsManagerImpl::RemoveSharedPageContext(
    web::WebStateID tab_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::erase_if(shared_tabs_,
                [tab_id](const auto& pair) { return pair.first == tab_id; });
}
