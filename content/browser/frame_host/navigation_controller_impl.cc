// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 *     (http://www.torchmobile.com/)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/frame_host/navigation_controller_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"  // Temporary
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "cc/base/switches.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/browser_url_handler_impl.h"
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"
#include "content/browser/dom_storage/session_storage_namespace_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/frame_host/navigation_entry_impl.h"
#include "content/browser/frame_host/navigation_entry_screenshot_manager.h"
#include "content/browser/frame_host/navigation_handle_impl.h"
#include "content/browser/frame_host/navigator.h"
#include "content/browser/renderer_host/render_view_host_impl.h"  // Temporary
#include "content/browser/site_instance_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_utils.h"
#include "media/base/mime_util.h"
#include "net/base/escape.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/blink/public/common/blob/blob_utils.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "url/url_constants.h"

namespace content {
namespace {

// Invoked when entries have been pruned, or removed. For example, if the
// current entries are [google, digg, yahoo], with the current entry google,
// and the user types in cnet, then digg and yahoo are pruned.
void NotifyPrunedEntries(NavigationControllerImpl* nav_controller,
                         bool from_front,
                         int count) {
  PrunedDetails details;
  details.from_front = from_front;
  details.count = count;
  nav_controller->delegate()->NotifyNavigationListPruned(details);
}

// Ensure the given NavigationEntry has a valid state, so that WebKit does not
// get confused if we navigate back to it.
//
// An empty state is treated as a new navigation by WebKit, which would mean
// losing the navigation entries and generating a new navigation entry after
// this one. We don't want that. To avoid this we create a valid state which
// WebKit will not treat as a new navigation.
void SetPageStateIfEmpty(NavigationEntryImpl* entry) {
  if (!entry->GetPageState().IsValid())
    entry->SetPageState(PageState::CreateFromURL(entry->GetURL()));
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<std::unique_ptr<NavigationEntryImpl>>* entries,
    RestoreType type) {
  for (size_t i = 0; i < entries->size(); ++i) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    (*entries)[i]->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    (*entries)[i]->set_restore_type(type);
    // NOTE(darin): This code is only needed for backwards compat.
    SetPageStateIfEmpty((*entries)[i].get());
  }
}

// Determines whether or not we should be carrying over a user agent override
// between two NavigationEntries.
bool ShouldKeepOverride(const NavigationEntry* last_entry) {
  return last_entry && last_entry->GetIsOverridingUserAgent();
}

// Determines whether to override user agent for a navigation.
bool ShouldOverrideUserAgent(
    NavigationController::UserAgentOverrideOption override_user_agent,
    const NavigationEntry* last_committed_entry) {
  switch (override_user_agent) {
    case NavigationController::UA_OVERRIDE_INHERIT:
      return ShouldKeepOverride(last_committed_entry);
    case NavigationController::UA_OVERRIDE_TRUE:
      return true;
    case NavigationController::UA_OVERRIDE_FALSE:
      return false;
    default:
      break;
  }
  NOTREACHED();
  return false;
}

// Returns true this navigation should be treated as a reload. For e.g.
// navigating to the last committed url via the address bar or clicking on a
// link which results in a navigation to the last committed or pending
// navigation, etc.
// |url|, |virtual_url|, |base_url_for_data_url|, |transition_type| correspond
// to the new navigation (i.e. the pending NavigationEntry).
// |last_committed_entry| is the last navigation that committed.
bool ShouldTreatNavigationAsReload(
    const GURL& url,
    const GURL& virtual_url,
    const GURL& base_url_for_data_url,
    ui::PageTransition transition_type,
    bool is_main_frame,
    bool is_post,
    bool is_reload,
    bool is_navigation_to_existing_entry,
    bool has_interstitial,
    const NavigationEntryImpl* last_committed_entry) {
  // Don't convert when an interstitial is showing.
  if (has_interstitial)
    return false;

  // Only convert main frame navigations to a new entry.
  if (!is_main_frame || is_reload || is_navigation_to_existing_entry)
    return false;

  // Only convert to reload if at least one navigation committed.
  if (!last_committed_entry)
    return false;

  // Skip navigations initiated by external applications.
  if (transition_type & ui::PAGE_TRANSITION_FROM_API)
    return false;

  // We treat (PAGE_TRANSITION_RELOAD | PAGE_TRANSITION_FROM_ADDRESS_BAR),
  // PAGE_TRANSITION_TYPED or PAGE_TRANSITION_LINK transitions as navigations
  // which should be treated as reloads.
  bool transition_type_can_be_converted = false;
  if (ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_RELOAD) &&
      (transition_type & ui::PAGE_TRANSITION_FROM_ADDRESS_BAR)) {
    transition_type_can_be_converted = true;
  }
  if (ui::PageTransitionCoreTypeIs(transition_type,
                                   ui::PAGE_TRANSITION_TYPED)) {
    transition_type_can_be_converted = true;
  }
  if (ui::PageTransitionCoreTypeIs(transition_type, ui::PAGE_TRANSITION_LINK))
    transition_type_can_be_converted = true;
  if (!transition_type_can_be_converted)
    return false;

  // This check is required for cases like view-source:, etc. Here the URL of
  // the navigation entry would contain the url of the page, while the virtual
  // URL contains the full URL including the view-source prefix.
  if (virtual_url != last_committed_entry->GetVirtualURL())
    return false;

  // Check that the URL match.
  if (url != last_committed_entry->GetURL())
    return false;

  // This check is required for Android WebView loadDataWithBaseURL. Apps
  // can pass in anything in the base URL and we need to ensure that these
  // match before classifying it as a reload.
  if (url.SchemeIs(url::kDataScheme) && base_url_for_data_url.is_valid()) {
    if (base_url_for_data_url != last_committed_entry->GetBaseURLForDataURL())
      return false;
  }

  // Skip entries with SSL errors.
  if (last_committed_entry->ssl_error())
    return false;

  // Don't convert to a reload when the last navigation was a POST or the new
  // navigation is a POST.
  if (last_committed_entry->GetHasPostData() || is_post)
    return false;

  return true;
}

bool IsValidURLForNavigation(bool is_main_frame,
                             const GURL& virtual_url,
                             const GURL& dest_url) {
  // Don't attempt to navigate if the virtual URL is non-empty and invalid.
  if (is_main_frame && !virtual_url.is_valid() && !virtual_url.is_empty()) {
    LOG(WARNING) << "Refusing to load for invalid virtual URL: "
                 << virtual_url.possibly_invalid_spec();
    return false;
  }

  // Don't attempt to navigate to non-empty invalid URLs.
  if (!dest_url.is_valid() && !dest_url.is_empty()) {
    LOG(WARNING) << "Refusing to load invalid URL: "
                 << dest_url.possibly_invalid_spec();
    return false;
  }

  // The renderer will reject IPC messages with URLs longer than
  // this limit, so don't attempt to navigate with a longer URL.
  if (dest_url.spec().size() > url::kMaxURLChars) {
    LOG(WARNING) << "Refusing to load URL as it exceeds " << url::kMaxURLChars
                 << " characters.";
    return false;
  }

  return true;
}

// See replaced_navigation_entry_data.h for details: this information is meant
// to ensure |*output_entry| keeps track of its original URL (landing page in
// case of server redirects) as it gets replaced (e.g. history.replaceState()),
// without overwriting it later, for main frames.
void CopyReplacedNavigationEntryDataIfPreviouslyEmpty(
    const NavigationEntryImpl& replaced_entry,
    NavigationEntryImpl* output_entry) {
  if (output_entry->GetReplacedEntryData().has_value())
    return;

  ReplacedNavigationEntryData data;
  data.first_committed_url = replaced_entry.GetURL();
  data.first_timestamp = replaced_entry.GetTimestamp();
  data.first_transition_type = replaced_entry.GetTransitionType();
  output_entry->SetReplacedEntryData(data);
}

FrameMsg_Navigate_Type::Value GetNavigationType(
    const GURL& old_url,
    const GURL& new_url,
    ReloadType reload_type,
    const NavigationEntryImpl& entry,
    const FrameNavigationEntry& frame_entry,
    bool is_same_document_history_load) {
  // Reload navigations
  switch (reload_type) {
    case ReloadType::NORMAL:
      return FrameMsg_Navigate_Type::RELOAD;
    case ReloadType::BYPASSING_CACHE:
      return FrameMsg_Navigate_Type::RELOAD_BYPASSING_CACHE;
    case ReloadType::ORIGINAL_REQUEST_URL:
      return FrameMsg_Navigate_Type::RELOAD_ORIGINAL_REQUEST_URL;
    case ReloadType::NONE:
      break;  // Fall through to rest of function.
  }

  if (entry.restore_type() == RestoreType::LAST_SESSION_EXITED_CLEANLY) {
    if (entry.GetHasPostData())
      return FrameMsg_Navigate_Type::RESTORE_WITH_POST;
    else
      return FrameMsg_Navigate_Type::RESTORE;
  }

  // History navigations.
  if (frame_entry.page_state().IsValid()) {
    if (is_same_document_history_load)
      return FrameMsg_Navigate_Type::HISTORY_SAME_DOCUMENT;
    else
      return FrameMsg_Navigate_Type::HISTORY_DIFFERENT_DOCUMENT;
  }
  DCHECK(!is_same_document_history_load);

  // A same-document fragment-navigation happens when the only part of the url
  // that is modified is after the '#' character.
  //
  // When modifying this condition, please take a look at:
  // FrameLoader::shouldPerformFragmentNavigation.
  //
  // Note: this check is only valid for navigations that are not history
  // navigations. For instance, if the history is: 'A#bar' -> 'B' -> 'A#foo', a
  // history navigation from 'A#foo' to 'A#bar' is not a same-document
  // navigation, but a different-document one. This is why history navigation
  // are classified before this check.
  if (new_url.has_ref() && old_url.EqualsIgnoringRef(new_url) &&
      frame_entry.method() == "GET") {
    return FrameMsg_Navigate_Type::SAME_DOCUMENT;
  } else {
    return FrameMsg_Navigate_Type::DIFFERENT_DOCUMENT;
  }
}

// Adjusts the original input URL if needed, to get the URL to actually load and
// the virtual URL, which may differ.
void RewriteUrlForNavigation(const GURL& original_url,
                             BrowserContext* browser_context,
                             GURL* url_to_load,
                             GURL* virtual_url,
                             bool* reverse_on_redirect) {
  // Fix up the given URL before letting it be rewritten, so that any minor
  // cleanup (e.g., removing leading dots) will not lead to a virtual URL.
  *virtual_url = original_url;
  BrowserURLHandlerImpl::GetInstance()->FixupURLBeforeRewrite(virtual_url,
                                                              browser_context);

  // Allow the browser URL handler to rewrite the URL. This will, for example,
  // remove "view-source:" from the beginning of the URL to get the URL that
  // will actually be loaded. This real URL won't be shown to the user, just
  // used internally.
  *url_to_load = *virtual_url;
  BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
      url_to_load, browser_context, reverse_on_redirect);
}

#if DCHECK_IS_ON()
// Helper sanity check function used in debug mode.
void ValidateRequestMatchesEntry(NavigationRequest* request,
                                 NavigationEntryImpl* entry) {
  if (request->frame_tree_node()->IsMainFrame()) {
    DCHECK_EQ(request->browser_initiated(), !entry->is_renderer_initiated());
    DCHECK(ui::PageTransitionTypeIncludingQualifiersIs(
        request->common_params().transition, entry->GetTransitionType()));
  }
  DCHECK_EQ(request->common_params().should_replace_current_entry,
            entry->should_replace_entry());
  DCHECK_EQ(request->request_params().should_clear_history_list,
            entry->should_clear_history_list());
  DCHECK_EQ(request->common_params().has_user_gesture,
            entry->has_user_gesture());
  DCHECK_EQ(request->common_params().base_url_for_data_url,
            entry->GetBaseURLForDataURL());
  DCHECK_EQ(request->request_params().can_load_local_resources,
            entry->GetCanLoadLocalResources());
  DCHECK_EQ(request->common_params().started_from_context_menu,
            entry->has_started_from_context_menu());

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(request->frame_tree_node());
  if (!frame_entry) {
    NOTREACHED();
    return;
  }

  DCHECK_EQ(request->common_params().url, frame_entry->url());
  DCHECK_EQ(request->common_params().method, frame_entry->method());

  size_t redirect_size = request->request_params().redirects.size();
  if (redirect_size == frame_entry->redirect_chain().size()) {
    for (size_t i = 0; i < redirect_size; ++i) {
      DCHECK_EQ(request->request_params().redirects[i],
                frame_entry->redirect_chain()[i]);
    }
  } else {
    NOTREACHED();
  }
}
#endif  // DCHECK_IS_ON()

}  // namespace

// NavigationControllerImpl ----------------------------------------------------

const size_t kMaxEntryCountForTestingNotSet = static_cast<size_t>(-1);

// static
size_t NavigationControllerImpl::max_entry_count_for_testing_ =
    kMaxEntryCountForTestingNotSet;

// Should Reload check for post data? The default is true, but is set to false
// when testing.
static bool g_check_for_repost = true;

// static
std::unique_ptr<NavigationEntry> NavigationController::CreateNavigationEntry(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    bool is_renderer_initiated,
    const std::string& extra_headers,
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  GURL url_to_load;
  GURL virtual_url;
  bool reverse_on_redirect = false;
  RewriteUrlForNavigation(url, browser_context, &url_to_load, &virtual_url,
                          &reverse_on_redirect);

  NavigationEntryImpl* entry = new NavigationEntryImpl(
      nullptr,  // The site instance for tabs is sent on navigation
                // (WebContents::GetSiteInstance).
      url_to_load, referrer, base::string16(), transition,
      is_renderer_initiated, blob_url_loader_factory);
  entry->SetVirtualURL(virtual_url);
  entry->set_user_typed_url(virtual_url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  return base::WrapUnique(entry);
}

// static
void NavigationController::DisablePromptOnRepost() {
  g_check_for_repost = false;
}

base::Time NavigationControllerImpl::TimeSmoother::GetSmoothedTime(
    base::Time t) {
  // If |t| is between the water marks, we're in a run of duplicates
  // or just getting out of it, so increase the high-water mark to get
  // a time that probably hasn't been used before and return it.
  if (low_water_mark_ <= t && t <= high_water_mark_) {
    high_water_mark_ += base::TimeDelta::FromMicroseconds(1);
    return high_water_mark_;
  }

  // Otherwise, we're clear of the last duplicate run, so reset the
  // water marks.
  low_water_mark_ = high_water_mark_ = t;
  return t;
}

NavigationControllerImpl::NavigationControllerImpl(
    NavigationControllerDelegate* delegate,
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      pending_entry_(nullptr),
      failed_pending_entry_id_(0),
      last_committed_entry_index_(-1),
      pending_entry_index_(-1),
      transient_entry_index_(-1),
      delegate_(delegate),
      ssl_manager_(this),
      needs_reload_(false),
      is_initial_navigation_(true),
      in_navigate_to_pending_entry_(false),
      pending_reload_(ReloadType::NONE),
      get_timestamp_callback_(base::Bind(&base::Time::Now)),
      screenshot_manager_(new NavigationEntryScreenshotManager(this)),
      last_committed_reload_type_(ReloadType::NONE) {
  DCHECK(browser_context_);
}

NavigationControllerImpl::~NavigationControllerImpl() {
  DiscardNonCommittedEntriesInternal();
}

WebContents* NavigationControllerImpl::GetWebContents() const {
  return delegate_->GetWebContents();
}

BrowserContext* NavigationControllerImpl::GetBrowserContext() const {
  return browser_context_;
}

void NavigationControllerImpl::Restore(
    int selected_navigation,
    RestoreType type,
    std::vector<std::unique_ptr<NavigationEntry>>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));
  DCHECK_EQ(-1, pending_entry_index_);

  needs_reload_ = true;
  entries_.reserve(entries->size());
  for (auto& entry : *entries)
    entries_.push_back(
        NavigationEntryImpl::FromNavigationEntry(std::move(entry)));

  // At this point, the |entries| is full of empty scoped_ptrs, so it can be
  // cleared out safely.
  entries->clear();

  // And finish the restore.
  FinishRestore(selected_navigation, type);
}

void NavigationControllerImpl::Reload(ReloadType reload_type,
                                      bool check_for_repost) {
  DCHECK_NE(ReloadType::NONE, reload_type);

  if (transient_entry_index_ != -1) {
    // If an interstitial is showing, treat a reload as a navigation to the
    // transient entry's URL.
    NavigationEntryImpl* transient_entry = GetTransientEntry();
    if (!transient_entry)
      return;
    LoadURL(transient_entry->GetURL(),
            Referrer(),
            ui::PAGE_TRANSITION_RELOAD,
            transient_entry->extra_headers());
    return;
  }

  NavigationEntryImpl* entry = nullptr;
  int current_index = -1;

  // If we are reloading the initial navigation, just use the current
  // pending entry.  Otherwise look up the current entry.
  if (IsInitialNavigation() && pending_entry_) {
    entry = pending_entry_;
    // The pending entry might be in entries_ (e.g., after a Clone), so we
    // should also update the current_index.
    current_index = pending_entry_index_;
  } else {
    DiscardNonCommittedEntriesInternal();
    current_index = GetCurrentEntryIndex();
    if (current_index != -1) {
      entry = GetEntryAtIndex(current_index);
    }
  }

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (!entry)
    return;

  // Check if previous navigation was a reload to track consecutive reload
  // operations.
  if (last_committed_reload_type_ != ReloadType::NONE) {
    DCHECK(!last_committed_reload_time_.is_null());
    base::Time now =
        time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
    DCHECK_GT(now, last_committed_reload_time_);
    if (!last_committed_reload_time_.is_null() &&
        now > last_committed_reload_time_) {
      base::TimeDelta delta = now - last_committed_reload_time_;
      UMA_HISTOGRAM_MEDIUM_TIMES("Navigation.Reload.ReloadToReloadDuration",
                                 delta);
      if (last_committed_reload_type_ == ReloadType::NORMAL) {
        UMA_HISTOGRAM_MEDIUM_TIMES(
            "Navigation.Reload.ReloadMainResourceToReloadDuration", delta);
      }
    }
  }

  // Set ReloadType for |entry| in order to check it at commit time.
  entry->set_reload_type(reload_type);

  if (g_check_for_repost && check_for_repost &&
      entry->GetHasPostData()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    delegate_->NotifyBeforeFormRepostWarningShow();

    pending_reload_ = reload_type;
    delegate_->ActivateAndShowRepostFormWarningDialog();
  } else {
    if (!IsInitialNavigation())
      DiscardNonCommittedEntriesInternal();

    pending_entry_ = entry;
    pending_entry_index_ = current_index;
    pending_entry_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);

    NavigateToExistingPendingEntry(reload_type);
  }
}

void NavigationControllerImpl::CancelPendingReload() {
  DCHECK(pending_reload_ != ReloadType::NONE);
  pending_reload_ = ReloadType::NONE;
}

void NavigationControllerImpl::ContinuePendingReload() {
  if (pending_reload_ == ReloadType::NONE) {
    NOTREACHED();
  } else {
    Reload(pending_reload_, false);
    pending_reload_ = ReloadType::NONE;
  }
}

bool NavigationControllerImpl::IsInitialNavigation() const {
  return is_initial_navigation_;
}

bool NavigationControllerImpl::IsInitialBlankNavigation() const {
  // TODO(creis): Once we create a NavigationEntry for the initial blank page,
  // we'll need to check for entry count 1 and restore_type NONE (to exclude
  // the cloned tab case).
  return IsInitialNavigation() && GetEntryCount() == 0;
}

NavigationEntryImpl*
NavigationControllerImpl::GetEntryWithUniqueID(int nav_entry_id) const {
  int index = GetEntryIndexWithUniqueID(nav_entry_id);
  return (index != -1) ? entries_[index].get() : nullptr;
}

void NavigationControllerImpl::SetPendingEntry(
    std::unique_ptr<NavigationEntryImpl> entry) {
  DiscardNonCommittedEntriesInternal();
  pending_entry_ = entry.release();
  DCHECK_EQ(-1, pending_entry_index_);
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_PENDING,
      Source<NavigationController>(this),
      Details<NavigationEntry>(pending_entry_));
}

NavigationEntryImpl* NavigationControllerImpl::GetActiveEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntryImpl* NavigationControllerImpl::GetVisibleEntry() const {
  if (transient_entry_index_ != -1)
    return entries_[transient_entry_index_].get();
  // The pending entry is safe to return for new (non-history), browser-
  // initiated navigations.  Most renderer-initiated navigations should not
  // show the pending entry, to prevent URL spoof attacks.
  //
  // We make an exception for renderer-initiated navigations in new tabs, as
  // long as no other page has tried to access the initial empty document in
  // the new tab.  If another page modifies this blank page, a URL spoof is
  // possible, so we must stop showing the pending entry.
  bool safe_to_show_pending =
      pending_entry_ &&
      // Require a new navigation.
      pending_entry_index_ == -1 &&
      // Require either browser-initiated or an unmodified new tab.
      (!pending_entry_->is_renderer_initiated() || IsUnmodifiedBlankTab());

  // Also allow showing the pending entry for history navigations in a new tab,
  // such as Ctrl+Back.  In this case, no existing page is visible and no one
  // can script the new tab before it commits.
  if (!safe_to_show_pending &&
      pending_entry_ &&
      pending_entry_index_ != -1 &&
      IsInitialNavigation() &&
      !pending_entry_->is_renderer_initiated())
    safe_to_show_pending = true;

  if (safe_to_show_pending)
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationControllerImpl::GetCurrentEntryIndex() const {
  if (transient_entry_index_ != -1)
    return transient_entry_index_;
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntryImpl* NavigationControllerImpl::GetLastCommittedEntry() const {
  if (last_committed_entry_index_ == -1)
    return nullptr;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationControllerImpl::CanViewSource() const {
  const std::string& mime_type = delegate_->GetContentsMimeType();
  bool is_viewable_mime_type = blink::IsSupportedNonImageMimeType(mime_type) &&
                               !media::IsSupportedMediaMimeType(mime_type);
  NavigationEntry* visible_entry = GetVisibleEntry();
  return visible_entry && !visible_entry->IsViewSourceMode() &&
      is_viewable_mime_type && !delegate_->GetInterstitialPage();
}

int NavigationControllerImpl::GetLastCommittedEntryIndex() const {
  // The last committed entry index must always be less than the number of
  // entries.  If there are no entries, it must be -1. However, there may be a
  // transient entry while the last committed entry index is still -1.
  DCHECK_LT(last_committed_entry_index_, GetEntryCount());
  DCHECK(GetEntryCount() || last_committed_entry_index_ == -1);
  return last_committed_entry_index_;
}

int NavigationControllerImpl::GetEntryCount() const {
  DCHECK_LE(entries_.size(), max_entry_count());
  return static_cast<int>(entries_.size());
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtIndex(
    int index) const {
  if (index < 0 || index >= GetEntryCount())
    return nullptr;

  return entries_[index].get();
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtOffset(
    int offset) const {
  return GetEntryAtIndex(GetIndexForOffset(offset));
}

int NavigationControllerImpl::GetIndexForOffset(int offset) const {
  return GetCurrentEntryIndex() + offset;
}

void NavigationControllerImpl::TakeScreenshot() {
  screenshot_manager_->TakeScreenshot();
}

void NavigationControllerImpl::SetScreenshotManager(
    std::unique_ptr<NavigationEntryScreenshotManager> manager) {
  if (manager.get())
    screenshot_manager_ = std::move(manager);
  else
    screenshot_manager_.reset(new NavigationEntryScreenshotManager(this));
}

bool NavigationControllerImpl::CanGoBack() const {
  return CanGoToOffset(-1);
}

bool NavigationControllerImpl::CanGoForward() const {
  return CanGoToOffset(1);
}

bool NavigationControllerImpl::CanGoToOffset(int offset) const {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

void NavigationControllerImpl::GoBack() {
  // Call GoToIndex rather than GoToOffset to get the NOTREACHED() check.
  GoToIndex(GetIndexForOffset(-1));
}

void NavigationControllerImpl::GoForward() {
  // Call GoToIndex rather than GoToOffset to get the NOTREACHED() check.
  GoToIndex(GetIndexForOffset(1));
}

void NavigationControllerImpl::GoToIndex(int index) {
  TRACE_EVENT0("browser,navigation,benchmark",
               "NavigationControllerImpl::GoToIndex");
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  if (transient_entry_index_ != -1) {
    if (index == transient_entry_index_) {
      // Nothing to do when navigating to the transient.
      return;
    }
    if (index > transient_entry_index_) {
      // Removing the transient is goint to shift all entries by 1.
      index--;
    }
  }

  DiscardNonCommittedEntries();

  DCHECK_EQ(nullptr, pending_entry_);
  DCHECK_EQ(-1, pending_entry_index_);
  pending_entry_ = entries_[index].get();
  pending_entry_index_ = index;
  pending_entry_->SetTransitionType(ui::PageTransitionFromInt(
      pending_entry_->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));
  NavigateToExistingPendingEntry(ReloadType::NONE);
}

void NavigationControllerImpl::GoToOffset(int offset) {
  // Note: This is actually reached in unit tests.
  if (!CanGoToOffset(offset))
    return;

  GoToIndex(GetIndexForOffset(offset));
}

bool NavigationControllerImpl::RemoveEntryAtIndex(int index) {
  if (index == last_committed_entry_index_ ||
      index == pending_entry_index_)
    return false;

  RemoveEntryAtIndexInternal(index);
  return true;
}

void NavigationControllerImpl::UpdateVirtualURLToURL(
    NavigationEntryImpl* entry, const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandlerImpl::GetInstance()->ReverseURLRewrite(
          &new_virtual_url, entry->GetVirtualURL(), browser_context_)) {
    entry->SetVirtualURL(new_virtual_url);
  }
}

void NavigationControllerImpl::LoadURL(
    const GURL& url,
    const Referrer& referrer,
    ui::PageTransition transition,
    const std::string& extra_headers) {
  LoadURLParams params(url);
  params.referrer = referrer;
  params.transition_type = transition;
  params.extra_headers = extra_headers;
  LoadURLWithParams(params);
}

void NavigationControllerImpl::LoadURLWithParams(const LoadURLParams& params) {
  TRACE_EVENT1("browser,navigation",
               "NavigationControllerImpl::LoadURLWithParams",
               "url", params.url.possibly_invalid_spec());
  if (HandleDebugURL(params.url, params.transition_type)) {
    // If Telemetry is running, allow the URL load to proceed as if it's
    // unhandled, otherwise Telemetry can't tell if Navigation completed.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            cc::switches::kEnableGpuBenchmarking))
      return;
  }

  // Checks based on params.load_type.
  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
    case LOAD_TYPE_HTTP_POST:
      break;
    case LOAD_TYPE_DATA:
      if (!params.url.SchemeIs(url::kDataScheme)) {
        NOTREACHED() << "Data load must use data scheme.";
        return;
      }
      break;
    default:
      NOTREACHED();
      break;
  }

  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigateWithoutEntry(params);
}

bool NavigationControllerImpl::PendingEntryMatchesHandle(
    NavigationHandleImpl* handle) const {
  return pending_entry_ &&
         pending_entry_->GetUniqueID() == handle->pending_nav_entry_id();
}

bool NavigationControllerImpl::RendererDidNavigate(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    LoadCommittedDetails* details,
    bool is_same_document_navigation,
    NavigationHandleImpl* navigation_handle) {
  is_initial_navigation_ = false;

  // Save the previous state before we clobber it.
  bool overriding_user_agent_changed = false;
  if (GetLastCommittedEntry()) {
    details->previous_url = GetLastCommittedEntry()->GetURL();
    details->previous_entry_index = GetLastCommittedEntryIndex();
    if (pending_entry_ &&
        pending_entry_->GetIsOverridingUserAgent() !=
            GetLastCommittedEntry()->GetIsOverridingUserAgent())
      overriding_user_agent_changed = true;
  } else {
    details->previous_url = GURL();
    details->previous_entry_index = -1;
  }

  // If there is a pending entry at this point, it should have a SiteInstance,
  // except for restored entries.
  bool was_restored = false;
  DCHECK(pending_entry_index_ == -1 || pending_entry_->site_instance() ||
         pending_entry_->restore_type() != RestoreType::NONE);
  if (pending_entry_ && pending_entry_->restore_type() != RestoreType::NONE) {
    pending_entry_->set_restore_type(RestoreType::NONE);
    was_restored = true;
  }

  // If this is a navigation to a matching pending_entry_ and the SiteInstance
  // has changed, this must be treated as a new navigation with replacement.
  // Set the replacement bit here and ClassifyNavigation will identify this
  // case and return NEW_PAGE.
  if (!rfh->GetParent() && pending_entry_ &&
      pending_entry_->GetUniqueID() == params.nav_entry_id &&
      pending_entry_->site_instance() &&
      pending_entry_->site_instance() != rfh->GetSiteInstance()) {
    DCHECK_NE(-1, pending_entry_index_);
    // TODO(nasko,creis): Instead of setting this value here, set
    // should_replace_current_entry on the parameters we send to the
    // renderer process as part of CommitNavigation. The renderer should
    // in turn send it back here as part of |params| and it can be just
    // enforced and renderer process terminated on mismatch.
    details->did_replace_entry = true;
  } else {
    // The renderer tells us whether the navigation replaces the current entry.
    details->did_replace_entry = params.should_replace_current_entry;
  }

  // Do navigation-type specific actions. These will make and commit an entry.
  details->type = ClassifyNavigation(rfh, params);

  // is_same_document must be computed before the entry gets committed.
  details->is_same_document = is_same_document_navigation;

  // Save reload type and timestamp for a reload navigation to detect
  // consecutive reloads when the next reload is requested.
  if (PendingEntryMatchesHandle(navigation_handle)) {
    if (pending_entry_->reload_type() != ReloadType::NONE) {
      last_committed_reload_type_ = pending_entry_->reload_type();
      last_committed_reload_time_ =
          time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
    } else if (!pending_entry_->is_renderer_initiated() ||
               params.gesture == NavigationGestureUser) {
      last_committed_reload_type_ = ReloadType::NONE;
      last_committed_reload_time_ = base::Time();
    }
  }

  switch (details->type) {
    case NAVIGATION_TYPE_NEW_PAGE:
      RendererDidNavigateToNewPage(rfh, params, details->is_same_document,
                                   details->did_replace_entry,
                                   navigation_handle);
      break;
    case NAVIGATION_TYPE_EXISTING_PAGE:
      RendererDidNavigateToExistingPage(rfh, params, details->is_same_document,
                                        was_restored, navigation_handle);
      break;
    case NAVIGATION_TYPE_SAME_PAGE:
      RendererDidNavigateToSamePage(rfh, params, navigation_handle);
      break;
    case NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(rfh, params, details->is_same_document,
                                     details->did_replace_entry);
      break;
    case NAVIGATION_TYPE_AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(rfh, params)) {
        // We don't send a notification about auto-subframe PageState during
        // UpdateStateForFrame, since it looks like nothing has changed.  Send
        // it here at commit time instead.
        NotifyEntryChanged(GetLastCommittedEntry());
        return false;
      }
      break;
    case NAVIGATION_TYPE_NAV_IGNORE:
      // If a pending navigation was in progress, this canceled it.  We should
      // discard it and make sure it is removed from the URL bar.  After that,
      // there is nothing we can do with this navigation, so we just return to
      // the caller that nothing has happened.
      if (pending_entry_) {
        DiscardNonCommittedEntries();
        delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
      }
      return false;
    default:
      NOTREACHED();
  }

  // At this point, we know that the navigation has just completed, so
  // record the time.
  //
  // TODO(akalin): Use "sane time" as described in
  // https://www.chromium.org/developers/design-documents/sane-time .
  base::Time timestamp =
      time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
  DVLOG(1) << "Navigation finished at (smoothed) timestamp "
           << timestamp.ToInternalValue();

  // We should not have a pending entry anymore.  Clear it again in case any
  // error cases above forgot to do so.
  DiscardNonCommittedEntriesInternal();

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  DCHECK(params.page_state.IsValid()) << "Shouldn't see an empty PageState.";
  NavigationEntryImpl* active_entry = GetLastCommittedEntry();
  active_entry->SetTimestamp(timestamp);
  active_entry->SetHttpStatusCode(params.http_status_code);

  // Grab the corresponding FrameNavigationEntry for a few updates, but only if
  // the SiteInstance matches (to avoid updating the wrong entry by mistake).
  // A mismatch can occur if the renderer lies or due to a unique name collision
  // after a race with an OOPIF (see https://crbug.com/616820).
  FrameNavigationEntry* frame_entry =
      active_entry->GetFrameEntry(rfh->frame_tree_node());
  if (frame_entry && frame_entry->site_instance() != rfh->GetSiteInstance())
    frame_entry = nullptr;
  // Make sure we've updated the PageState in one of the helper methods.
  // TODO(creis): Remove the "if" once https://crbug.com/522193 is fixed.
  if (frame_entry) {
    DCHECK(params.page_state == frame_entry->page_state());
  }

  // Use histogram to track memory impact of redirect chain because it's now
  // not cleared for committed entries.
  size_t redirect_chain_size = 0;
  for (size_t i = 0; i < params.redirects.size(); ++i) {
    redirect_chain_size += params.redirects[i].spec().length();
  }
  UMA_HISTOGRAM_COUNTS_1M("Navigation.RedirectChainSize", redirect_chain_size);

  // Once it is committed, we no longer need to track several pieces of state on
  // the entry.
  active_entry->ResetForCommit(frame_entry);

  // The active entry's SiteInstance should match our SiteInstance.
  // TODO(creis): This check won't pass for subframes until we create entries
  // for subframe navigations.
  if (!rfh->GetParent())
    CHECK_EQ(active_entry->site_instance(), rfh->GetSiteInstance());

  // Remember the bindings the renderer process has at this point, so that
  // we do not grant this entry additional bindings if we come back to it.
  active_entry->SetBindings(rfh->GetEnabledBindings());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame = !rfh->GetParent();
  details->http_status_code = params.http_status_code;

  NotifyNavigationEntryCommitted(details);

  if (active_entry->GetURL().SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
      navigation_handle->GetNetErrorCode() == net::OK) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus",
                          !!active_entry->GetSSL().certificate);
  }

  if (overriding_user_agent_changed)
    delegate_->UpdateOverridingUserAgent();

  // Update the nav_entry_id for each RenderFrameHost in the tree, so that each
  // one knows the latest NavigationEntry it is showing (whether it has
  // committed anything in this navigation or not). This allows things like
  // state and title updates from RenderFrames to apply to the latest relevant
  // NavigationEntry.
  int nav_entry_id = active_entry->GetUniqueID();
  for (FrameTreeNode* node : delegate_->GetFrameTree()->Nodes())
    node->current_frame_host()->set_nav_entry_id(nav_entry_id);
  return true;
}

NavigationType NavigationControllerImpl::ClassifyNavigation(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) const {
  if (params.did_create_new_entry) {
    // A new entry. We may or may not have a pending entry for the page, and
    // this may or may not be the main frame.
    if (!rfh->GetParent()) {
      return NAVIGATION_TYPE_NEW_PAGE;
    }

    // When this is a new subframe navigation, we should have a committed page
    // in which it's a subframe. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry())
      return NAVIGATION_TYPE_NAV_IGNORE;

    // Valid subframe navigation.
    return NAVIGATION_TYPE_NEW_SUBFRAME;
  }

  // We only clear the session history when navigating to a new page.
  DCHECK(!params.history_list_was_cleared);

  if (rfh->GetParent()) {
    // All manual subframes would be did_create_new_entry and handled above, so
    // we know this is auto.
    if (GetLastCommittedEntry()) {
      return NAVIGATION_TYPE_AUTO_SUBFRAME;
    } else {
      // We ignore subframes created in non-committed pages; we'd appreciate if
      // people stopped doing that.
      return NAVIGATION_TYPE_NAV_IGNORE;
    }
  }

  if (params.nav_entry_id == 0) {
    // This is a renderer-initiated navigation (nav_entry_id == 0), but didn't
    // create a new page.

    // Just like above in the did_create_new_entry case, it's possible to
    // scribble onto an uncommitted page. Again, there isn't any navigation
    // stuff that we can do, so ignore it here as well.
    NavigationEntry* last_committed = GetLastCommittedEntry();
    if (!last_committed)
      return NAVIGATION_TYPE_NAV_IGNORE;

    // This is history.replaceState() or history.reload().
    // TODO(nasko): With error page isolation, reloading an existing session
    // history entry can result in change of SiteInstance. Check for such a case
    // here and classify it as NEW_PAGE, as such navigations should be treated
    // as new with replacement.
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  if (pending_entry_ && pending_entry_->GetUniqueID() == params.nav_entry_id) {
    // If the SiteInstance of the |pending_entry_| does not match the
    // SiteInstance that got committed, treat this as a new navigation with
    // replacement. This can happen if back/forward/reload encounters a server
    // redirect to a different site or an isolated error page gets successfully
    // reloaded into a different SiteInstance.
    if (pending_entry_->site_instance() &&
        pending_entry_->site_instance() != rfh->GetSiteInstance()) {
      return NAVIGATION_TYPE_NEW_PAGE;
    }

    if (pending_entry_index_ == -1) {
      // In this case, we have a pending entry for a load of a new URL but Blink
      // didn't do a new navigation (params.did_create_new_entry). First check
      // to make sure Blink didn't treat a new cross-process navigation as
      // inert, and thus set params.did_create_new_entry to false. In that case,
      // we must treat it as NEW since the SiteInstance doesn't match the entry.
      if (!GetLastCommittedEntry() ||
          GetLastCommittedEntry()->site_instance() != rfh->GetSiteInstance()) {
        return NAVIGATION_TYPE_NEW_PAGE;
      }

      // Otherwise, this happens when you press enter in the URL bar to reload.
      // We will create a pending entry, but Blink will convert it to a reload
      // since it's the same page and not create a new entry for it (the user
      // doesn't want to have a new back/forward entry when they do this).
      // Therefore we want to just ignore the pending entry and go back to where
      // we were (the "existing entry").
      // TODO(creis,avi): Eliminate SAME_PAGE in https://crbug.com/536102.
      return NAVIGATION_TYPE_SAME_PAGE;
    }
  }

  // Everything below here is assumed to be an existing entry, but if there is
  // no last committed entry, we must consider it a new navigation instead.
  if (!GetLastCommittedEntry())
    return NAVIGATION_TYPE_NEW_PAGE;

  if (params.intended_as_new_entry) {
    // This was intended to be a navigation to a new entry but the pending entry
    // got cleared in the meanwhile. Classify as EXISTING_PAGE because we may or
    // may not have a pending entry.
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  if (params.url_is_unreachable && failed_pending_entry_id_ != 0 &&
      params.nav_entry_id == failed_pending_entry_id_) {
    // If the renderer was going to a new pending entry that got cleared because
    // of an error, this is the case of the user trying to retry a failed load
    // by pressing return. Classify as EXISTING_PAGE because we probably don't
    // have a pending entry.
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithUniqueID(params.nav_entry_id);
  if (existing_entry_index == -1) {
    // The renderer has committed a navigation to an entry that no longer
    // exists. Because the renderer is showing that page, resurrect that entry.
    return NAVIGATION_TYPE_NEW_PAGE;
  }

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  return NAVIGATION_TYPE_EXISTING_PAGE;
}

void NavigationControllerImpl::RendererDidNavigateToNewPage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    bool replace_entry,
    NavigationHandleImpl* handle) {
  std::unique_ptr<NavigationEntryImpl> new_entry;
  bool update_virtual_url = false;

  // First check if this is an in-page navigation.  If so, clone the current
  // entry instead of looking at the pending entry, because the pending entry
  // does not have any subframe history items.
  if (is_same_document && GetLastCommittedEntry()) {
    FrameNavigationEntry* frame_entry = new FrameNavigationEntry(
        rfh->frame_tree_node()->unique_name(), params.item_sequence_number,
        params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
        params.url, params.referrer, params.redirects, params.page_state,
        params.method, params.post_id, nullptr /* blob_url_loader_factory */);

    new_entry = GetLastCommittedEntry()->CloneAndReplace(
        frame_entry, true, rfh->frame_tree_node(),
        delegate_->GetFrameTree()->root());
    if (new_entry->GetURL().GetOrigin() != params.url.GetOrigin()) {
      // TODO(jam): we had one report of this with a URL that was redirecting to
      // only tildes. Until we understand that better, don't copy the cert in
      // this case.
      new_entry->GetSSL() = SSLStatus();

      if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
          handle->GetNetErrorCode() == net::OK) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus.NewPageInPageOriginMismatch",
            !!new_entry->GetSSL().certificate);
      }
    }

    // We expect |frame_entry| to be owned by |new_entry|.  This should never
    // fail, because it's the main frame.
    CHECK(frame_entry->HasOneRef());

    update_virtual_url = new_entry->update_virtual_url_with_url();

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        handle->GetNetErrorCode() == net::OK) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus.NewPageInPage",
                            !!new_entry->GetSSL().certificate);
    }
  }

  // Only make a copy of the pending entry if it is appropriate for the new page
  // that was just loaded. Verify this by checking if the entry corresponds
  // to the given navigation handle. Additionally, coarsely check that:
  // 1. The SiteInstance hasn't been assigned to something else.
  // 2. The pending entry was intended as a new entry, rather than being a
  // history navigation that was interrupted by an unrelated,
  // renderer-initiated navigation.
  // TODO(csharrison): Investigate whether we can remove some of the coarser
  // checks.
  if (!new_entry &&
      PendingEntryMatchesHandle(handle) && pending_entry_index_ == -1 &&
      (!pending_entry_->site_instance() ||
       pending_entry_->site_instance() == rfh->GetSiteInstance())) {
    new_entry = pending_entry_->Clone();

    update_virtual_url = new_entry->update_virtual_url_with_url();
    new_entry->GetSSL() = SSLStatus(handle->GetSSLInfo());

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        handle->GetNetErrorCode() == net::OK) {
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.SecureSchemeHasSSLStatus.NewPagePendingEntryMatches",
          !!new_entry->GetSSL().certificate);
    }
  }

  // For non-in-page commits with no matching pending entry, create a new entry.
  if (!new_entry) {
    new_entry = base::WrapUnique(new NavigationEntryImpl);

    // Find out whether the new entry needs to update its virtual URL on URL
    // change and set up the entry accordingly. This is needed to correctly
    // update the virtual URL when replaceState is called after a pushState.
    GURL url = params.url;
    bool needs_update = false;
    BrowserURLHandlerImpl::GetInstance()->RewriteURLIfNecessary(
        &url, browser_context_, &needs_update);
    new_entry->set_update_virtual_url_with_url(needs_update);

    // When navigating to a new page, give the browser URL handler a chance to
    // update the virtual URL based on the new URL. For example, this is needed
    // to show chrome://bookmarks/#1 when the bookmarks webui extension changes
    // the URL.
    update_virtual_url = needs_update;
    new_entry->GetSSL() = SSLStatus(handle->GetSSLInfo());

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        handle->GetNetErrorCode() == net::OK) {
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.SecureSchemeHasSSLStatus.NewPageNoMatchingEntry",
          !!new_entry->GetSSL().certificate);
    }
  }

  // Don't use the page type from the pending entry. Some interstitial page
  // may have set the type to interstitial. Once we commit, however, the page
  // type must always be normal or error.
  new_entry->set_page_type(params.url_is_unreachable ? PAGE_TYPE_ERROR
                                                     : PAGE_TYPE_NORMAL);
  new_entry->SetURL(params.url);
  if (update_virtual_url)
    UpdateVirtualURLToURL(new_entry.get(), params.url);
  new_entry->SetReferrer(params.referrer);
  new_entry->SetTransitionType(params.transition);
  new_entry->set_site_instance(
      static_cast<SiteInstanceImpl*>(rfh->GetSiteInstance()));
  new_entry->SetOriginalRequestURL(params.original_request_url);
  new_entry->SetIsOverridingUserAgent(params.is_overriding_user_agent);

  // Update the FrameNavigationEntry for new main frame commits.
  FrameNavigationEntry* frame_entry =
      new_entry->GetFrameEntry(rfh->frame_tree_node());
  frame_entry->set_frame_unique_name(rfh->frame_tree_node()->unique_name());
  frame_entry->set_item_sequence_number(params.item_sequence_number);
  frame_entry->set_document_sequence_number(params.document_sequence_number);
  frame_entry->set_redirect_chain(params.redirects);
  frame_entry->SetPageState(params.page_state);
  frame_entry->set_method(params.method);
  frame_entry->set_post_id(params.post_id);

  // history.pushState() is classified as a navigation to a new page, but sets
  // is_same_document to true. In this case, we already have the title and
  // favicon available, so set them immediately.
  if (is_same_document && GetLastCommittedEntry()) {
    new_entry->SetTitle(GetLastCommittedEntry()->GetTitle());
    new_entry->GetFavicon() = GetLastCommittedEntry()->GetFavicon();
  }

  DCHECK(!params.history_list_was_cleared || !replace_entry);
  // The browser requested to clear the session history when it initiated the
  // navigation. Now we know that the renderer has updated its state accordingly
  // and it is safe to also clear the browser side history.
  if (params.history_list_was_cleared) {
    DiscardNonCommittedEntriesInternal();
    entries_.clear();
    last_committed_entry_index_ = -1;
  }

  // If this is a new navigation with replacement and there is a
  // pending_entry_ which matches the navigation reported by the renderer
  // process, then it should be the one replaced, so update the
  // last_committed_entry_index_ to use it.
  if (replace_entry && pending_entry_index_ != -1 &&
      pending_entry_->GetUniqueID() == params.nav_entry_id) {
    last_committed_entry_index_ = pending_entry_index_;
  }

  InsertOrReplaceEntry(std::move(new_entry), replace_entry);
}

void NavigationControllerImpl::RendererDidNavigateToExistingPage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    bool was_restored,
    NavigationHandleImpl* handle) {
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";

  // We should only get here for main frame navigations.
  DCHECK(!rfh->GetParent());

  NavigationEntryImpl* entry;
  if (params.intended_as_new_entry) {
    // This was intended as a new entry but the pending entry was lost in the
    // meanwhile and no new page was created. We are stuck at the last committed
    // entry.
    entry = GetLastCommittedEntry();
    // If this is a same document navigation, then there's no SSLStatus in the
    // NavigationHandle so don't overwrite the existing entry's SSLStatus.
    if (!is_same_document)
      entry->GetSSL() = SSLStatus(handle->GetSSLInfo());

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        handle->GetNetErrorCode() == net::OK) {
      bool has_cert = !!entry->GetSSL().certificate;
      if (is_same_document) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentIntendedAsNew",
            has_cert);
      } else {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageDifferentDocumentIntendedAsNew",
            has_cert);
      }
    }
  } else if (params.nav_entry_id) {
    // This is a browser-initiated navigation (back/forward/reload).
    entry = GetEntryWithUniqueID(params.nav_entry_id);

    if (is_same_document) {
      // There's no SSLStatus in the NavigationHandle for same document
      // navigations, so normally we leave |entry|'s SSLStatus as is. However if
      // this was a restored same document navigation entry, then it won't have
      // an SSLStatus. So we need to copy over the SSLStatus from the entry that
      // navigated it.
      NavigationEntryImpl* last_entry = GetLastCommittedEntry();
      if (entry->GetURL().GetOrigin() == last_entry->GetURL().GetOrigin() &&
          last_entry->GetSSL().initialized && !entry->GetSSL().initialized &&
          was_restored) {
        entry->GetSSL() = last_entry->GetSSL();
      }
    } else {
      // In rapid back/forward navigations |handle| sometimes won't have a cert
      // (http://crbug.com/727892). So we use the handle's cert if it exists,
      // otherwise we only reuse the existing cert if the origins match.
      if (handle->GetSSLInfo().is_valid()) {
        entry->GetSSL() = SSLStatus(handle->GetSSLInfo());
      } else if (entry->GetURL().GetOrigin() != handle->GetURL().GetOrigin()) {
        entry->GetSSL() = SSLStatus();
      }
    }

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        handle->GetNetErrorCode() == net::OK) {
      bool has_cert = !!entry->GetSSL().certificate;
      if (is_same_document && was_restored) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentRestoredBrowserInitiated",
            has_cert);
      } else if (is_same_document && !was_restored) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentBrowserInitiated",
            has_cert);
      } else if (!is_same_document && was_restored) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageRestoredBrowserInitiated",
            has_cert);
      } else {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus.ExistingPageBrowserInitiated",
            has_cert);
      }
    }
  } else {
    // This is renderer-initiated. The only kinds of renderer-initated
    // navigations that are EXISTING_PAGE are reloads and history.replaceState,
    // which land us at the last committed entry.
    entry = GetLastCommittedEntry();

    // TODO(crbug.com/751023): Set page transition type to PAGE_TRANSITION_LINK
    // to avoid misleading interpretations (e.g. URLs paired with
    // PAGE_TRANSITION_TYPED that haven't actually been typed) as well as to fix
    // the inconsistency with what we report to observers (PAGE_TRANSITION_LINK
    // | PAGE_TRANSITION_CLIENT_REDIRECT).

    CopyReplacedNavigationEntryDataIfPreviouslyEmpty(*entry, entry);

    // If this is a same document navigation, then there's no SSLStatus in the
    // NavigationHandle so don't overwrite the existing entry's SSLStatus.
    if (!is_same_document)
      entry->GetSSL() = SSLStatus(handle->GetSSLInfo());

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        handle->GetNetErrorCode() == net::OK) {
      bool has_cert = !!entry->GetSSL().certificate;
      if (is_same_document) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageSameDocumentRendererInitiated",
            has_cert);
      } else {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus."
            "ExistingPageDifferentDocumentRendererInitiated",
            has_cert);
      }
    }
  }
  DCHECK(entry);

  // The URL may have changed due to redirects.
  entry->set_page_type(params.url_is_unreachable ? PAGE_TYPE_ERROR
                                                 : PAGE_TYPE_NORMAL);
  entry->SetURL(params.url);
  entry->SetReferrer(params.referrer);
  if (entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(entry, params.url);

  // The site instance will normally be the same except
  // 1) session restore, when no site instance will be assigned or
  // 2) redirect, when the site instance is reset.
  DCHECK(entry->site_instance() == nullptr ||
         !entry->GetRedirectChain().empty() ||
         entry->site_instance() == rfh->GetSiteInstance());

  // Update the existing FrameNavigationEntry to ensure all of its members
  // reflect the parameters coming from the renderer process.
  entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, params.referrer, params.redirects, params.page_state,
      params.method, params.post_id, nullptr /* blob_url_loader_factory */);

  // The redirected to page should not inherit the favicon from the previous
  // page.
  if (ui::PageTransitionIsRedirect(params.transition) && !is_same_document)
    entry->GetFavicon() = FaviconStatus();

  // The entry we found in the list might be pending if the user hit
  // back/forward/reload. This load should commit it (since it's already in the
  // list, we can just discard the pending pointer).  We should also discard the
  // pending entry if it corresponds to a different navigation, since that one
  // is now likely canceled.  If it is not canceled, we will treat it as a new
  // navigation when it arrives, which is also ok.
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  DiscardNonCommittedEntriesInternal();

  // If a transient entry was removed, the indices might have changed, so we
  // have to query the entry index again.
  last_committed_entry_index_ = GetIndexOfEntry(entry);
}

void NavigationControllerImpl::RendererDidNavigateToSamePage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    NavigationHandleImpl* handle) {
  // This classification says that we have a pending entry that's the same as
  // the last committed entry. This entry is guaranteed to exist by
  // ClassifyNavigation. All we need to do is update the existing entry.
  NavigationEntryImpl* existing_entry = GetLastCommittedEntry();

  // If we classified this correctly, the SiteInstance should not have changed.
  CHECK_EQ(existing_entry->site_instance(), rfh->GetSiteInstance());

  // We assign the entry's unique ID to be that of the new one. Since this is
  // always the result of a user action, we want to dismiss infobars, etc. like
  // a regular user-initiated navigation.
  DCHECK_EQ(pending_entry_->GetUniqueID(), params.nav_entry_id);
  existing_entry->set_unique_id(pending_entry_->GetUniqueID());

  // The URL may have changed due to redirects.
  existing_entry->set_page_type(params.url_is_unreachable ? PAGE_TYPE_ERROR
                                                          : PAGE_TYPE_NORMAL);
  if (existing_entry->update_virtual_url_with_url())
    UpdateVirtualURLToURL(existing_entry, params.url);
  existing_entry->SetURL(params.url);

  // If a user presses enter in the omnibox and the server redirects, the URL
  // might change (but it's still considered a SAME_PAGE navigation). So we must
  // update the SSL status.
  existing_entry->GetSSL() = SSLStatus(handle->GetSSLInfo());

  if (existing_entry->GetURL().SchemeIs(url::kHttpsScheme) &&
      !rfh->GetParent() && handle->GetNetErrorCode() == net::OK) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus.SamePage",
                          !!existing_entry->GetSSL().certificate);
  }

  // The extra headers may have changed due to reloading with different headers.
  existing_entry->set_extra_headers(pending_entry_->extra_headers());

  // Update the existing FrameNavigationEntry to ensure all of its members
  // reflect the parameters coming from the renderer process.
  existing_entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, params.referrer, params.redirects, params.page_state,
      params.method, params.post_id, nullptr /* blob_url_loader_factory */);

  DiscardNonCommittedEntries();
}

void NavigationControllerImpl::RendererDidNavigateNewSubframe(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    bool replace_entry) {
  DCHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                      ui::PAGE_TRANSITION_MANUAL_SUBFRAME));

  // Manual subframe navigations just get the current entry cloned so the user
  // can go back or forward to it. The actual subframe information will be
  // stored in the page state for each of those entries. This happens out of
  // band with the actual navigations.
  DCHECK(GetLastCommittedEntry()) << "ClassifyNavigation should guarantee "
                                  << "that a last committed entry exists.";

  // The DCHECK below documents the fact that we don't know of any situation
  // where |replace_entry| is true for subframe navigations. This simplifies
  // reasoning about the replacement struct for subframes (see
  // CopyReplacedNavigationEntryDataIfPreviouslyEmpty()).
  DCHECK(!replace_entry);

  // Make sure we don't leak frame_entry if new_entry doesn't take ownership.
  scoped_refptr<FrameNavigationEntry> frame_entry(new FrameNavigationEntry(
      rfh->frame_tree_node()->unique_name(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, params.referrer, params.redirects, params.page_state,
      params.method, params.post_id, nullptr /* blob_url_loader_factory */));

  std::unique_ptr<NavigationEntryImpl> new_entry =
      GetLastCommittedEntry()->CloneAndReplace(
          frame_entry.get(), is_same_document, rfh->frame_tree_node(),
          delegate_->GetFrameTree()->root());

  // TODO(creis): Update this to add the frame_entry if we can't find the one
  // to replace, which can happen due to a unique name change.  See
  // https://crbug.com/607205.  For now, frame_entry will be deleted when it
  // goes out of scope if it doesn't get used.

  InsertOrReplaceEntry(std::move(new_entry), replace_entry);
}

bool NavigationControllerImpl::RendererDidNavigateAutoSubframe(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  DCHECK(ui::PageTransitionCoreTypeIs(params.transition,
                                      ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // We're guaranteed to have a previously committed entry, and we now need to
  // handle navigation inside of a subframe in it without creating a new entry.
  DCHECK(GetLastCommittedEntry());

  // For newly created subframes, we don't need to send a commit notification.
  // This is only necessary for history navigations in subframes.
  bool send_commit_notification = false;

  // If the |nav_entry_id| is non-zero and matches an existing entry, this is
  // a history navigation.  Update the last committed index accordingly.
  // If we don't recognize the |nav_entry_id|, it might be a recently pruned
  // entry.  We'll handle it below.
  if (params.nav_entry_id) {
    int entry_index = GetEntryIndexWithUniqueID(params.nav_entry_id);
    if (entry_index != -1 && entry_index != last_committed_entry_index_) {
      // Make sure that a subframe commit isn't changing the main frame's
      // origin. Otherwise the renderer process may be confused, leading to a
      // URL spoof. We can't check the path since that may change
      // (https://crbug.com/373041).
      // TODO(creis): For now, restrict this check to HTTP(S) origins, because
      // about:blank, file, and unique origins are more subtle to get right.
      // We'll abstract out the relevant checks from IsURLSameDocumentNavigation
      // and share them here.  See https://crbug.com/618104.
      const GURL& dest_top_url = GetEntryAtIndex(entry_index)->GetURL();
      const GURL& current_top_url = GetLastCommittedEntry()->GetURL();
      if (current_top_url.SchemeIsHTTPOrHTTPS() &&
          dest_top_url.SchemeIsHTTPOrHTTPS() &&
          current_top_url.GetOrigin() != dest_top_url.GetOrigin()) {
        bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                        bad_message::NC_AUTO_SUBFRAME);
      }

      // We only need to discard the pending entry in this history navigation
      // case.  For newly created subframes, there was no pending entry.
      last_committed_entry_index_ = entry_index;
      DiscardNonCommittedEntriesInternal();

      // History navigations should send a commit notification.
      send_commit_notification = true;
    }
  }

  // This may be a "new auto" case where we add a new FrameNavigationEntry, or
  // it may be a "history auto" case where we update an existing one.
  NavigationEntryImpl* last_committed = GetLastCommittedEntry();
  last_committed->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, params.referrer, params.redirects, params.page_state,
      params.method, params.post_id, nullptr /* blob_url_loader_factory */);

  return send_commit_notification;
}

int NavigationControllerImpl::GetIndexOfEntry(
    const NavigationEntryImpl* entry) const {
  for (size_t i = 0; i < entries_.size(); ++i) {
    if (entries_[i].get() == entry)
      return i;
  }
  return -1;
}

// There are two general cases where a navigation is "same-document":
// 1. A fragment navigation, in which the url is kept the same except for the
//    reference fragment.
// 2. A history API navigation (pushState and replaceState). This case is
//    always same-document, but the urls are not guaranteed to match excluding
//    the fragment. The relevant spec allows pushState/replaceState to any URL
//    on the same origin.
// However, due to reloads, even identical urls are *not* guaranteed to be
// same-document navigations, we have to trust the renderer almost entirely.
// The one thing we do know is that cross-origin navigations will *never* be
// same-document. Therefore, trust the renderer if the URLs are on the same
// origin, and assume the renderer is malicious if a cross-origin navigation
// claims to be same-document.
//
// TODO(creis): Clean up and simplify the about:blank and origin checks below,
// which are likely redundant with each other.  Be careful about data URLs vs
// about:blank, both of which are unique origins and thus not considered equal.
bool NavigationControllerImpl::IsURLSameDocumentNavigation(
    const GURL& url,
    const url::Origin& origin,
    bool renderer_says_same_document,
    RenderFrameHost* rfh) const {
  RenderFrameHostImpl* rfhi = static_cast<RenderFrameHostImpl*>(rfh);
  GURL last_committed_url;
  if (rfh->GetParent()) {
    // Use the FrameTreeNode's current_url and not rfh->GetLastCommittedURL(),
    // which might be empty in a new RenderFrameHost after a process swap.
    // Here, we care about the last committed URL in the FrameTreeNode,
    // regardless of which process it is in.
    last_committed_url = rfhi->frame_tree_node()->current_url();
  } else {
    NavigationEntry* last_committed = GetLastCommittedEntry();
    // There must be a last-committed entry to compare URLs to. TODO(avi): When
    // might Blink say that a navigation is in-page yet there be no last-
    // committed entry?
    if (!last_committed)
      return false;
    last_committed_url = last_committed->GetURL();
  }

  WebPreferences prefs = rfh->GetRenderViewHost()->GetWebkitPreferences();
  const url::Origin& committed_origin =
      rfhi->frame_tree_node()->current_origin();
  bool is_same_origin = last_committed_url.is_empty() ||
                        // TODO(japhet): We should only permit navigations
                        // originating from about:blank to be in-page if the
                        // about:blank is the first document that frame loaded.
                        // We don't have sufficient information to identify
                        // that case at the moment, so always allow about:blank
                        // for now.
                        last_committed_url == url::kAboutBlankURL ||
                        last_committed_url.GetOrigin() == url.GetOrigin() ||
                        committed_origin == origin ||
                        !prefs.web_security_enabled ||
                        (prefs.allow_universal_access_from_file_urls &&
                         committed_origin.scheme() == url::kFileScheme);
  if (!is_same_origin && renderer_says_same_document) {
    bad_message::ReceivedBadMessage(rfh->GetProcess(),
                                    bad_message::NC_IN_PAGE_NAVIGATION);
  }
  return is_same_origin && renderer_says_same_document;
}

void NavigationControllerImpl::CopyStateFrom(const NavigationController& temp,
                                             bool needs_reload) {
  const NavigationControllerImpl& source =
      static_cast<const NavigationControllerImpl&>(temp);
  // Verify that we look new.
  DCHECK(GetEntryCount() == 0 && !GetPendingEntry());

  if (source.GetEntryCount() == 0)
    return;  // Nothing new to do.

  needs_reload_ = needs_reload;
  InsertEntriesFrom(source, source.GetEntryCount());

  for (auto it = source.session_storage_namespace_map_.begin();
       it != source.session_storage_namespace_map_.end(); ++it) {
    SessionStorageNamespaceImpl* source_namespace =
        static_cast<SessionStorageNamespaceImpl*>(it->second.get());
    session_storage_namespace_map_[it->first] = source_namespace->Clone();
  }

  FinishRestore(source.last_committed_entry_index_,
                RestoreType::CURRENT_SESSION);
}

void NavigationControllerImpl::CopyStateFromAndPrune(
    NavigationController* temp,
    bool replace_entry) {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());

  NavigationControllerImpl* source =
      static_cast<NavigationControllerImpl*>(temp);

  // Remove all the entries leaving the last committed entry.
  PruneAllButLastCommittedInternal();

  // We now have one entry, possibly with a new pending entry.  Ensure that
  // adding the entries from source won't put us over the limit.
  DCHECK_EQ(1, GetEntryCount());
  if (!replace_entry)
    source->PruneOldestEntryIfFull();

  // Insert the entries from source. Don't use source->GetCurrentEntryIndex as
  // we don't want to copy over the transient entry. Ignore any pending entry,
  // since it has not committed in source.
  int max_source_index = source->last_committed_entry_index_;
  if (max_source_index == -1)
    max_source_index = source->GetEntryCount();
  else
    max_source_index++;

  // Ignore the source's current entry if merging with replacement.
  // TODO(davidben): This should preserve entries forward of the current
  // too. http://crbug.com/317872
  if (replace_entry && max_source_index > 0)
    max_source_index--;

  InsertEntriesFrom(*source, max_source_index);

  // Adjust indices such that the last entry and pending are at the end now.
  last_committed_entry_index_ = GetEntryCount() - 1;

  delegate_->SetHistoryOffsetAndLength(last_committed_entry_index_,
                                       GetEntryCount());
}

bool NavigationControllerImpl::CanPruneAllButLastCommitted() {
  // If there is no last committed entry, we cannot prune.  Even if there is a
  // pending entry, it may not commit, leaving this WebContents blank, despite
  // possibly giving it new entries via CopyStateFromAndPrune.
  if (last_committed_entry_index_ == -1)
    return false;

  // We cannot prune if there is a pending entry at an existing entry index.
  // It may not commit, so we have to keep the last committed entry, and thus
  // there is no sensible place to keep the pending entry.  It is ok to have
  // a new pending entry, which can optionally commit as a new navigation.
  if (pending_entry_index_ != -1)
    return false;

  // We should not prune if we are currently showing a transient entry.
  if (transient_entry_index_ != -1)
    return false;

  return true;
}

void NavigationControllerImpl::PruneAllButLastCommitted() {
  PruneAllButLastCommittedInternal();

  DCHECK_EQ(0, last_committed_entry_index_);
  DCHECK_EQ(1, GetEntryCount());

  delegate_->SetHistoryOffsetAndLength(last_committed_entry_index_,
                                       GetEntryCount());
}

void NavigationControllerImpl::PruneAllButLastCommittedInternal() {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());

  // Erase all entries but the last committed entry.  There may still be a
  // new pending entry after this.
  entries_.erase(entries_.begin(),
                 entries_.begin() + last_committed_entry_index_);
  entries_.erase(entries_.begin() + 1, entries_.end());
  last_committed_entry_index_ = 0;
}

void NavigationControllerImpl::DeleteNavigationEntries(
    const DeletionPredicate& deletionPredicate) {
  // It is up to callers to check the invariants before calling this.
  CHECK(CanPruneAllButLastCommitted());
  std::vector<int> delete_indices;
  for (size_t i = 0; i < entries_.size(); i++) {
    if (i != static_cast<size_t>(last_committed_entry_index_) &&
        deletionPredicate.Run(*entries_[i])) {
      delete_indices.push_back(i);
    }
  }
  if (delete_indices.empty())
    return;

  if (delete_indices.size() == GetEntryCount() - 1U) {
    PruneAllButLastCommitted();
  } else {
    // Do the deletion in reverse to preserve indices.
    for (auto it = delete_indices.rbegin(); it != delete_indices.rend(); ++it) {
      RemoveEntryAtIndex(*it);
    }
    delegate_->SetHistoryOffsetAndLength(last_committed_entry_index_,
                                         GetEntryCount());
  }
  delegate()->NotifyNavigationEntriesDeleted();
}

void NavigationControllerImpl::DiscardPendingEntry(bool was_failure) {
  // It is not safe to call DiscardPendingEntry while NavigateToEntry is in
  // progress, since this will cause a use-after-free.  (We only allow this
  // when the tab is being destroyed for shutdown, since it won't return to
  // NavigateToEntry in that case.)  http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_ || delegate_->IsBeingDestroyed());

  if (was_failure && pending_entry_) {
    failed_pending_entry_id_ = pending_entry_->GetUniqueID();
  } else {
    failed_pending_entry_id_ = 0;
  }

  if (pending_entry_) {
    if (pending_entry_index_ == -1)
      delete pending_entry_;
    pending_entry_index_ = -1;
    pending_entry_ = nullptr;
  }
}

void NavigationControllerImpl::SetPendingNavigationSSLError(bool error) {
  if (pending_entry_)
    pending_entry_->set_ssl_error(error);
}

#if defined(OS_ANDROID)
// static
bool NavigationControllerImpl::ValidateDataURLAsString(
    const scoped_refptr<const base::RefCountedString>& data_url_as_string) {
  if (!data_url_as_string)
    return false;

  if (data_url_as_string->size() > kMaxLengthOfDataURLString)
    return false;

  // The number of characters that is enough for validating a data: URI.
  // From the GURL's POV, the only important part here is scheme, it doesn't
  // check the actual content. Thus we can take only the prefix of the url, to
  // avoid unneeded copying of a potentially long string.
  const size_t kDataUriPrefixMaxLen = 64;
  GURL data_url(
      std::string(data_url_as_string->front_as<char>(),
                  std::min(data_url_as_string->size(), kDataUriPrefixMaxLen)));
  if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme))
    return false;

  return true;
}
#endif

bool NavigationControllerImpl::StartHistoryNavigationInNewSubframe(
    RenderFrameHostImpl* render_frame_host,
    const GURL& default_url) {
  NavigationEntryImpl* entry =
      GetEntryWithUniqueID(render_frame_host->nav_entry_id());
  if (!entry)
    return false;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(render_frame_host->frame_tree_node());
  if (!frame_entry)
    return false;

  // Track how often history navigations load a different URL into a subframe
  // than the frame's default URL.
  bool restoring_different_url = frame_entry->url() != default_url;
  UMA_HISTOGRAM_BOOLEAN("SessionRestore.RestoredSubframeURL",
                        restoring_different_url);
  // If this frame's unique name uses a frame path, record the name length.
  // If these names are long in practice, then a proposed plan to truncate
  // unique names might affect restore behavior, since it is complex to deal
  // with truncated names inside frame paths.
  if (restoring_different_url) {
    const std::string& unique_name =
        render_frame_host->frame_tree_node()->unique_name();
    const char kFramePathPrefix[] = "<!--framePath ";
    if (base::StartsWith(unique_name, kFramePathPrefix,
                         base::CompareCase::SENSITIVE)) {
      UMA_HISTOGRAM_COUNTS_1M("SessionRestore.RestoreSubframeFramePathLength",
                              unique_name.size());
    }
  }

  std::unique_ptr<NavigationRequest> request = CreateNavigationRequestFromEntry(
      render_frame_host->frame_tree_node(), *entry, frame_entry,
      ReloadType::NONE, false /* is_same_document_history_load */,
      true /* is_history_navigation_in_new_child */);

  if (!request)
    return false;

  render_frame_host->frame_tree_node()->navigator()->Navigate(
      std::move(request), ReloadType::NONE, RestoreType::NONE);

  return true;
}

void NavigationControllerImpl::NavigateFromFrameProxy(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    bool is_renderer_initiated,
    SiteInstance* source_site_instance,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry,
    const std::string& method,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const std::string& extra_headers,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  FrameTreeNode* node = render_frame_host->frame_tree_node();
  // Create a NavigationEntry for the transfer, without making it the pending
  // entry. Subframe transfers should have a clone of the last committed entry
  // with a FrameNavigationEntry for the target frame. Main frame transfers
  // should have a new NavigationEntry.
  // TODO(creis): Make this unnecessary by creating (and validating) the params
  // directly, passing them to the destination RenderFrameHost.  See
  // https://crbug.com/536906.
  std::unique_ptr<NavigationEntryImpl> entry;
  if (!node->IsMainFrame()) {
    // Subframe case: create FrameNavigationEntry.
    if (GetLastCommittedEntry()) {
      entry = GetLastCommittedEntry()->Clone();
      entry->set_extra_headers(extra_headers);
      // TODO(arthursonzogni): What about |is_renderer_initiated|?
      // Renderer-initiated navigation that target a remote frame are currently
      // classified as browser-initiated when this one has already navigated.
      // See https://crbug.com/722251.
    } else {
      // If there's no last committed entry, create an entry for about:blank
      // with a subframe entry for our destination.
      // TODO(creis): Ensure this case can't exist in https://crbug.com/524208.
      entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
          GURL(url::kAboutBlankURL), referrer, page_transition,
          is_renderer_initiated, extra_headers, browser_context_,
          nullptr /* blob_url_loader_factory */));
    }
    entry->AddOrUpdateFrameEntry(
        node, -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), url, referrer,
        std::vector<GURL>(), PageState(), method, -1, blob_url_loader_factory);
  } else {
    // Main frame case.
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        url, referrer, page_transition, is_renderer_initiated, extra_headers,
        browser_context_, blob_url_loader_factory));
    entry->root_node()->frame_entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(source_site_instance));
    entry->root_node()->frame_entry->set_method(method);
  }

  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  if (should_replace_current_entry && GetEntryCount() > 0)
    entry->set_should_replace_entry(true);

  bool override_user_agent = false;
  if (GetLastCommittedEntry() &&
      GetLastCommittedEntry()->GetIsOverridingUserAgent()) {
    entry->SetIsOverridingUserAgent(true);
    override_user_agent = true;
  }
  // TODO(creis): Set user gesture and intent received timestamp on Android.

  // We may not have successfully added the FrameNavigationEntry to |entry|
  // above (per https://crbug.com/608402), in which case we create it from
  // scratch.  This works because we do not depend on |frame_entry| being inside
  // |entry| during NavigateToEntry.  This will go away when we shortcut this
  // further in https://crbug.com/536906.
  scoped_refptr<FrameNavigationEntry> frame_entry(entry->GetFrameEntry(node));
  if (!frame_entry) {
    frame_entry = new FrameNavigationEntry(
        node->unique_name(), -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), url, referrer,
        std::vector<GURL>(), PageState(), method, -1, blob_url_loader_factory);
  }

  LoadURLParams params(url);
  params.source_site_instance = source_site_instance;
  params.load_type = method == "POST" ? LOAD_TYPE_HTTP_POST : LOAD_TYPE_DEFAULT;
  params.transition_type = page_transition;
  params.frame_tree_node_id =
      render_frame_host->frame_tree_node()->frame_tree_node_id();
  params.referrer = referrer;
  /* params.redirect_chain: skip */
  params.extra_headers = extra_headers;
  params.is_renderer_initiated = is_renderer_initiated;
  params.override_user_agent = UA_OVERRIDE_INHERIT;
  /* params.base_url_for_data_url: skip */
  /* params.virtual_url_for_data_url: skip */
  /* params.data_url_as_string: skip */
  params.post_data = post_body;
  params.can_load_local_resources = false;
  params.should_replace_current_entry = false;
  /* params.frame_name: skip */
  // TODO(clamy): See if user gesture should be propagated to this function.
  params.has_user_gesture = false;
  params.should_clear_history_list = false;
  params.started_from_context_menu = false;
  /* params.navigation_ui_data: skip */
  /* params.input_start: skip */
  params.was_activated = WasActivatedOption::kUnknown;

  std::unique_ptr<NavigationRequest> request =
      CreateNavigationRequestFromLoadParams(
          render_frame_host->frame_tree_node(), params, override_user_agent,
          should_replace_current_entry, false /* has_user_gesture */,
          ReloadType::NONE, *entry, frame_entry.get());

  if (!request)
    return;

  render_frame_host->frame_tree_node()->navigator()->Navigate(
      std::move(request), ReloadType::NONE, RestoreType::NONE);
}

void NavigationControllerImpl::ClearAllScreenshots() {
  screenshot_manager_->ClearAllScreenshots();
}

void NavigationControllerImpl::SetSessionStorageNamespace(
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  if (!session_storage_namespace)
    return;

  // We can't overwrite an existing SessionStorage without violating spec.
  // Attempts to do so may give a tab access to another tab's session storage
  // so die hard on an error.
  bool successful_insert = session_storage_namespace_map_.insert(
      make_pair(partition_id,
                static_cast<SessionStorageNamespaceImpl*>(
                    session_storage_namespace)))
          .second;
  CHECK(successful_insert) << "Cannot replace existing SessionStorageNamespace";
}

bool NavigationControllerImpl::IsUnmodifiedBlankTab() const {
  return IsInitialNavigation() &&
         !GetLastCommittedEntry() &&
         !delegate_->HasAccessedInitialDocument();
}

SessionStorageNamespace*
NavigationControllerImpl::GetSessionStorageNamespace(SiteInstance* instance) {
  std::string partition_id;
  if (instance) {
    // TODO(ajwong): When GetDefaultSessionStorageNamespace() goes away, remove
    // this if statement so |instance| must not be NULL.
    partition_id =
        GetContentClient()->browser()->GetStoragePartitionIdForSite(
            browser_context_, instance->GetSiteURL());
  }

  // TODO(ajwong): Should this use the |partition_id| directly rather than
  // re-lookup via |instance|?  http://crbug.com/142685
  StoragePartition* partition =
      BrowserContext::GetStoragePartition(browser_context_, instance);
  DOMStorageContextWrapper* context_wrapper =
      static_cast<DOMStorageContextWrapper*>(partition->GetDOMStorageContext());

  SessionStorageNamespaceMap::const_iterator it =
      session_storage_namespace_map_.find(partition_id);
  if (it != session_storage_namespace_map_.end()) {
    // Ensure that this namespace actually belongs to this partition.
    DCHECK(static_cast<SessionStorageNamespaceImpl*>(it->second.get())->
        IsFromContext(context_wrapper));
    return it->second.get();
  }

  // Create one if no one has accessed session storage for this partition yet.
  scoped_refptr<SessionStorageNamespaceImpl> session_storage_namespace =
      SessionStorageNamespaceImpl::Create(context_wrapper);
  SessionStorageNamespaceImpl* session_storage_namespace_ptr =
      session_storage_namespace.get();
  session_storage_namespace_map_[partition_id] =
      std::move(session_storage_namespace);

  return session_storage_namespace_ptr;
}

SessionStorageNamespace*
NavigationControllerImpl::GetDefaultSessionStorageNamespace() {
  // TODO(ajwong): Remove if statement in GetSessionStorageNamespace().
  return GetSessionStorageNamespace(nullptr);
}

const SessionStorageNamespaceMap&
NavigationControllerImpl::GetSessionStorageNamespaceMap() const {
  return session_storage_namespace_map_;
}

bool NavigationControllerImpl::NeedsReload() const {
  return needs_reload_;
}

void NavigationControllerImpl::SetNeedsReload() {
  needs_reload_ = true;

  if (last_committed_entry_index_ != -1) {
    entries_[last_committed_entry_index_]->SetTransitionType(
        ui::PAGE_TRANSITION_RELOAD);
  }
}

void NavigationControllerImpl::RemoveEntryAtIndexInternal(int index) {
  DCHECK(index < GetEntryCount());
  DCHECK(index != last_committed_entry_index_);

  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);
  if (last_committed_entry_index_ > index)
    last_committed_entry_index_--;
}

void NavigationControllerImpl::DiscardNonCommittedEntries() {
  bool transient = transient_entry_index_ != -1;
  DiscardNonCommittedEntriesInternal();

  // If there was a transient entry, invalidate everything so the new active
  // entry state is shown.
  if (transient) {
    delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
  }
}

NavigationEntryImpl* NavigationControllerImpl::GetPendingEntry() const {
  // If there is no pending_entry_, there should be no pending_entry_index_.
  DCHECK(pending_entry_ || pending_entry_index_ == -1);

  // If there is a pending_entry_index_, then pending_entry_ must be the entry
  // at that index.
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_ == GetEntryAtIndex(pending_entry_index_));

  return pending_entry_;
}

int NavigationControllerImpl::GetPendingEntryIndex() const {
  // The pending entry index must always be less than the number of entries.
  // If there are no entries, it must be exactly -1.
  DCHECK_LT(pending_entry_index_, GetEntryCount());
  DCHECK(GetEntryCount() != 0 || pending_entry_index_ == -1);
  return pending_entry_index_;
}

void NavigationControllerImpl::InsertOrReplaceEntry(
    std::unique_ptr<NavigationEntryImpl> entry,
    bool replace) {
  DCHECK(!ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // If the pending_entry_index_ is -1, the navigation was to a new page, and we
  // need to keep continuity with the pending entry, so copy the pending entry's
  // unique ID to the committed entry. If the pending_entry_index_ isn't -1,
  // then the renderer navigated on its own, independent of the pending entry,
  // so don't copy anything.
  if (pending_entry_ && pending_entry_index_ == -1)
    entry->set_unique_id(pending_entry_->GetUniqueID());

  DiscardNonCommittedEntriesInternal();

  int current_size = static_cast<int>(entries_.size());

  // When replacing, don't prune the forward history.
  if (replace && current_size > 0) {
    CopyReplacedNavigationEntryDataIfPreviouslyEmpty(
        *entries_[last_committed_entry_index_], entry.get());
    entries_[last_committed_entry_index_] = std::move(entry);
    return;
  }

  // We shouldn't see replace == true when there's no committed entries.
  DCHECK(!replace);

  if (current_size > 0) {
    // Prune any entries which are in front of the current entry.
    // last_committed_entry_index_ must be updated here since calls to
    // NotifyPrunedEntries() below may re-enter and we must make sure
    // last_committed_entry_index_ is not left in an invalid state.
    int num_pruned = 0;
    while (last_committed_entry_index_ < (current_size - 1)) {
      num_pruned++;
      entries_.pop_back();
      current_size--;
    }
    if (num_pruned > 0)  // Only notify if we did prune something.
      NotifyPrunedEntries(this, false, num_pruned);
  }

  PruneOldestEntryIfFull();

  entries_.push_back(std::move(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
}

void NavigationControllerImpl::PruneOldestEntryIfFull() {
  if (entries_.size() >= max_entry_count()) {
    DCHECK_EQ(max_entry_count(), entries_.size());
    DCHECK_GT(last_committed_entry_index_, 0);
    RemoveEntryAtIndex(0);
    NotifyPrunedEntries(this, true, 1);
  }
}

void NavigationControllerImpl::NavigateToExistingPendingEntry(
    ReloadType reload_type) {
  DCHECK(pending_entry_);
  DCHECK(IsInitialNavigation() || pending_entry_index_ != -1);
  DCHECK(!IsRendererDebugURL(pending_entry_->GetURL()));
  needs_reload_ = false;

  // If we were navigating to a slow-to-commit page, and the user performs
  // a session history navigation to the last committed page, RenderViewHost
  // will force the throbber to start, but WebKit will essentially ignore the
  // navigation, and won't send a message to stop the throbber. To prevent this
  // from happening, we drop the navigation here and stop the slow-to-commit
  // page from loading (which would normally happen during the navigation).
  if (pending_entry_index_ == last_committed_entry_index_ &&
      pending_entry_->restore_type() == RestoreType::NONE &&
      pending_entry_->GetTransitionType() & ui::PAGE_TRANSITION_FORWARD_BACK) {
    delegate_->Stop();

    // If an interstitial page is showing, we want to close it to get back to
    // what was showing before.
    //
    // There are two ways to get the interstitial page given a WebContents.
    // Because WebContents::GetInterstitialPage() returns null between the
    // interstitial's Show() method being called and the interstitial becoming
    // visible, while InterstitialPage::GetInterstitialPage() returns the
    // interstitial during that time, use the latter.
    InterstitialPage* interstitial =
        InterstitialPage::GetInterstitialPage(GetWebContents());
    if (interstitial)
      interstitial->DontProceed();

    DiscardNonCommittedEntries();
    return;
  }

  FrameTreeNode* root = delegate_->GetFrameTree()->root();

  // Compare FrameNavigationEntries to see which frames in the tree need to be
  // navigated.
  std::vector<std::unique_ptr<NavigationRequest>> same_document_loads;
  std::vector<std::unique_ptr<NavigationRequest>> different_document_loads;
  if (GetLastCommittedEntry()) {
    FindFramesToNavigate(root, reload_type, &same_document_loads,
                         &different_document_loads);
  }

  if (same_document_loads.empty() && different_document_loads.empty()) {
    // If we don't have any frames to navigate at this point, either
    // (1) there is no previous history entry to compare against, or
    // (2) we were unable to match any frames by name. In the first case,
    // doing a different document navigation to the root item is the only valid
    // thing to do. In the second case, we should have been able to find a
    // frame to navigate based on names if this were a same document
    // navigation, so we can safely assume this is the different document case.
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            root, *pending_entry_, pending_entry_->GetFrameEntry(root),
            reload_type, false /* is_same_document_history_load */,
            false /* is_history_navigation_in_new_child */);
    if (!navigation_request) {
      // This navigation cannot start (e.g. the URL is invalid), delete the
      // pending NavigationEntry.
      DiscardPendingEntry(false);
      return;
    }
    different_document_loads.push_back(std::move(navigation_request));
  }

  // If an interstitial page is showing, the previous renderer is blocked and
  // cannot make new requests.  Unblock (and disable) it to allow this
  // navigation to succeed.  The interstitial will stay visible until the
  // resulting DidNavigate.
  // TODO(clamy): See if this can be removed now that PlzNavigate has shipped.
  // See https://crbug.com/849250
  if (delegate_->GetInterstitialPage()) {
    static_cast<InterstitialPageImpl*>(delegate_->GetInterstitialPage())
        ->CancelForNavigation();
  }

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;

  // Send all the same document frame loads before the different document loads.
  for (auto& item : same_document_loads) {
    FrameTreeNode* frame = item->frame_tree_node();
    frame->navigator()->Navigate(std::move(item), reload_type,
                                 pending_entry_->restore_type());
  }
  for (auto& item : different_document_loads) {
    FrameTreeNode* frame = item->frame_tree_node();
    frame->navigator()->Navigate(std::move(item), reload_type,
                                 pending_entry_->restore_type());
  }

  in_navigate_to_pending_entry_ = false;
}

void NavigationControllerImpl::FindFramesToNavigate(
    FrameTreeNode* frame,
    ReloadType reload_type,
    std::vector<std::unique_ptr<NavigationRequest>>* same_document_loads,
    std::vector<std::unique_ptr<NavigationRequest>>* different_document_loads) {
  DCHECK(pending_entry_);
  DCHECK_GE(last_committed_entry_index_, 0);
  FrameNavigationEntry* new_item = pending_entry_->GetFrameEntry(frame);
  // TODO(creis): Store the last committed FrameNavigationEntry to use here,
  // rather than assuming the NavigationEntry has up to date info on subframes.
  FrameNavigationEntry* old_item =
      GetLastCommittedEntry()->GetFrameEntry(frame);
  if (!new_item)
    return;

  // Schedule a load in this frame if the new item isn't for the same item
  // sequence number in the same SiteInstance. Newly restored items may not have
  // a SiteInstance yet, in which case it will be assigned on first commit.
  if (!old_item ||
      new_item->item_sequence_number() != old_item->item_sequence_number() ||
      (new_item->site_instance() != nullptr &&
       new_item->site_instance() != old_item->site_instance())) {
    // Same document loads happen if the previous item has the same document
    // sequence number.  Note that we should treat them as different document if
    // the destination RenderFrameHost (which is necessarily the current
    // RenderFrameHost for same document navigations) doesn't have a last
    // committed page.  This case can happen for Ctrl+Back or after a renderer
    // crash.
    if (old_item &&
        new_item->document_sequence_number() ==
            old_item->document_sequence_number() &&
        !frame->current_frame_host()->GetLastCommittedURL().is_empty()) {
      std::unique_ptr<NavigationRequest> navigation_request =
          CreateNavigationRequestFromEntry(
              frame, *pending_entry_, new_item, reload_type,
              true /* is_same_document_history_load */,
              false /* is_history_navigation_in_new_child */);
      if (navigation_request) {
        // Only add the request if was properly created. It's possible for the
        // creation to fail in certain cases, e.g. when the URL is invalid.
        same_document_loads->push_back(std::move(navigation_request));
      }

      // TODO(avi, creis): This is a bug; we should not return here. Rather, we
      // should continue on and navigate all child frames which have also
      // changed. This bug is the cause of <https://crbug.com/542299>, which is
      // a NC_IN_PAGE_NAVIGATION renderer kill.
      //
      // However, this bug is a bandaid over a deeper and worse problem. Doing a
      // pushState immediately after loading a subframe is a race, one that no
      // web page author expects. If we fix this bug, many large websites break.
      // For example, see <https://crbug.com/598043> and the spec discussion at
      // <https://github.com/whatwg/html/issues/1191>.
      //
      // For now, we accept this bug, and hope to resolve the race in a
      // different way that will one day allow us to fix this.
      return;
    } else {
      std::unique_ptr<NavigationRequest> navigation_request =
          CreateNavigationRequestFromEntry(
              frame, *pending_entry_, new_item, reload_type,
              false /* is_same_document_history_load */,
              false /* is_history_navigation_in_new_child */);
      if (navigation_request) {
        // Only add the request if was properly created. It's possible for the
        // creation to fail in certain cases, e.g. when the URL is invalid.
        different_document_loads->push_back(std::move(navigation_request));
      }
      // For a different document, the subframes will be destroyed, so there's
      // no need to consider them.
      return;
    }
  }

  for (size_t i = 0; i < frame->child_count(); i++) {
    FindFramesToNavigate(frame->child_at(i), reload_type, same_document_loads,
                         different_document_loads);
  }
}

void NavigationControllerImpl::NavigateWithoutEntry(
    const LoadURLParams& params) {
  // Find the appropriate FrameTreeNode.
  FrameTreeNode* node = nullptr;
  if (params.frame_tree_node_id != RenderFrameHost::kNoFrameTreeNodeId ||
      !params.frame_name.empty()) {
    node = params.frame_tree_node_id != RenderFrameHost::kNoFrameTreeNodeId
               ? delegate_->GetFrameTree()->FindByID(params.frame_tree_node_id)
               : delegate_->GetFrameTree()->FindByName(params.frame_name);
  }

  // If no FrameTreeNode was specified, navigate the main frame.
  if (!node)
    node = delegate_->GetFrameTree()->root();

  // Compute overrides to the LoadURLParams for |override_user_agent|,
  // |should_replace_current_entry| and |has_user_gesture| that will be used
  // both in the creation of the NavigationEntry and the NavigationRequest.
  // Ideally, the LoadURLParams themselves would be updated, but since they are
  // passed as a const reference, this is not possible.
  // TODO(clamy): When we only create a NavigationRequest, move this to
  // CreateNavigationRequestFromLoadURLParams.
  bool override_user_agent = ShouldOverrideUserAgent(params.override_user_agent,
                                                     GetLastCommittedEntry());

  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  bool should_replace_current_entry =
      params.should_replace_current_entry && entries_.size();

  // Always propagate `has_user_gesture` on Android but only when the request
  // was originated by the renderer on other platforms. This is merely for
  // backward compatibility as browser process user gestures create confusion in
  // many tests.
  bool has_user_gesture = false;
#if defined(OS_ANDROID)
  has_user_gesture = params.has_user_gesture;
#else
  if (params.is_renderer_initiated)
    has_user_gesture = params.has_user_gesture;
#endif

  // Javascript URLs should not create NavigationEntries. All other navigations
  // do, including navigations to chrome renderer debug URLs.
  std::unique_ptr<NavigationEntryImpl> entry;
  if (!params.url.SchemeIs(url::kJavaScriptScheme)) {
    entry = CreateNavigationEntryFromLoadParams(
        node, params, override_user_agent, should_replace_current_entry,
        has_user_gesture);
    DiscardPendingEntry(false);
    SetPendingEntry(std::move(entry));
  }

  // Renderer-debug URLs are sent to the renderer process immediately for
  // processing and don't need to create a NavigationRequest.
  // Note: this includes navigations to JavaScript URLs, which are considered
  // renderer-debug URLs.
  // Note: we intentionally leave the pending entry in place for renderer debug
  // URLs, unlike the cases below where we clear it if the navigation doesn't
  // proceed.
  if (IsRendererDebugURL(params.url)) {
    HandleRendererDebugURL(node, params.url);
    return;
  }

  // Convert navigations to the current URL to a reload.
  // TODO(clamy): We should be using FrameTreeNode::IsMainFrame here instead of
  // relying on the frame tree node id from LoadURLParams. Unfortunately,
  // DevTools sometimes issues navigations to main frames that they do not
  // expect to see treated as reload, and it only works because they pass a
  // FrameTreeNode id in their LoadURLParams. Change this once they no longer do
  // that. See https://crbug.com/850926.
  ReloadType reload_type = ReloadType::NONE;
  if (ShouldTreatNavigationAsReload(
          params.url, pending_entry_->GetVirtualURL(),
          params.base_url_for_data_url, params.transition_type,
          params.frame_tree_node_id == RenderFrameHost::kNoFrameTreeNodeId,
          params.load_type ==
              NavigationController::LOAD_TYPE_HTTP_POST /* is_post */,
          false /* is_reload */, false /* is_navigation_to_existing_entry */,
          transient_entry_index_ != -1 /* has_interstitial */,
          GetLastCommittedEntry())) {
    reload_type = ReloadType::NORMAL;
  }

  // navigation_ui_data should only be present for main frame navigations.
  DCHECK(node->IsMainFrame() || !params.navigation_ui_data);

  DCHECK(pending_entry_);
  std::unique_ptr<NavigationRequest> request =
      CreateNavigationRequestFromLoadParams(
          node, params, override_user_agent, should_replace_current_entry,
          has_user_gesture, reload_type, *pending_entry_,
          pending_entry_->GetFrameEntry(node));

  // If the navigation couldn't start, return immediately and discard the
  // pending NavigationEntry.
  if (!request) {
    DiscardPendingEntry(false);
    return;
  }

#if DCHECK_IS_ON()
  // Safety check that NavigationRequest and NavigationEntry match.
  ValidateRequestMatchesEntry(request.get(), pending_entry_);
#endif

  // If an interstitial page is showing, the previous renderer is blocked and
  // cannot make new requests.  Unblock (and disable) it to allow this
  // navigation to succeed.  The interstitial will stay visible until the
  // resulting DidNavigate.
  if (delegate_->GetInterstitialPage()) {
    static_cast<InterstitialPageImpl*>(delegate_->GetInterstitialPage())
        ->CancelForNavigation();
  }

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;

  node->navigator()->Navigate(std::move(request), reload_type,
                              RestoreType::NONE);

  in_navigate_to_pending_entry_ = false;
}

void NavigationControllerImpl::HandleRendererDebugURL(
    FrameTreeNode* frame_tree_node,
    const GURL& url) {
  if (!frame_tree_node->current_frame_host()->IsRenderFrameLive()) {
    // Any renderer-side debug URLs or javascript: URLs should be ignored if
    // the renderer process is not live, unless it is the initial navigation
    // of the tab.
    if (!IsInitialNavigation()) {
      DiscardNonCommittedEntries();
      return;
    }
    frame_tree_node->render_manager()->InitializeRenderFrameIfNecessary(
        frame_tree_node->current_frame_host());
  }
  frame_tree_node->current_frame_host()->HandleRendererDebugURL(url);
}

std::unique_ptr<NavigationEntryImpl>
NavigationControllerImpl::CreateNavigationEntryFromLoadParams(
    FrameTreeNode* node,
    const LoadURLParams& params,
    bool override_user_agent,
    bool should_replace_current_entry,
    bool has_user_gesture) {
  // Browser initiated navigations might not have a blob_url_loader_factory set
  // in params even if the navigation is to a blob URL. If that happens, lookup
  // the correct url loader factory to use here.
  auto blob_url_loader_factory = params.blob_url_loader_factory;
  if (!blob_url_loader_factory && blink::BlobUtils::MojoBlobURLsEnabled() &&
      params.url.SchemeIsBlob()) {
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        GetBrowserContext(), params.url);
  }

  std::unique_ptr<NavigationEntryImpl> entry;

  // For subframes, create a pending entry with a corresponding frame entry.
  if (!node->IsMainFrame()) {
    DCHECK(GetLastCommittedEntry());

    // Create an identical NavigationEntry with a new FrameNavigationEntry for
    // the target subframe.
    entry = GetLastCommittedEntry()->Clone();
    entry->AddOrUpdateFrameEntry(
        node, -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()),
        params.url, params.referrer, params.redirect_chain, PageState(), "GET",
        -1, blob_url_loader_factory);
  } else {
    // Otherwise, create a pending entry for the main frame.

    // extra_headers in params are \n separated; navigation entries want \r\n.
    std::string extra_headers_crlf;
    base::ReplaceChars(params.extra_headers, "\n", "\r\n", &extra_headers_crlf);
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        params.url, params.referrer, params.transition_type,
        params.is_renderer_initiated, extra_headers_crlf, browser_context_,
        blob_url_loader_factory));
    entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()));
    entry->SetRedirectChain(params.redirect_chain);
  }

  // Set the FTN ID (only used in non-site-per-process, for tests).
  entry->set_frame_tree_node_id(node->frame_tree_node_id());
  entry->set_should_replace_entry(should_replace_current_entry);
  entry->set_should_clear_history_list(params.should_clear_history_list);
  entry->SetIsOverridingUserAgent(override_user_agent);
  entry->set_has_user_gesture(has_user_gesture);

  switch (params.load_type) {
    case LOAD_TYPE_DEFAULT:
      break;
    case LOAD_TYPE_HTTP_POST:
      entry->SetHasPostData(true);
      entry->SetPostData(params.post_data);
      break;
    case LOAD_TYPE_DATA:
      entry->SetBaseURLForDataURL(params.base_url_for_data_url);
      entry->SetVirtualURL(params.virtual_url_for_data_url);
#if defined(OS_ANDROID)
      entry->SetDataURLAsString(params.data_url_as_string);
#endif
      entry->SetCanLoadLocalResources(params.can_load_local_resources);
      break;
    default:
      NOTREACHED();
      break;
  }

  // TODO(clamy): NavigationEntry is meant for information that will be kept
  // after the navigation ended and therefore is not appropriate for
  // started_from_context_menu. Move started_from_context_menu to
  // NavigationUIData.
  entry->set_started_from_context_menu(params.started_from_context_menu);

  return entry;
}

std::unique_ptr<NavigationRequest>
NavigationControllerImpl::CreateNavigationRequestFromLoadParams(
    FrameTreeNode* node,
    const LoadURLParams& params,
    bool override_user_agent,
    bool should_replace_current_entry,
    bool has_user_gesture,
    ReloadType reload_type,
    const NavigationEntryImpl& entry,
    FrameNavigationEntry* frame_entry) {
  DCHECK_EQ(-1, GetIndexOfEntry(&entry));
  GURL url_to_load;
  GURL virtual_url;
  // For main frames, rewrite the URL if necessary and compute the virtual URL
  // that should be shown in the address bar.
  if (node->IsMainFrame()) {
    bool reverse_on_redirect = false;
    RewriteUrlForNavigation(params.url, browser_context_, &url_to_load,
                            &virtual_url, &reverse_on_redirect);

    // For DATA loads, override the virtual URL.
    if (params.load_type == LOAD_TYPE_DATA)
      virtual_url = params.virtual_url_for_data_url;

    if (virtual_url.is_empty())
      virtual_url = url_to_load;

    // TODO(clamy): In order to remove the pending NavigationEntry,
    // |virtual_url| and |reverse_on_redirect| should be stored in the
    // NavigationRequest.
  } else {
    url_to_load = params.url;
    virtual_url = params.url;
  }

  CHECK(!node->IsMainFrame() || virtual_url == entry.GetVirtualURL());
  CHECK_EQ(url_to_load, frame_entry->url());

  if (!IsValidURLForNavigation(node->IsMainFrame(), virtual_url, url_to_load))
    return nullptr;

  // Determine if Previews should be used for the navigation.
  PreviewsState previews_state = PREVIEWS_UNSPECIFIED;
  if (!node->IsMainFrame()) {
    // For subframes, use the state of the top-level frame.
    previews_state = node->frame_tree()
                         ->root()
                         ->current_frame_host()
                         ->last_navigation_previews_state();
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). If the navigation must wait on the current
  // RenderFrameHost to execute its BeforeUnload event, the navigation start
  // will be updated when the BeforeUnload ack is received.
  base::TimeTicks navigation_start = base::TimeTicks::Now();

  FrameMsg_Navigate_Type::Value navigation_type =
      GetNavigationType(node->current_url(),  // old_url
                        url_to_load,          // new_url
                        reload_type,          // reload_type
                        entry,                // entry
                        *frame_entry,         // frame_entry
                        false);               // is_same_document_history_load

  // Create the NavigationParams based on |params|.

  bool is_view_source_mode = virtual_url.SchemeIs(kViewSourceScheme);
  const GURL& history_url_for_data_url =
      params.base_url_for_data_url.is_empty() ? GURL() : virtual_url;
  CommonNavigationParams common_params(
      url_to_load, params.referrer, params.transition_type, navigation_type,
      !is_view_source_mode, should_replace_current_entry,
      params.base_url_for_data_url, history_url_for_data_url, previews_state,
      navigation_start,
      params.load_type == LOAD_TYPE_HTTP_POST ? "POST" : "GET",
      params.post_data, base::Optional<SourceLocation>(),
      params.started_from_context_menu, has_user_gesture, InitiatorCSPInfo(),
      params.input_start);

  RequestNavigationParams request_params(
      override_user_agent, params.redirect_chain, common_params.url,
      common_params.method, params.can_load_local_resources,
      frame_entry->page_state(), entry.GetUniqueID(),
      false /* is_history_navigation_in_new_child */,
      entry.GetSubframeUniqueNames(node), true /* intended_as_new_entry */,
      -1 /* pending_history_list_offset */,
      params.should_clear_history_list ? -1 : GetLastCommittedEntryIndex(),
      params.should_clear_history_list ? 0 : GetEntryCount(),
      is_view_source_mode, params.should_clear_history_list);
#if defined(OS_ANDROID)
  if (ValidateDataURLAsString(params.data_url_as_string)) {
    request_params.data_url_as_string = params.data_url_as_string->data();
  }
#endif

  request_params.was_activated = params.was_activated;

  // A form submission may happen here if the navigation is a renderer-initiated
  // form submission that took the OpenURL path.
  scoped_refptr<network::ResourceRequestBody> request_body = params.post_data;

  // extra_headers in params are \n separated; NavigationRequests want \r\n.
  std::string extra_headers_crlf;
  base::ReplaceChars(params.extra_headers, "\n", "\r\n", &extra_headers_crlf);
  return NavigationRequest::CreateBrowserInitiated(
      node, common_params, request_params, !params.is_renderer_initiated,
      extra_headers_crlf, *frame_entry, entry, request_body,
      params.navigation_ui_data ? params.navigation_ui_data->Clone() : nullptr);
}

std::unique_ptr<NavigationRequest>
NavigationControllerImpl::CreateNavigationRequestFromEntry(
    FrameTreeNode* frame_tree_node,
    const NavigationEntryImpl& entry,
    FrameNavigationEntry* frame_entry,
    ReloadType reload_type,
    bool is_same_document_history_load,
    bool is_history_navigation_in_new_child) {
  GURL dest_url = frame_entry->url();
  Referrer dest_referrer = frame_entry->referrer();
  if (reload_type == ReloadType::ORIGINAL_REQUEST_URL &&
      entry.GetOriginalRequestURL().is_valid() && !entry.GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit as signaled by the
    // ReloadType, if it's valid and if a POST wasn't involved; the latter
    // case avoids issues with sending data to the wrong page.
    dest_url = entry.GetOriginalRequestURL();
    dest_referrer = Referrer();
  }

  if (!IsValidURLForNavigation(frame_tree_node->IsMainFrame(),
                               entry.GetVirtualURL(), dest_url)) {
    return nullptr;
  }

  // Determine if Previews should be used for the navigation.
  PreviewsState previews_state = PREVIEWS_UNSPECIFIED;
  if (!frame_tree_node->IsMainFrame()) {
    // For subframes, use the state of the top-level frame.
    previews_state = frame_tree_node->frame_tree()
                         ->root()
                         ->current_frame_host()
                         ->last_navigation_previews_state();
  }

  // This will be used to set the Navigation Timing API navigationStart
  // parameter for browser navigations in new tabs (intents, tabs opened through
  // "Open link in new tab"). If the navigation must wait on the current
  // RenderFrameHost to execute its BeforeUnload event, the navigation start
  // will be updated when the BeforeUnload ack is received.
  base::TimeTicks navigation_start = base::TimeTicks::Now();

  FrameMsg_Navigate_Type::Value navigation_type = GetNavigationType(
      frame_tree_node->current_url(),  // old_url
      dest_url,                        // new_url
      reload_type,                     // reload_type
      entry,                           // entry
      *frame_entry,                    // frame_entry
      is_same_document_history_load);  // is_same_document_history_load

  // A form submission may happen here if the navigation is a
  // back/forward/reload navigation that does a form resubmission.
  scoped_refptr<network::ResourceRequestBody> request_body;
  std::string post_content_type;
  if (frame_entry->method() == "POST") {
    request_body = frame_entry->GetPostData(&post_content_type);
    // Might have a LF at end.
    post_content_type =
        base::TrimWhitespaceASCII(post_content_type, base::TRIM_ALL)
            .as_string();
  }

  // Create the NavigationParams based on |entry| and |frame_entry|.
  CommonNavigationParams common_params = entry.ConstructCommonNavigationParams(
      *frame_entry, request_body, dest_url, dest_referrer, navigation_type,
      previews_state, navigation_start, base::TimeTicks() /* input_start */);

  // TODO(clamy): |intended_as_new_entry| below should always be false once
  // Reload no longer leads to this being called for a pending NavigationEntry
  // of index -1.
  RequestNavigationParams request_params =
      entry.ConstructRequestNavigationParams(
          *frame_entry, common_params.url, common_params.method,
          is_history_navigation_in_new_child,
          entry.GetSubframeUniqueNames(frame_tree_node),
          GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          GetIndexOfEntry(&entry), GetLastCommittedEntryIndex(),
          GetEntryCount());
  request_params.post_content_type = post_content_type;

  return NavigationRequest::CreateBrowserInitiated(
      frame_tree_node, common_params, request_params,
      !entry.is_renderer_initiated(), entry.extra_headers(), *frame_entry,
      entry, request_body, nullptr /* navigation_ui_data */);
}

void NavigationControllerImpl::NotifyNavigationEntryCommitted(
    LoadCommittedDetails* details) {
  details->entry = GetLastCommittedEntry();

  // We need to notify the ssl_manager_ before the web_contents_ so the
  // location bar will have up-to-date information about the security style
  // when it wants to draw.  See http://crbug.com/11157
  ssl_manager_.DidCommitProvisionalLoad(*details);

  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
  delegate_->NotifyNavigationEntryCommitted(*details);

  // TODO(avi): Remove. http://crbug.com/170921
  NotificationDetails notification_details =
      Details<LoadCommittedDetails>(details);
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_COMMITTED,
      Source<NavigationController>(this),
      notification_details);
}

// static
size_t NavigationControllerImpl::max_entry_count() {
  if (max_entry_count_for_testing_ != kMaxEntryCountForTestingNotSet)
     return max_entry_count_for_testing_;
  return kMaxSessionHistoryEntries;
}

void NavigationControllerImpl::SetActive(bool is_active) {
  if (is_active && needs_reload_)
    LoadIfNecessary();
}

void NavigationControllerImpl::LoadIfNecessary() {
  if (!needs_reload_)
    return;

  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  if (pending_entry_) {
    NavigateToExistingPendingEntry(ReloadType::NONE);
  } else if (last_committed_entry_index_ != -1) {
    pending_entry_ = entries_[last_committed_entry_index_].get();
    pending_entry_index_ = last_committed_entry_index_;
    NavigateToExistingPendingEntry(ReloadType::NONE);
  } else {
    // If there is something to reload, the successful reload will clear the
    // |needs_reload_| flag. Otherwise, just do it here.
    needs_reload_ = false;
  }
}

void NavigationControllerImpl::NotifyEntryChanged(
    const NavigationEntry* entry) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = GetIndexOfEntry(
      NavigationEntryImpl::FromNavigationEntry(entry));
  delegate_->NotifyNavigationEntryChanged(det);
}

void NavigationControllerImpl::FinishRestore(int selected_index,
                                             RestoreType type) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_, type);

  last_committed_entry_index_ = selected_index;
}

void NavigationControllerImpl::DiscardNonCommittedEntriesInternal() {
  DiscardPendingEntry(false);
  DiscardTransientEntry();
}

void NavigationControllerImpl::DiscardTransientEntry() {
  if (transient_entry_index_ == -1)
    return;
  entries_.erase(entries_.begin() + transient_entry_index_);
  if (last_committed_entry_index_ > transient_entry_index_)
    last_committed_entry_index_--;
  if (pending_entry_index_ > transient_entry_index_)
    pending_entry_index_--;
  transient_entry_index_ = -1;
}

int NavigationControllerImpl::GetEntryIndexWithUniqueID(
    int nav_entry_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if (entries_[i]->GetUniqueID() == nav_entry_id)
      return i;
  }
  return -1;
}

NavigationEntryImpl* NavigationControllerImpl::GetTransientEntry() const {
  if (transient_entry_index_ == -1)
    return nullptr;
  return entries_[transient_entry_index_].get();
}

void NavigationControllerImpl::SetTransientEntry(
    std::unique_ptr<NavigationEntry> entry) {
  // Discard any current transient entry, we can only have one at a time.
  int index = 0;
  if (last_committed_entry_index_ != -1)
    index = last_committed_entry_index_ + 1;
  DiscardTransientEntry();
  entries_.insert(entries_.begin() + index,
                  NavigationEntryImpl::FromNavigationEntry(std::move(entry)));
  if (pending_entry_index_ >= index)
    pending_entry_index_++;
  transient_entry_index_ = index;
  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
}

void NavigationControllerImpl::InsertEntriesFrom(
    const NavigationControllerImpl& source,
    int max_index) {
  DCHECK_LE(max_index, source.GetEntryCount());
  size_t insert_index = 0;
  for (int i = 0; i < max_index; i++) {
    // When cloning a tab, copy all entries except interstitial pages.
    if (source.entries_[i]->GetPageType() != PAGE_TYPE_INTERSTITIAL) {
      // TODO(creis): Once we start sharing FrameNavigationEntries between
      // NavigationEntries, it will not be safe to share them with another tab.
      // Must have a version of Clone that recreates them.
      entries_.insert(entries_.begin() + insert_index++,
                      source.entries_[i]->Clone());
    }
  }
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_ == GetEntryAtIndex(pending_entry_index_));
}

void NavigationControllerImpl::SetGetTimestampCallbackForTest(
    const base::Callback<base::Time()>& get_timestamp_callback) {
  get_timestamp_callback_ = get_timestamp_callback;
}

}  // namespace content
