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

#include "content/browser/renderer_host/navigation_controller_impl.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
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
#include "content/browser/renderer_host/debug_urls.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_delegate.h"
#include "content/browser/site_instance_impl.h"
#include "content/browser/web_package/web_bundle_navigation_info.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/trace_utils.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/replaced_navigation_entry_data.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_features.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "media/base/mime_util.h"
#include "net/base/escape.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
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
                         int index,
                         int count) {
  PrunedDetails details;
  details.index = index;
  details.count = count;
  nav_controller->delegate()->NotifyNavigationListPruned(details);
}

// Configure all the NavigationEntries in entries for restore. This resets
// the transition type to reload and makes sure the content state isn't empty.
void ConfigureEntriesForRestore(
    std::vector<std::unique_ptr<NavigationEntryImpl>>* entries,
    RestoreType type) {
  for (auto& entry : *entries) {
    // Use a transition type of reload so that we don't incorrectly increase
    // the typed count.
    entry->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);
    entry->set_restore_type(type);
  }
}

// Determines whether or not we should be carrying over a user agent override
// between two NavigationEntries.
bool ShouldKeepOverride(NavigationEntry* last_entry) {
  return last_entry && last_entry->GetIsOverridingUserAgent();
}

// Determines whether to override user agent for a navigation.
bool ShouldOverrideUserAgent(
    NavigationController::UserAgentOverrideOption override_user_agent,
    NavigationEntry* last_committed_entry) {
  switch (override_user_agent) {
    case NavigationController::UA_OVERRIDE_INHERIT:
      return ShouldKeepOverride(last_committed_entry);
    case NavigationController::UA_OVERRIDE_TRUE:
      return true;
    case NavigationController::UA_OVERRIDE_FALSE:
      return false;
  }
  NOTREACHED();
  return false;
}

// Returns true this navigation should be treated as a reload. For e.g.
// navigating to the last committed url via the address bar or clicking on a
// link which results in a navigation to the last committed or pending
// navigation, etc.
// |node| is the FrameTreeNode which is navigating. |url|, |virtual_url|,
// |base_url_for_data_url|, |transition_type| correspond to the new navigation
// (i.e. the pending NavigationEntry). |last_committed_entry| is the last
// navigation that committed.
bool ShouldTreatNavigationAsReload(FrameTreeNode* node,
                                   const GURL& url,
                                   const GURL& virtual_url,
                                   const GURL& base_url_for_data_url,
                                   ui::PageTransition transition_type,
                                   bool is_post,
                                   bool is_reload,
                                   bool is_navigation_to_existing_entry,
                                   NavigationEntryImpl* last_committed_entry) {
  if (is_reload || is_navigation_to_existing_entry)
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

  // Check that the URLs match.
  FrameNavigationEntry* frame_entry = last_committed_entry->GetFrameEntry(node);
  // If there's no frame entry then by definition the URLs don't match.
  if (!frame_entry)
    return false;

  if (url != frame_entry->url())
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
  if (frame_entry->get_has_post_data() || is_post)
    return false;

  return true;
}

bool DoesURLMatchOriginForNavigation(
    const GURL& url,
    const base::Optional<url::Origin>& origin) {
  // If there is no origin supplied there is nothing to match. This can happen
  // for navigations to a pending entry and therefore it should be allowed.
  if (!origin)
    return true;

  return origin->CanBeDerivedFrom(url);
}

base::Optional<url::Origin> GetCommittedOriginForFrameEntry(
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  // Error pages commit in an opaque origin, yet have the real URL that resulted
  // in an error as the |params.url|. Since successful reload of an error page
  // should commit in the correct origin, setting the opaque origin on the
  // FrameNavigationEntry will be incorrect.
  if (params.url_is_unreachable)
    return base::nullopt;

  return base::make_optional(params.origin);
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

  // Reject renderer debug URLs because they should have been handled before
  // we get to this point. This check handles renderer debug URLs
  // that are inside a view-source: URL (e.g. view-source:chrome://kill) and
  // provides defense-in-depth if a renderer debug URL manages to get here via
  // some other path. We want to reject the navigation here so it doesn't
  // violate assumptions in downstream code.
  if (IsRendererDebugURL(dest_url)) {
    LOG(WARNING) << "Refusing to load renderer debug URL: "
                 << dest_url.possibly_invalid_spec();
    return false;
  }

  return true;
}

// See replaced_navigation_entry_data.h for details: this information is meant
// to ensure |*output_entry| keeps track of its original URL (landing page in
// case of server redirects) as it gets replaced (e.g. history.replaceState()),
// without overwriting it later, for main frames.
void CopyReplacedNavigationEntryDataIfPreviouslyEmpty(
    NavigationEntryImpl* replaced_entry,
    NavigationEntryImpl* output_entry) {
  if (output_entry->GetReplacedEntryData().has_value())
    return;

  ReplacedNavigationEntryData data;
  data.first_committed_url = replaced_entry->GetURL();
  data.first_timestamp = replaced_entry->GetTimestamp();
  data.first_transition_type = replaced_entry->GetTransitionType();
  output_entry->set_replaced_entry_data(data);
}

mojom::NavigationType GetNavigationType(const GURL& old_url,
                                        const GURL& new_url,
                                        ReloadType reload_type,
                                        NavigationEntryImpl* entry,
                                        const FrameNavigationEntry& frame_entry,
                                        bool is_same_document_history_load) {
  // Reload navigations
  switch (reload_type) {
    case ReloadType::NORMAL:
      return mojom::NavigationType::RELOAD;
    case ReloadType::BYPASSING_CACHE:
      return mojom::NavigationType::RELOAD_BYPASSING_CACHE;
    case ReloadType::ORIGINAL_REQUEST_URL:
      return mojom::NavigationType::RELOAD_ORIGINAL_REQUEST_URL;
    case ReloadType::NONE:
      break;  // Fall through to rest of function.
  }

  if (entry->restore_type() == RestoreType::LAST_SESSION_EXITED_CLEANLY) {
    return entry->GetHasPostData() ? mojom::NavigationType::RESTORE_WITH_POST
                                   : mojom::NavigationType::RESTORE;
  }

  // History navigations.
  if (frame_entry.page_state().IsValid()) {
    return is_same_document_history_load
               ? mojom::NavigationType::HISTORY_SAME_DOCUMENT
               : mojom::NavigationType::HISTORY_DIFFERENT_DOCUMENT;
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
  bool is_same_doc = new_url.has_ref() && old_url.EqualsIgnoringRef(new_url) &&
                     frame_entry.method() == "GET";
  return is_same_doc ? mojom::NavigationType::SAME_DOCUMENT
                     : mojom::NavigationType::DIFFERENT_DOCUMENT;
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
  DCHECK_EQ(request->commit_params().should_clear_history_list,
            entry->should_clear_history_list());
  DCHECK_EQ(request->common_params().has_user_gesture,
            entry->has_user_gesture());
  DCHECK_EQ(request->common_params().base_url_for_data_url,
            entry->GetBaseURLForDataURL());
  DCHECK_EQ(request->commit_params().can_load_local_resources,
            entry->GetCanLoadLocalResources());
  DCHECK_EQ(request->common_params().started_from_context_menu,
            entry->has_started_from_context_menu());

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(request->frame_tree_node());
  if (!frame_entry) {
    NOTREACHED();
    return;
  }

  DCHECK_EQ(request->common_params().method, frame_entry->method());

  size_t redirect_size = request->commit_params().redirects.size();
  if (redirect_size == frame_entry->redirect_chain().size()) {
    for (size_t i = 0; i < redirect_size; ++i) {
      DCHECK_EQ(request->commit_params().redirects[i],
                frame_entry->redirect_chain()[i]);
    }
  } else {
    NOTREACHED();
  }
}
#endif  // DCHECK_IS_ON()

// Returns whether the session history NavigationRequests in |navigations|
// would stay within the subtree of the sandboxed iframe in
// |sandbox_frame_tree_node_id|.
bool DoesSandboxNavigationStayWithinSubtree(
    int sandbox_frame_tree_node_id,
    const std::vector<std::unique_ptr<NavigationRequest>>& navigations) {
  for (auto& item : navigations) {
    bool within_subtree = false;
    // Check whether this NavigationRequest affects a frame within the
    // sandboxed frame's subtree by walking up the tree looking for the
    // sandboxed frame.
    for (auto* frame = item->frame_tree_node(); frame;
         frame = FrameTreeNode::From(frame->parent())) {
      if (frame->frame_tree_node_id() == sandbox_frame_tree_node_id) {
        within_subtree = true;
        break;
      }
    }
    if (!within_subtree)
      return false;
  }
  return true;
}

}  // namespace

// NavigationControllerImpl::PendingEntryRef------------------------------------

NavigationControllerImpl::PendingEntryRef::PendingEntryRef(
    base::WeakPtr<NavigationControllerImpl> controller)
    : controller_(controller) {}

NavigationControllerImpl::PendingEntryRef::~PendingEntryRef() {
  if (!controller_)  // Can be null with interstitials.
    return;

  controller_->PendingEntryRefDeleted(this);
}

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
    Referrer referrer,
    base::Optional<url::Origin> initiator_origin,
    ui::PageTransition transition,
    bool is_renderer_initiated,
    const std::string& extra_headers,
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory) {
  return NavigationControllerImpl::CreateNavigationEntry(
      url, referrer, std::move(initiator_origin),
      nullptr /* source_site_instance */, transition, is_renderer_initiated,
      extra_headers, browser_context, std::move(blob_url_loader_factory),
      false /* should_replace_entry */, nullptr /* web_contents */);
}

// static
std::unique_ptr<NavigationEntryImpl>
NavigationControllerImpl::CreateNavigationEntry(
    const GURL& url,
    Referrer referrer,
    base::Optional<url::Origin> initiator_origin,
    SiteInstance* source_site_instance,
    ui::PageTransition transition,
    bool is_renderer_initiated,
    const std::string& extra_headers,
    BrowserContext* browser_context,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    bool should_replace_entry,
    WebContents* web_contents) {
  GURL url_to_load;
  GURL virtual_url;
  bool reverse_on_redirect = false;
  RewriteUrlForNavigation(url, browser_context, &url_to_load, &virtual_url,
                          &reverse_on_redirect);

  // Let the NTP override the navigation params and pretend that this is a
  // browser-initiated, bookmark-like navigation.
  GetContentClient()->browser()->OverrideNavigationParams(
      web_contents, source_site_instance, &transition, &is_renderer_initiated,
      &referrer, &initiator_origin);

  auto entry = std::make_unique<NavigationEntryImpl>(
      nullptr,  // The site instance for tabs is sent on navigation
                // (WebContents::GetSiteInstance).
      url_to_load, referrer, initiator_origin, base::string16(), transition,
      is_renderer_initiated, blob_url_loader_factory);
  entry->SetVirtualURL(virtual_url);
  entry->set_user_typed_url(virtual_url);
  entry->set_update_virtual_url_with_url(reverse_on_redirect);
  entry->set_extra_headers(extra_headers);
  entry->set_should_replace_entry(should_replace_entry);
  return entry;
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

NavigationControllerImpl::ScopedShowRepostDialogForTesting::
    ScopedShowRepostDialogForTesting()
    : was_disallowed_(g_check_for_repost) {
  g_check_for_repost = true;
}

NavigationControllerImpl::ScopedShowRepostDialogForTesting::
    ~ScopedShowRepostDialogForTesting() {
  g_check_for_repost = was_disallowed_;
}

NavigationControllerImpl::NavigationControllerImpl(
    NavigationControllerDelegate* delegate,
    BrowserContext* browser_context)
    : browser_context_(browser_context),
      delegate_(delegate),
      ssl_manager_(this),
      get_timestamp_callback_(base::BindRepeating(&base::Time::Now)) {
  DCHECK(browser_context_);
}

NavigationControllerImpl::~NavigationControllerImpl() {
  // The NavigationControllerImpl might be called inside its delegate
  // destructor. Calling it is not valid anymore.
  delegate_ = nullptr;
  DiscardNonCommittedEntries();
}

WebContents* NavigationControllerImpl::GetWebContents() {
  return delegate_->GetWebContents();
}

BrowserContext* NavigationControllerImpl::GetBrowserContext() {
  return browser_context_;
}

void NavigationControllerImpl::Restore(
    int selected_navigation,
    RestoreType type,
    std::vector<std::unique_ptr<NavigationEntry>>* entries) {
  // Verify that this controller is unused and that the input is valid.
  DCHECK_EQ(0, GetEntryCount());
  DCHECK(!GetPendingEntry());
  DCHECK(selected_navigation >= 0 &&
         selected_navigation < static_cast<int>(entries->size()));
  DCHECK_EQ(-1, pending_entry_index_);

  needs_reload_ = true;
  needs_reload_type_ = NeedsReloadType::kRestoreSession;
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
  NavigationEntryImpl* entry = nullptr;
  int current_index = -1;

  if (entry_replaced_by_post_commit_error_) {
    // If there is an entry that was replaced by a currently active post-commit
    // error navigation, this can't be the initial navigation.
    DCHECK(!IsInitialNavigation());
    // If the current entry is a post commit error, we reload the entry it
    // replaced instead. We leave the error entry in place until a commit
    // replaces it, but the pending entry points to the original entry in the
    // meantime. Note that NavigateToExistingPendingEntry is able to handle the
    // case that pending_entry_ != entries_[pending_entry_index_].
    entry = entry_replaced_by_post_commit_error_.get();
    current_index = GetCurrentEntryIndex();
  } else if (IsInitialNavigation() && pending_entry_) {
    // If we are reloading the initial navigation, just use the current
    // pending entry.  Otherwise look up the current entry.
    entry = pending_entry_;
    // The pending entry might be in entries_ (e.g., after a Clone), so we
    // should also update the current_index.
    current_index = pending_entry_index_;
  } else {
    DiscardNonCommittedEntries();
    current_index = GetCurrentEntryIndex();
    if (current_index != -1) {
      entry = GetEntryAtIndex(current_index);
    }
  }

  // If we are no where, then we can't reload.  TODO(darin): We should add a
  // CanReload method.
  if (!entry)
    return;

  // Set ReloadType for |entry|.
  entry->set_reload_type(reload_type);

  if (g_check_for_repost && check_for_repost && entry->GetHasPostData()) {
    // The user is asking to reload a page with POST data. Prompt to make sure
    // they really want to do this. If they do, the dialog will call us back
    // with check_for_repost = false.
    delegate_->NotifyBeforeFormRepostWarningShow();

    pending_reload_ = reload_type;
    delegate_->ActivateAndShowRepostFormWarningDialog();
    return;
  }

  if (!IsInitialNavigation())
    DiscardNonCommittedEntries();

  pending_entry_ = entry;
  pending_entry_index_ = current_index;
  pending_entry_->SetTransitionType(ui::PAGE_TRANSITION_RELOAD);

  NavigateToExistingPendingEntry(reload_type,
                                 FrameTreeNode::kFrameTreeNodeInvalidId);
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

bool NavigationControllerImpl::IsInitialNavigation() {
  return is_initial_navigation_;
}

bool NavigationControllerImpl::IsInitialBlankNavigation() {
  // TODO(creis): Once we create a NavigationEntry for the initial blank page,
  // we'll need to check for entry count 1 and restore_type NONE (to exclude
  // the cloned tab case).
  return IsInitialNavigation() && GetEntryCount() == 0;
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryWithUniqueID(
    int nav_entry_id) const {
  int index = GetEntryIndexWithUniqueID(nav_entry_id);
  return (index != -1) ? entries_[index].get() : nullptr;
}

void NavigationControllerImpl::RegisterExistingOriginToPreventOptInIsolation(
    const url::Origin& origin) {
  for (int i = 0; i < GetEntryCount(); i++) {
    auto* entry = GetEntryAtIndex(i);
    entry->RegisterExistingOriginToPreventOptInIsolation(origin);
  }
  if (entry_replaced_by_post_commit_error_) {
    // It's possible we could come back to this entry if the error
    // page/interstitial goes away.
    entry_replaced_by_post_commit_error_
        ->RegisterExistingOriginToPreventOptInIsolation(origin);
  }
  // TODO(wjmaclean): Register pending commit NavigationRequests rather than
  // visiting pending_entry_, which lacks a committed origin. This will be done
  // in https://chromium-review.googlesource.com/c/chromium/src/+/2136703.
}

void NavigationControllerImpl::SetPendingEntry(
    std::unique_ptr<NavigationEntryImpl> entry) {
  DiscardNonCommittedEntries();
  pending_entry_ = entry.release();
  DCHECK_EQ(-1, pending_entry_index_);
  NotificationService::current()->Notify(
      NOTIFICATION_NAV_ENTRY_PENDING, Source<NavigationController>(this),
      Details<NavigationEntry>(pending_entry_));
}

NavigationEntryImpl* NavigationControllerImpl::GetActiveEntry() {
  if (pending_entry_)
    return pending_entry_;
  return GetLastCommittedEntry();
}

NavigationEntryImpl* NavigationControllerImpl::GetVisibleEntry() {
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
  if (!safe_to_show_pending && pending_entry_ && pending_entry_index_ != -1 &&
      IsInitialNavigation() && !pending_entry_->is_renderer_initiated())
    safe_to_show_pending = true;

  if (safe_to_show_pending)
    return pending_entry_;
  return GetLastCommittedEntry();
}

int NavigationControllerImpl::GetCurrentEntryIndex() {
  if (pending_entry_index_ != -1)
    return pending_entry_index_;
  return last_committed_entry_index_;
}

NavigationEntryImpl* NavigationControllerImpl::GetLastCommittedEntry() {
  if (last_committed_entry_index_ == -1)
    return nullptr;
  return entries_[last_committed_entry_index_].get();
}

bool NavigationControllerImpl::CanViewSource() {
  const std::string& mime_type = delegate_->GetContentsMimeType();
  bool is_viewable_mime_type = blink::IsSupportedNonImageMimeType(mime_type) &&
                               !media::IsSupportedMediaMimeType(mime_type);
  NavigationEntry* visible_entry = GetVisibleEntry();
  return visible_entry && !visible_entry->IsViewSourceMode() &&
         is_viewable_mime_type;
}

int NavigationControllerImpl::GetLastCommittedEntryIndex() {
  // The last committed entry index must always be less than the number of
  // entries.  If there are no entries, it must be -1.
  DCHECK_LT(last_committed_entry_index_, GetEntryCount());
  DCHECK(GetEntryCount() || last_committed_entry_index_ == -1);
  return last_committed_entry_index_;
}

int NavigationControllerImpl::GetEntryCount() {
  DCHECK_LE(entries_.size(), max_entry_count());
  return static_cast<int>(entries_.size());
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtIndex(int index) {
  if (index < 0 || index >= GetEntryCount())
    return nullptr;

  return entries_[index].get();
}

NavigationEntryImpl* NavigationControllerImpl::GetEntryAtOffset(int offset) {
  return GetEntryAtIndex(GetIndexForOffset(offset));
}

int NavigationControllerImpl::GetIndexForOffset(int offset) {
  return GetCurrentEntryIndex() + offset;
}

bool NavigationControllerImpl::CanGoBack() {
  if (!base::FeatureList::IsEnabled(features::kHistoryManipulationIntervention))
    return CanGoToOffset(-1);

  for (int index = GetIndexForOffset(-1); index >= 0; index--) {
    if (!GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
      return true;
  }
  return false;
}

bool NavigationControllerImpl::CanGoForward() {
  return CanGoToOffset(1);
}

bool NavigationControllerImpl::CanGoToOffset(int offset) {
  int index = GetIndexForOffset(offset);
  return index >= 0 && index < GetEntryCount();
}

void NavigationControllerImpl::GoBack() {
  int target_index = GetIndexForOffset(-1);

  // Log metrics for the number of entries that are eligible for skipping on
  // back button and move the target index past the skippable entries, if
  // history intervention is enabled.
  int count_entries_skipped = 0;
  bool all_skippable_entries = true;
  bool history_intervention_enabled =
      base::FeatureList::IsEnabled(features::kHistoryManipulationIntervention);
  for (int index = target_index; index >= 0; index--) {
    if (GetEntryAtIndex(index)->should_skip_on_back_forward_ui()) {
      count_entries_skipped++;
    } else {
      if (history_intervention_enabled)
        target_index = index;
      all_skippable_entries = false;
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Navigation.BackForward.BackTargetSkipped",
                            count_entries_skipped, kMaxSessionHistoryEntries);
  UMA_HISTOGRAM_BOOLEAN("Navigation.BackForward.AllBackTargetsSkippable",
                        all_skippable_entries);

  // Do nothing if all entries are skippable. Normally this path would not
  // happen as consumers would have already checked it in CanGoBack but a lot of
  // tests do not do that.
  if (history_intervention_enabled && all_skippable_entries)
    return;

  GoToIndex(target_index);
}

void NavigationControllerImpl::GoForward() {
  int target_index = GetIndexForOffset(1);

  // Log metrics for the number of entries that are eligible for skipping on
  // forward button and move the target index past the skippable entries, if
  // history intervention is enabled.
  // Note that at least one entry (the last one) will be non-skippable since
  // entries are marked skippable only when they add another entry because of
  // redirect or pushState.
  int count_entries_skipped = 0;
  bool history_intervention_enabled =
      base::FeatureList::IsEnabled(features::kHistoryManipulationIntervention);
  for (size_t index = target_index; index < entries_.size(); index++) {
    if (GetEntryAtIndex(index)->should_skip_on_back_forward_ui()) {
      count_entries_skipped++;
    } else {
      if (history_intervention_enabled)
        target_index = index;
      break;
    }
  }
  UMA_HISTOGRAM_ENUMERATION("Navigation.BackForward.ForwardTargetSkipped",
                            count_entries_skipped, kMaxSessionHistoryEntries);

  GoToIndex(target_index);
}

void NavigationControllerImpl::GoToIndex(int index) {
  GoToIndex(index, FrameTreeNode::kFrameTreeNodeInvalidId);
}

void NavigationControllerImpl::GoToIndex(int index,
                                         int sandbox_frame_tree_node_id) {
  TRACE_EVENT0("browser,navigation,benchmark",
               "NavigationControllerImpl::GoToIndex");
  if (index < 0 || index >= static_cast<int>(entries_.size())) {
    NOTREACHED();
    return;
  }

  DiscardNonCommittedEntries();

  DCHECK_EQ(nullptr, pending_entry_);
  DCHECK_EQ(-1, pending_entry_index_);
  pending_entry_ = entries_[index].get();
  pending_entry_index_ = index;
  pending_entry_->SetTransitionType(ui::PageTransitionFromInt(
      pending_entry_->GetTransitionType() | ui::PAGE_TRANSITION_FORWARD_BACK));
  NavigateToExistingPendingEntry(ReloadType::NONE, sandbox_frame_tree_node_id);
}

void NavigationControllerImpl::GoToOffset(int offset) {
  // Note: This is actually reached in unit tests.
  if (!CanGoToOffset(offset))
    return;

  GoToIndex(GetIndexForOffset(offset));
}

bool NavigationControllerImpl::RemoveEntryAtIndex(int index) {
  if (index == last_committed_entry_index_ || index == pending_entry_index_)
    return false;

  RemoveEntryAtIndexInternal(index);
  return true;
}

void NavigationControllerImpl::PruneForwardEntries() {
  DiscardNonCommittedEntries();
  int remove_start_index = last_committed_entry_index_ + 1;
  int num_removed = static_cast<int>(entries_.size()) - remove_start_index;
  if (num_removed <= 0)
    return;
  entries_.erase(entries_.begin() + remove_start_index, entries_.end());
  NotifyPrunedEntries(this, remove_start_index /* start index */,
                      num_removed /* count */);
}

void NavigationControllerImpl::UpdateVirtualURLToURL(NavigationEntryImpl* entry,
                                                     const GURL& new_url) {
  GURL new_virtual_url(new_url);
  if (BrowserURLHandlerImpl::GetInstance()->ReverseURLRewrite(
          &new_virtual_url, entry->GetVirtualURL(), browser_context_)) {
    entry->SetVirtualURL(new_virtual_url);
  }
}

void NavigationControllerImpl::LoadURL(const GURL& url,
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
  if (params.is_renderer_initiated)
    DCHECK(params.initiator_origin.has_value());

  TRACE_EVENT1("browser,navigation",
               "NavigationControllerImpl::LoadURLWithParams", "url",
               params.url.possibly_invalid_spec());
  bool is_explicit_navigation =
      GetContentClient()->browser()->IsExplicitNavigation(
          params.transition_type);
  if (HandleDebugURL(params.url, params.transition_type,
                     is_explicit_navigation)) {
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
  }

  // The user initiated a load, we don't need to reload anymore.
  needs_reload_ = false;

  NavigateWithoutEntry(params);
}

bool NavigationControllerImpl::PendingEntryMatchesRequest(
    NavigationRequest* request) const {
  return pending_entry_ &&
         pending_entry_->GetUniqueID() == request->nav_entry_id();
}

bool NavigationControllerImpl::RendererDidNavigate(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    LoadCommittedDetails* details,
    bool is_same_document_navigation,
    bool previous_document_was_activated,
    NavigationRequest* navigation_request) {
  DCHECK(navigation_request);
  is_initial_navigation_ = false;

  // Save the previous state before we clobber it.
  bool overriding_user_agent_changed = false;
  if (GetLastCommittedEntry()) {
    if (entry_replaced_by_post_commit_error_) {
      if (is_same_document_navigation) {
        // Same document navigations should not be possible on error pages and
        // would leave the controller in a weird state. Kill the renderer if
        // that happens.
        bad_message::ReceivedBadMessage(
            rfh->GetProcess(), bad_message::NC_SAME_DOCUMENT_POST_COMMIT_ERROR);
      }
      // Any commit while a post-commit error page is showing should put the
      // original entry back, replacing the error page's entry.  This includes
      // reloads, where the original entry was used as the pending entry and
      // should now be at the correct index at commit time.
      entries_[last_committed_entry_index_] =
          std::move(entry_replaced_by_post_commit_error_);
    }
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

  // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
  // implementing back-forward cache.

  // Create a new metrics object or reuse the previous one depending on whether
  // it's a main frame navigation or not.
  scoped_refptr<BackForwardCacheMetrics> back_forward_cache_metrics =
      BackForwardCacheMetrics::CreateOrReuseBackForwardCacheMetrics(
          GetLastCommittedEntry(), !rfh->GetParent(),
          params.document_sequence_number);
  // Notify the last active entry that we have navigated away.
  if (!rfh->GetParent() && !is_same_document_navigation) {
    if (NavigationEntryImpl* navigation_entry = GetLastCommittedEntry()) {
      if (auto* metrics = navigation_entry->back_forward_cache_metrics()) {
        metrics->MainFrameDidNavigateAwayFromDocument(rfh, details,
                                                      navigation_request);
      }
    }
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

  // Make sure we do not discard the pending entry for a different ongoing
  // navigation when a same document commit comes in unexpectedly from the
  // renderer.  Limit this to a very narrow set of conditions to avoid risks to
  // other navigation types. See https://crbug.com/900036.
  // TODO(crbug.com/926009): Handle history.pushState() as well.
  bool keep_pending_entry = is_same_document_navigation &&
                            details->type == NAVIGATION_TYPE_EXISTING_PAGE &&
                            pending_entry_ &&
                            !PendingEntryMatchesRequest(navigation_request);

  switch (details->type) {
    case NAVIGATION_TYPE_NEW_PAGE:
      RendererDidNavigateToNewPage(
          rfh, params, details->is_same_document, details->did_replace_entry,
          previous_document_was_activated, navigation_request);
      break;
    case NAVIGATION_TYPE_EXISTING_PAGE:
      RendererDidNavigateToExistingPage(rfh, params, details->is_same_document,
                                        was_restored, navigation_request,
                                        keep_pending_entry);
      break;
    case NAVIGATION_TYPE_SAME_PAGE:
      RendererDidNavigateToSamePage(rfh, params, details->is_same_document,
                                    navigation_request);
      break;
    case NAVIGATION_TYPE_NEW_SUBFRAME:
      RendererDidNavigateNewSubframe(
          rfh, params, details->is_same_document, details->did_replace_entry,
          previous_document_was_activated, navigation_request);
      break;
    case NAVIGATION_TYPE_AUTO_SUBFRAME:
      if (!RendererDidNavigateAutoSubframe(rfh, params, navigation_request)) {
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
      if (pending_entry_)
        DiscardNonCommittedEntries();
      return false;
    case NAVIGATION_TYPE_UNKNOWN:
      NOTREACHED();
      break;
  }

  // At this point, we know that the navigation has just completed, so
  // record the time.
  //
  // TODO(akalin): Use "sane time" as described in
  // https://www.chromium.org/developers/design-documents/sane-time .
  base::Time timestamp =
      time_smoother_.GetSmoothedTime(get_timestamp_callback_.Run());
  DVLOG(1) << "Navigation finished at (smoothed) timestamp "
           << timestamp.ToDeltaSinceWindowsEpoch().InMicroseconds();

  // If we aren't keeping the pending entry, there shouldn't be one at this
  // point. Clear it again in case any error cases above forgot to do so.
  // TODO(pbos): Consider a CHECK here that verifies that the pending entry has
  // been cleared instead of protecting against it.
  if (!keep_pending_entry)
    DiscardNonCommittedEntries();

  // All committed entries should have nonempty content state so WebKit doesn't
  // get confused when we go back to them (see the function for details).
  DCHECK(params.page_state.IsValid()) << "Shouldn't see an empty PageState.";
  NavigationEntryImpl* active_entry = GetLastCommittedEntry();
  active_entry->SetTimestamp(timestamp);
  active_entry->SetHttpStatusCode(params.http_status_code);
  // TODO(altimin, crbug.com/933147): Remove this logic after we are done with
  // implementing back-forward cache.
  if (!active_entry->back_forward_cache_metrics()) {
    active_entry->set_back_forward_cache_metrics(
        std::move(back_forward_cache_metrics));
  }
  active_entry->back_forward_cache_metrics()->DidCommitNavigation(
      navigation_request,
      back_forward_cache_.IsAllowed(navigation_request->GetURL()));

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

    // Remember the bindings the renderer process has at this point, so that
    // we do not grant this entry additional bindings if we come back to it.
    frame_entry->SetBindings(rfh->GetEnabledBindings());
  }

  // Once it is committed, we no longer need to track several pieces of state on
  // the entry.
  active_entry->ResetForCommit(frame_entry);

  // The active entry's SiteInstance should match our SiteInstance.
  // TODO(creis): This check won't pass for subframes until we create entries
  // for subframe navigations.
  if (!rfh->GetParent())
    CHECK_EQ(active_entry->site_instance(), rfh->GetSiteInstance());

  // Now prep the rest of the details for the notification and broadcast.
  details->entry = active_entry;
  details->is_main_frame = !rfh->GetParent();
  details->http_status_code = params.http_status_code;

  // If the NavigationRequest was created without a NavigationEntry and
  // SetIsOverridingUserAgent() was called, it needs to be applied to the
  // NavigationEntry now.
  if (!navigation_request->nav_entry_id() &&
      navigation_request->was_set_overriding_user_agent_called()) {
    active_entry->SetIsOverridingUserAgent(
        navigation_request->entry_overrides_ua());
  }

  NotifyNavigationEntryCommitted(details);

  if (active_entry->GetURL().SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
      navigation_request->GetNetErrorCode() == net::OK) {
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
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params) {
  TraceReturnReason<tracing_category::kNavigation> trace_return(
      "ClassifyNavigation");

  if (params.did_create_new_entry) {
    // A new entry. We may or may not have a pending entry for the page, and
    // this may or may not be the main frame.
    if (!rfh->GetParent()) {
      trace_return.set_return_reason("new entry, no parent, new page");
      return NAVIGATION_TYPE_NEW_PAGE;
    }

    // When this is a new subframe navigation, we should have a committed page
    // in which it's a subframe. This may not be the case when an iframe is
    // navigated on a popup navigated to about:blank (the iframe would be
    // written into the popup by script on the main page). For these cases,
    // there isn't any navigation stuff we can do, so just ignore it.
    if (!GetLastCommittedEntry()) {
      trace_return.set_return_reason("new entry, no last committed, ignore");
      return NAVIGATION_TYPE_NAV_IGNORE;
    }

    // Valid subframe navigation.
    trace_return.set_return_reason("new entry, new subframe");
    return NAVIGATION_TYPE_NEW_SUBFRAME;
  }

  // We only clear the session history when navigating to a new page.
  DCHECK(!params.history_list_was_cleared);

  if (rfh->GetParent()) {
    // All manual subframes would be did_create_new_entry and handled above, so
    // we know this is auto.
    if (GetLastCommittedEntry()) {
      trace_return.set_return_reason("subframe, last commmited, auto subframe");
      return NAVIGATION_TYPE_AUTO_SUBFRAME;
    }

    // We ignore subframes created in non-committed pages; we'd appreciate if
    // people stopped doing that.
    trace_return.set_return_reason("subframe, no last commmited, ignore");
    return NAVIGATION_TYPE_NAV_IGNORE;
  }

  if (params.nav_entry_id == 0) {
    // This is a renderer-initiated navigation (nav_entry_id == 0), but didn't
    // create a new page.

    // Just like above in the did_create_new_entry case, it's possible to
    // scribble onto an uncommitted page. Again, there isn't any navigation
    // stuff that we can do, so ignore it here as well.
    NavigationEntry* last_committed = GetLastCommittedEntry();
    if (!last_committed) {
      trace_return.set_return_reason("nav entry 0, no last committed, ignore");
      return NAVIGATION_TYPE_NAV_IGNORE;
    }

    // This is history.replaceState() or history.reload().
    // TODO(nasko): With error page isolation, reloading an existing session
    // history entry can result in change of SiteInstance. Check for such a case
    // here and classify it as NEW_PAGE, as such navigations should be treated
    // as new with replacement.
    trace_return.set_return_reason(
        "nav entry 0, last committed, existing page");
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
      trace_return.set_return_reason("pending matching nav entry, new page");
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
        trace_return.set_return_reason("no pending, new page");
        return NAVIGATION_TYPE_NEW_PAGE;
      }

      // Otherwise, this happens when you press enter in the URL bar to reload.
      // We will create a pending entry, but Blink will convert it to a reload
      // since it's the same page and not create a new entry for it (the user
      // doesn't want to have a new back/forward entry when they do this).
      // Therefore we want to just ignore the pending entry and go back to where
      // we were (the "existing entry").
      // TODO(creis,avi): Eliminate SAME_PAGE in https://crbug.com/536102.
      trace_return.set_return_reason("no pending, same page");
      return NAVIGATION_TYPE_SAME_PAGE;
    }
  }

  // Everything below here is assumed to be an existing entry, but if there is
  // no last committed entry, we must consider it a new navigation instead.
  if (!GetLastCommittedEntry()) {
    trace_return.set_return_reason("no last committed, new page");
    return NAVIGATION_TYPE_NEW_PAGE;
  }

  if (params.intended_as_new_entry) {
    // This was intended to be a navigation to a new entry but the pending entry
    // got cleared in the meanwhile. Classify as EXISTING_PAGE because we may or
    // may not have a pending entry.
    trace_return.set_return_reason("indented as new entry, new page");
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  if (params.url_is_unreachable && failed_pending_entry_id_ != 0 &&
      params.nav_entry_id == failed_pending_entry_id_) {
    // If the renderer was going to a new pending entry that got cleared because
    // of an error, this is the case of the user trying to retry a failed load
    // by pressing return. Classify as EXISTING_PAGE because we probably don't
    // have a pending entry.
    trace_return.set_return_reason(
        "unreachable, matching pending, existing page");
    return NAVIGATION_TYPE_EXISTING_PAGE;
  }

  // Now we know that the notification is for an existing page. Find that entry.
  int existing_entry_index = GetEntryIndexWithUniqueID(params.nav_entry_id);
  trace_return.traced_value()->SetInteger("existing_entry_index",
                                          existing_entry_index);
  if (existing_entry_index == -1) {
    // The renderer has committed a navigation to an entry that no longer
    // exists. Because the renderer is showing that page, resurrect that entry.
    trace_return.set_return_reason("existing entry -1, new page");
    return NAVIGATION_TYPE_NEW_PAGE;
  }

  // Since we weeded out "new" navigations above, we know this is an existing
  // (back/forward) navigation.
  trace_return.set_return_reason("default return, existing page");
  return NAVIGATION_TYPE_EXISTING_PAGE;
}

void NavigationControllerImpl::RendererDidNavigateToNewPage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    bool replace_entry,
    bool previous_document_was_activated,
    NavigationRequest* request) {
  std::unique_ptr<NavigationEntryImpl> new_entry;
  bool update_virtual_url = false;

  const base::Optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;

  // First check if this is an in-page navigation.  If so, clone the current
  // entry instead of looking at the pending entry, because the pending entry
  // does not have any subframe history items.
  if (is_same_document && GetLastCommittedEntry()) {
    auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
        rfh->frame_tree_node()->unique_name(), params.item_sequence_number,
        params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
        params.url, (params.url_is_unreachable) ? nullptr : &params.origin,
        params.referrer, initiator_origin, params.redirects, params.page_state,
        params.method, params.post_id, nullptr /* blob_url_loader_factory */,
        nullptr /* web_bundle_navigation_info */);

    new_entry = GetLastCommittedEntry()->CloneAndReplace(
        frame_entry, true, rfh->frame_tree_node(),
        delegate_->GetFrameTree()->root());
    if (new_entry->GetURL().GetOrigin() != params.url.GetOrigin()) {
      // TODO(jam): we had one report of this with a URL that was redirecting to
      // only tildes. Until we understand that better, don't copy the cert in
      // this case.
      new_entry->GetSSL() = SSLStatus();

      if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
          request->GetNetErrorCode() == net::OK) {
        UMA_HISTOGRAM_BOOLEAN(
            "Navigation.SecureSchemeHasSSLStatus.NewPageInPageOriginMismatch",
            !!new_entry->GetSSL().certificate);
      }
    }

    // It is expected that |frame_entry| is now owned by |new_entry|. This means
    // that |frame_entry| should now have a reference count of exactly 2: one
    // due to the local variable |frame_entry|, and another due to |new_entry|
    // also retaining one. This should never fail, because it's the main frame.
    CHECK(!frame_entry->HasOneRef() && frame_entry->HasAtLeastOneRef());

    update_virtual_url = new_entry->update_virtual_url_with_url();

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        request->GetNetErrorCode() == net::OK) {
      UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus.NewPageInPage",
                            !!new_entry->GetSSL().certificate);
    }
  }

  // Only make a copy of the pending entry if it is appropriate for the new page
  // that was just loaded. Verify this by checking if the entry corresponds
  // to the given NavigationRequest. Additionally, coarsely check that:
  // 1. The SiteInstance hasn't been assigned to something else.
  // 2. The pending entry was intended as a new entry, rather than being a
  // history navigation that was interrupted by an unrelated,
  // renderer-initiated navigation.
  // TODO(csharrison): Investigate whether we can remove some of the coarser
  // checks.
  if (!new_entry && PendingEntryMatchesRequest(request) &&
      pending_entry_index_ == -1 &&
      (!pending_entry_->site_instance() ||
       pending_entry_->site_instance() == rfh->GetSiteInstance())) {
    new_entry = pending_entry_->Clone();

    update_virtual_url = new_entry->update_virtual_url_with_url();
    new_entry->GetSSL() =
        SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        request->GetNetErrorCode() == net::OK) {
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.SecureSchemeHasSSLStatus.NewPagePendingEntryMatches",
          !!new_entry->GetSSL().certificate);
    }
  }

  // For non-in-page commits with no matching pending entry, create a new entry.
  if (!new_entry) {
    new_entry = std::make_unique<NavigationEntryImpl>(
        rfh->GetSiteInstance(), params.url, params.referrer, initiator_origin,
        base::string16(),  // title
        params.transition, request->IsRendererInitiated(),
        nullptr);  // blob_url_loader_factory

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
    new_entry->GetSSL() =
        SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        request->GetNetErrorCode() == net::OK) {
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
  if (!params.url_is_unreachable)
    frame_entry->set_committed_origin(params.origin);
  if (request->web_bundle_navigation_info()) {
    frame_entry->set_web_bundle_navigation_info(
        request->web_bundle_navigation_info()->Clone());
  }

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
    DiscardNonCommittedEntries();
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

  SetShouldSkipOnBackForwardUIIfNeeded(
      replace_entry, previous_document_was_activated,
      request->IsRendererInitiated(), request->GetPreviousPageUkmSourceId());

  InsertOrReplaceEntry(std::move(new_entry), replace_entry,
                       !request->post_commit_error_page_html().empty());
}

void NavigationControllerImpl::RendererDidNavigateToExistingPage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    bool was_restored,
    NavigationRequest* request,
    bool keep_pending_entry) {
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
    // NavigationRequest so don't overwrite the existing entry's SSLStatus.
    if (!is_same_document)
      entry->GetSSL() =
          SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        request->GetNetErrorCode() == net::OK) {
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
      // There's no SSLStatus in the NavigationRequest for same document
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
      // In rapid back/forward navigations |request| sometimes won't have a cert
      // (http://crbug.com/727892). So we use the request's cert if it exists,
      // otherwise we only reuse the existing cert if the origins match.
      if (request->GetSSLInfo().has_value() &&
          request->GetSSLInfo()->is_valid()) {
        entry->GetSSL() = SSLStatus(*(request->GetSSLInfo()));
      } else if (entry->GetURL().GetOrigin() != request->GetURL().GetOrigin()) {
        entry->GetSSL() = SSLStatus();
      }
    }

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        request->GetNetErrorCode() == net::OK) {
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

    CopyReplacedNavigationEntryDataIfPreviouslyEmpty(entry, entry);

    // If this is a same document navigation, then there's no SSLStatus in the
    // NavigationRequest so don't overwrite the existing entry's SSLStatus.
    if (!is_same_document)
      entry->GetSSL() =
          SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

    if (params.url.SchemeIs(url::kHttpsScheme) && !rfh->GetParent() &&
        request->GetNetErrorCode() == net::OK) {
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
  DCHECK(!entry->site_instance() || !entry->GetRedirectChain().empty() ||
         entry->site_instance() == rfh->GetSiteInstance());

  // Update the existing FrameNavigationEntry to ensure all of its members
  // reflect the parameters coming from the renderer process.
  const base::Optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;
  entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, GetCommittedOriginForFrameEntry(params), params.referrer,
      initiator_origin, params.redirects, params.page_state, params.method,
      params.post_id, nullptr /* blob_url_loader_factory */,
      request->web_bundle_navigation_info()
          ? request->web_bundle_navigation_info()->Clone()
          : nullptr);

  // The redirected to page should not inherit the favicon from the previous
  // page.
  if (ui::PageTransitionIsRedirect(params.transition) && !is_same_document)
    entry->GetFavicon() = FaviconStatus();

  // We should also usually discard the pending entry if it corresponds to a
  // different navigation, since that one is now likely canceled.  In rare
  // cases, we leave the pending entry for another navigation in place when we
  // know it is still ongoing, to avoid a flicker in the omnibox (see
  // https://crbug.com/900036).
  //
  // Note that we need to use the "internal" version since we don't want to
  // actually change any other state, just kill the pointer.
  if (!keep_pending_entry)
    DiscardNonCommittedEntries();

  // Update the last committed index to reflect the committed entry.
  last_committed_entry_index_ = GetIndexOfEntry(entry);
}

void NavigationControllerImpl::RendererDidNavigateToSamePage(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    NavigationRequest* request) {
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
  // might change (but it's still considered a SAME_PAGE navigation), so we must
  // update the SSL status if we perform a network request (e.g. a
  // non-same-document navigation). Requests that don't result in a network
  // request do not have a valid SSL status, but since the document didn't
  // change, the previous SSLStatus is still valid.
  if (!is_same_document)
    existing_entry->GetSSL() =
        SSLStatus(request->GetSSLInfo().value_or(net::SSLInfo()));

  if (existing_entry->GetURL().SchemeIs(url::kHttpsScheme) &&
      !rfh->GetParent() && request->GetNetErrorCode() == net::OK) {
    UMA_HISTOGRAM_BOOLEAN("Navigation.SecureSchemeHasSSLStatus.SamePage",
                          !!existing_entry->GetSSL().certificate);
  }

  // The extra headers may have changed due to reloading with different headers.
  existing_entry->set_extra_headers(pending_entry_->extra_headers());

  // Update the existing FrameNavigationEntry to ensure all of its members
  // reflect the parameters coming from the renderer process.
  const base::Optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;
  existing_entry->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, GetCommittedOriginForFrameEntry(params), params.referrer,
      initiator_origin, params.redirects, params.page_state, params.method,
      params.post_id, nullptr /* blob_url_loader_factory */,
      request->web_bundle_navigation_info()
          ? request->web_bundle_navigation_info()->Clone()
          : nullptr);

  DiscardNonCommittedEntries();
}

void NavigationControllerImpl::RendererDidNavigateNewSubframe(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    bool is_same_document,
    bool replace_entry,
    bool previous_document_was_activated,
    NavigationRequest* request) {
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

  // This FrameNavigationEntry might not end up being used in the
  // CloneAndReplace() call below, if a spot can't be found for it in the tree.
  const base::Optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;
  auto frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
      rfh->frame_tree_node()->unique_name(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, (params.url_is_unreachable) ? nullptr : &params.origin,
      params.referrer, initiator_origin, params.redirects, params.page_state,
      params.method, params.post_id, nullptr /* blob_url_loader_factory */,
      request->web_bundle_navigation_info()
          ? request->web_bundle_navigation_info()->Clone()
          : nullptr);

  std::unique_ptr<NavigationEntryImpl> new_entry =
      GetLastCommittedEntry()->CloneAndReplace(
          std::move(frame_entry), is_same_document, rfh->frame_tree_node(),
          delegate_->GetFrameTree()->root());

  SetShouldSkipOnBackForwardUIIfNeeded(
      replace_entry, previous_document_was_activated,
      request->IsRendererInitiated(), request->GetPreviousPageUkmSourceId());

  // TODO(creis): Update this to add the frame_entry if we can't find the one
  // to replace, which can happen due to a unique name change. See
  // https://crbug.com/607205. For now, the call to CloneAndReplace() will
  // delete the |frame_entry| when the function exits if it doesn't get used.

  InsertOrReplaceEntry(std::move(new_entry), replace_entry, false);
}

bool NavigationControllerImpl::RendererDidNavigateAutoSubframe(
    RenderFrameHostImpl* rfh,
    const FrameHostMsg_DidCommitProvisionalLoad_Params& params,
    NavigationRequest* request) {
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
      DiscardNonCommittedEntries();

      // History navigations should send a commit notification.
      send_commit_notification = true;
    }
  }

  // This may be a "new auto" case where we add a new FrameNavigationEntry, or
  // it may be a "history auto" case where we update an existing one.
  NavigationEntryImpl* last_committed = GetLastCommittedEntry();
  const base::Optional<url::Origin>& initiator_origin =
      request->common_params().initiator_origin;
  last_committed->AddOrUpdateFrameEntry(
      rfh->frame_tree_node(), params.item_sequence_number,
      params.document_sequence_number, rfh->GetSiteInstance(), nullptr,
      params.url, GetCommittedOriginForFrameEntry(params), params.referrer,
      initiator_origin, params.redirects, params.page_state, params.method,
      params.post_id, nullptr /* blob_url_loader_factory */,
      request->web_bundle_navigation_info()
          ? request->web_bundle_navigation_info()->Clone()
          : nullptr);

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
    RenderFrameHost* rfh) {
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

  auto prefs = rfhi->GetOrCreateWebPreferences();
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

void NavigationControllerImpl::CopyStateFrom(NavigationController* temp,
                                             bool needs_reload) {
  NavigationControllerImpl* source =
      static_cast<NavigationControllerImpl*>(temp);
  // Verify that we look new.
  DCHECK_EQ(0, GetEntryCount());
  DCHECK(!GetPendingEntry());

  if (source->GetEntryCount() == 0)
    return;  // Nothing new to do.

  needs_reload_ = needs_reload;
  needs_reload_type_ = NeedsReloadType::kCopyStateFrom;
  InsertEntriesFrom(source, source->GetEntryCount());

  for (auto it = source->session_storage_namespace_map_.begin();
       it != source->session_storage_namespace_map_.end(); ++it) {
    SessionStorageNamespaceImpl* source_namespace =
        static_cast<SessionStorageNamespaceImpl*>(it->second.get());
    session_storage_namespace_map_[it->first] = source_namespace->Clone();
  }

  FinishRestore(source->last_committed_entry_index_,
                RestoreType::CURRENT_SESSION);
}

void NavigationControllerImpl::CopyStateFromAndPrune(NavigationController* temp,
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
    source->PruneOldestSkippableEntryIfFull();

  // Insert the entries from source. Ignore any pending entry, since it has not
  // committed in source.
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

  InsertEntriesFrom(source, max_source_index);

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
        deletionPredicate.Run(entries_[i].get())) {
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

bool NavigationControllerImpl::IsEntryMarkedToBeSkipped(int index) {
  auto* entry = GetEntryAtIndex(index);
  return entry && entry->should_skip_on_back_forward_ui();
}

BackForwardCacheImpl& NavigationControllerImpl::GetBackForwardCache() {
  return back_forward_cache_;
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

  // Ensure any refs to the current pending entry are ignored if they get
  // deleted, by clearing the set of known refs. All future pending entries will
  // only be affected by new refs.
  pending_entry_refs_.clear();
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

void NavigationControllerImpl::NotifyUserActivation() {
  // When a user activation occurs, ensure that all adjacent entries for the
  // same document clear their skippable bit, so that the history manipulation
  // intervention does not apply to them.
  // TODO(crbug.com/949279) in case it becomes necessary for resetting based on
  // which frame created an entry and which frame has the user gesture.
  auto* last_committed_entry = GetLastCommittedEntry();
  if (!last_committed_entry)
    return;

  SetSkippableForSameDocumentEntries(GetLastCommittedEntryIndex(), false);
}

bool NavigationControllerImpl::StartHistoryNavigationInNewSubframe(
    RenderFrameHostImpl* render_frame_host,
    mojo::PendingAssociatedRemote<mojom::NavigationClient>* navigation_client) {
  NavigationEntryImpl* entry =
      GetEntryWithUniqueID(render_frame_host->nav_entry_id());
  if (!entry)
    return false;

  FrameNavigationEntry* frame_entry =
      entry->GetFrameEntry(render_frame_host->frame_tree_node());
  if (!frame_entry)
    return false;

  std::unique_ptr<NavigationRequest> request = CreateNavigationRequestFromEntry(
      render_frame_host->frame_tree_node(), entry, frame_entry,
      ReloadType::NONE, false /* is_same_document_history_load */,
      true /* is_history_navigation_in_new_child */);

  if (!request)
    return false;

  request->SetNavigationClient(std::move(*navigation_client));

  render_frame_host->frame_tree_node()->navigator().Navigate(
      std::move(request), ReloadType::NONE, RestoreType::NONE);

  return true;
}

bool NavigationControllerImpl::ReloadFrame(FrameTreeNode* frame_tree_node) {
  NavigationEntryImpl* entry = GetEntryAtIndex(GetCurrentEntryIndex());
  if (!entry)
    return false;
  FrameNavigationEntry* frame_entry = entry->GetFrameEntry(frame_tree_node);
  if (!frame_entry)
    return false;
  ReloadType reload_type = ReloadType::NORMAL;
  entry->set_reload_type(reload_type);
  std::unique_ptr<NavigationRequest> request = CreateNavigationRequestFromEntry(
      frame_tree_node, entry, frame_entry, reload_type,
      false /* is_same_document_history_load */,
      false /* is_history_navigation_in_new_child */);
  if (!request)
    return false;
  frame_tree_node->navigator().Navigate(std::move(request), reload_type,
                                        RestoreType::NONE);
  return true;
}

void NavigationControllerImpl::GoToOffsetInSandboxedFrame(
    int offset,
    int sandbox_frame_tree_node_id) {
  if (!CanGoToOffset(offset))
    return;
  GoToIndex(GetIndexForOffset(offset), sandbox_frame_tree_node_id);
}

void NavigationControllerImpl::NavigateFromFrameProxy(
    RenderFrameHostImpl* render_frame_host,
    const GURL& url,
    const GlobalFrameRoutingId& initiator_routing_id,
    const base::Optional<url::Origin>& initiator_origin,
    bool is_renderer_initiated,
    SiteInstance* source_site_instance,
    const Referrer& referrer,
    ui::PageTransition page_transition,
    bool should_replace_current_entry,
    NavigationDownloadPolicy download_policy,
    const std::string& method,
    scoped_refptr<network::ResourceRequestBody> post_body,
    const std::string& extra_headers,
    scoped_refptr<network::SharedURLLoaderFactory> blob_url_loader_factory,
    const base::Optional<Impression>& impression) {
  if (is_renderer_initiated)
    DCHECK(initiator_origin.has_value());

  FrameTreeNode* node = render_frame_host->frame_tree_node();

  // Don't allow an entry replacement if there is no entry to replace.
  // http://crbug.com/457149
  if (GetEntryCount() == 0)
    should_replace_current_entry = false;

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
          GURL(url::kAboutBlankURL), referrer, initiator_origin,
          source_site_instance, page_transition, is_renderer_initiated,
          extra_headers, browser_context_,
          nullptr /* blob_url_loader_factory */, should_replace_current_entry,
          GetWebContents()));
      // CreateNavigationEntry() may have changed the transition type.
      page_transition = entry->GetTransitionType();
    }
    entry->AddOrUpdateFrameEntry(
        node, -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), url,
        base::nullopt /* commit_origin */, referrer, initiator_origin,
        std::vector<GURL>(), PageState(), method, -1, blob_url_loader_factory,
        nullptr /* web_bundle_navigation_info */);
  } else {
    // Main frame case.
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        url, referrer, initiator_origin, source_site_instance, page_transition,
        is_renderer_initiated, extra_headers, browser_context_,
        blob_url_loader_factory, should_replace_current_entry,
        GetWebContents()));
    // CreateNavigationEntry() may have changed the transition type.
    page_transition = entry->GetTransitionType();
    entry->root_node()->frame_entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(source_site_instance));
    entry->root_node()->frame_entry->set_method(method);
  }

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
    frame_entry = base::MakeRefCounted<FrameNavigationEntry>(
        node->unique_name(), -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(source_site_instance), url,
        nullptr /* origin */, referrer, initiator_origin, std::vector<GURL>(),
        PageState(), method, -1, blob_url_loader_factory,
        nullptr /* web_bundle_navigation_info */);
  }

  LoadURLParams params(url);
  params.initiator_routing_id = initiator_routing_id;
  params.initiator_origin = initiator_origin;
  params.source_site_instance = source_site_instance;
  params.load_type = method == "POST" ? LOAD_TYPE_HTTP_POST : LOAD_TYPE_DEFAULT;
  params.transition_type = page_transition;
  params.frame_tree_node_id = node->frame_tree_node_id();
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
  /* params.should_replace_current_entry: skip */
  /* params.frame_name: skip */
  // TODO(clamy): See if user gesture should be propagated to this function.
  params.has_user_gesture = false;
  params.should_clear_history_list = false;
  params.started_from_context_menu = false;
  /* params.navigation_ui_data: skip */
  /* params.input_start: skip */
  params.was_activated = mojom::WasActivatedOption::kUnknown;
  /* params.reload_type: skip */
  params.impression = impression;

  std::unique_ptr<NavigationRequest> request =
      CreateNavigationRequestFromLoadParams(
          node, params, override_user_agent, should_replace_current_entry,
          false /* has_user_gesture */, download_policy, ReloadType::NONE,
          entry.get(), frame_entry.get(), params.transition_type);

  if (!request)
    return;

  // At this stage we are proceeding with this navigation. If this was renderer
  // initiated with user gesture, we need to make sure we clear up potential
  // remains of a cancelled browser initiated navigation to avoid URL spoofs.
  DiscardNonCommittedEntries();

  node->navigator().Navigate(std::move(request), ReloadType::NONE,
                             RestoreType::NONE);
}

void NavigationControllerImpl::SetSessionStorageNamespace(
    const std::string& partition_id,
    SessionStorageNamespace* session_storage_namespace) {
  if (!session_storage_namespace)
    return;

  // We can't overwrite an existing SessionStorage without violating spec.
  // Attempts to do so may give a tab access to another tab's session storage
  // so die hard on an error.
  bool successful_insert =
      session_storage_namespace_map_
          .insert(
              make_pair(partition_id, static_cast<SessionStorageNamespaceImpl*>(
                                          session_storage_namespace)))
          .second;
  CHECK(successful_insert) << "Cannot replace existing SessionStorageNamespace";
}

bool NavigationControllerImpl::IsUnmodifiedBlankTab() {
  return IsInitialNavigation() && !GetLastCommittedEntry() &&
         !delegate_->HasAccessedInitialDocument();
}

SessionStorageNamespace* NavigationControllerImpl::GetSessionStorageNamespace(
    SiteInstance* instance) {
  std::string partition_id;
  if (instance) {
    // TODO(ajwong): When GetDefaultSessionStorageNamespace() goes away, remove
    // this if statement so |instance| must not be null.
    partition_id = GetContentClient()->browser()->GetStoragePartitionIdForSite(
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
    DCHECK(static_cast<SessionStorageNamespaceImpl*>(it->second.get())
               ->IsFromContext(context_wrapper));
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
NavigationControllerImpl::GetSessionStorageNamespaceMap() {
  return session_storage_namespace_map_;
}

bool NavigationControllerImpl::NeedsReload() {
  return needs_reload_;
}

void NavigationControllerImpl::SetNeedsReload() {
  SetNeedsReload(NeedsReloadType::kRequestedByClient);
}

void NavigationControllerImpl::SetNeedsReload(NeedsReloadType type) {
  needs_reload_ = true;
  needs_reload_type_ = type;

  if (last_committed_entry_index_ != -1) {
    entries_[last_committed_entry_index_]->SetTransitionType(
        ui::PAGE_TRANSITION_RELOAD);
  }
}

void NavigationControllerImpl::RemoveEntryAtIndexInternal(int index) {
  DCHECK_LT(index, GetEntryCount());
  DCHECK_NE(index, last_committed_entry_index_);
  DiscardNonCommittedEntries();

  entries_.erase(entries_.begin() + index);
  if (last_committed_entry_index_ > index)
    last_committed_entry_index_--;
}

NavigationEntryImpl* NavigationControllerImpl::GetPendingEntry() {
  // If there is no pending_entry_, there should be no pending_entry_index_.
  DCHECK(pending_entry_ || pending_entry_index_ == -1);

  // If there is a pending_entry_index_, then pending_entry_ must be the entry
  // at that index. An exception is while a reload of a post commit error page
  // is ongoing; in that case pending entry will point to the entry replaced
  // by the error.
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_ == GetEntryAtIndex(pending_entry_index_) ||
         pending_entry_ == entry_replaced_by_post_commit_error_.get());

  return pending_entry_;
}

int NavigationControllerImpl::GetPendingEntryIndex() {
  // The pending entry index must always be less than the number of entries.
  // If there are no entries, it must be exactly -1.
  DCHECK_LT(pending_entry_index_, GetEntryCount());
  DCHECK(GetEntryCount() != 0 || pending_entry_index_ == -1);
  return pending_entry_index_;
}

void NavigationControllerImpl::InsertOrReplaceEntry(
    std::unique_ptr<NavigationEntryImpl> entry,
    bool replace,
    bool was_post_commit_error) {
  DCHECK(!ui::PageTransitionCoreTypeIs(entry->GetTransitionType(),
                                       ui::PAGE_TRANSITION_AUTO_SUBFRAME));

  // If the pending_entry_index_ is -1, the navigation was to a new page, and we
  // need to keep continuity with the pending entry, so copy the pending entry's
  // unique ID to the committed entry. If the pending_entry_index_ isn't -1,
  // then the renderer navigated on its own, independent of the pending entry,
  // so don't copy anything.
  if (pending_entry_ && pending_entry_index_ == -1)
    entry->set_unique_id(pending_entry_->GetUniqueID());

  DiscardNonCommittedEntries();

  // When replacing, don't prune the forward history.
  if ((replace || was_post_commit_error) && entries_.size() > 0) {
    CopyReplacedNavigationEntryDataIfPreviouslyEmpty(
        entries_[last_committed_entry_index_].get(), entry.get());
    // If the new entry is a post-commit error page, we store the current last
    // committed entry to the side so that we can put it back when navigating
    // away from the error.
    if (was_post_commit_error) {
      DCHECK(!entry_replaced_by_post_commit_error_);
      entry_replaced_by_post_commit_error_ =
          std::move(entries_[last_committed_entry_index_]);
    }
    entries_[last_committed_entry_index_] = std::move(entry);
    return;
  }

  // We shouldn't see replace == true when there's no committed entries.
  DCHECK(!replace);

  PruneForwardEntries();

  PruneOldestSkippableEntryIfFull();

  entries_.push_back(std::move(entry));
  last_committed_entry_index_ = static_cast<int>(entries_.size()) - 1;
}

void NavigationControllerImpl::PruneOldestSkippableEntryIfFull() {
  if (entries_.size() < max_entry_count())
    return;

  DCHECK_EQ(max_entry_count(), entries_.size());
  DCHECK_GT(last_committed_entry_index_, 0);
  CHECK_EQ(pending_entry_index_, -1);

  int index = 0;
  if (base::FeatureList::IsEnabled(
          features::kHistoryManipulationIntervention)) {
    // Retrieve the oldest skippable entry.
    for (; index < GetEntryCount(); index++) {
      if (GetEntryAtIndex(index)->should_skip_on_back_forward_ui())
        break;
    }
  }

  // If there is no skippable entry or if it is the last committed entry then
  // fall back to pruning the oldest entry. It is not safe to prune the last
  // committed entry.
  if (index == GetEntryCount() || index == last_committed_entry_index_)
    index = 0;

  bool should_succeed = RemoveEntryAtIndex(index);
  DCHECK_EQ(true, should_succeed);

  NotifyPrunedEntries(this, index, 1);
}

void NavigationControllerImpl::NavigateToExistingPendingEntry(
    ReloadType reload_type,
    int sandboxed_source_frame_tree_node_id) {
  TRACE_EVENT0("navigation",
               "NavigationControllerImpl::NavigateToExistingPendingEntry");
  DCHECK(pending_entry_);
  DCHECK(IsInitialNavigation() || pending_entry_index_ != -1);
  if (pending_entry_index_ != -1) {
    // The pending entry may not be in entries_ if a post-commit error page is
    // showing.
    DCHECK(pending_entry_ == entries_[pending_entry_index_].get() ||
           pending_entry_ == entry_replaced_by_post_commit_error_.get());
  }
  DCHECK(!IsRendererDebugURL(pending_entry_->GetURL()));
  bool is_forced_reload = needs_reload_;
  needs_reload_ = false;
  FrameTreeNode* root = delegate_->GetFrameTree()->root();
  int nav_entry_id = pending_entry_->GetUniqueID();

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

    DiscardNonCommittedEntries();
    return;
  }

  // Compare FrameNavigationEntries to see which frames in the tree need to be
  // navigated.
  std::vector<std::unique_ptr<NavigationRequest>> same_document_loads;
  std::vector<std::unique_ptr<NavigationRequest>> different_document_loads;
  FindFramesToNavigate(root, reload_type, &same_document_loads,
                       &different_document_loads);

  if (same_document_loads.empty() && different_document_loads.empty()) {
    // We were unable to match any frames to navigate.  This can happen if a
    // history navigation targets a subframe that no longer exists
    // (https://crbug.com/705550). In this case, we need to update the current
    // history entry to the pending one but keep the main document loaded.  We
    // also need to ensure that observers are informed about the updated
    // current history entry (e.g., for greying out back/forward buttons), and
    // that renderer processes update their history offsets.  The easiest way
    // to do all that is to schedule a "redundant" same-document navigation in
    // the main frame.
    //
    // Note that we don't want to remove this history entry, as it might still
    // be valid later, since a frame that it's targeting may be recreated.
    //
    // TODO(alexmos, creis): This behavior isn't ideal, as the user would
    // need to repeat history navigations until finding the one that works.
    // Consider changing this behavior to keep looking for the first valid
    // history entry that finds frames to navigate.
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            root, pending_entry_, pending_entry_->GetFrameEntry(root),
            ReloadType::NONE /* reload_type */,
            true /* is_same_document_history_load */,
            false /* is_history_navigation_in_new_child */);
    if (!navigation_request) {
      // If this navigation cannot start, delete the pending NavigationEntry.
      DiscardPendingEntry(false);
      return;
    }
    same_document_loads.push_back(std::move(navigation_request));

    // Sanity check that we never take this branch for any kinds of reloads,
    // as those should've queued a different-document load in the main frame.
    DCHECK(!is_forced_reload);
    DCHECK_EQ(reload_type, ReloadType::NONE);
  }

  // If |sandboxed_source_frame_node_id| is valid, then track whether this
  // navigation affects any frame outside the frame's subtree.
  if (sandboxed_source_frame_tree_node_id !=
      FrameTreeNode::kFrameTreeNodeInvalidId) {
    bool navigates_inside_tree =
        DoesSandboxNavigationStayWithinSubtree(
            sandboxed_source_frame_tree_node_id, same_document_loads) &&
        DoesSandboxNavigationStayWithinSubtree(
            sandboxed_source_frame_tree_node_id, different_document_loads);
    // Count the navigations as web use counters so we can determine
    // the number of pages that trigger this.
    FrameTreeNode* sandbox_source_frame_tree_node =
        FrameTreeNode::GloballyFindByID(sandboxed_source_frame_tree_node_id);
    if (sandbox_source_frame_tree_node) {
      GetContentClient()->browser()->LogWebFeatureForCurrentPage(
          sandbox_source_frame_tree_node->current_frame_host(),
          navigates_inside_tree
              ? blink::mojom::WebFeature::kSandboxBackForwardStaysWithinSubtree
              : blink::mojom::WebFeature::
                    kSandboxBackForwardAffectsFramesOutsideSubtree);
    }

    // If the navigation occurred outside the tree discard it because
    // the sandboxed frame didn't have permission to navigate outside
    // its tree. If it is possible that the navigation is both inside and
    // outside the frame tree and we discard it entirely because we don't
    // want to end up in a history state that didn't exist before.
    if (base::FeatureList::IsEnabled(
            features::kHistoryPreventSandboxedNavigation) &&
        !navigates_inside_tree) {
      DiscardPendingEntry(false);
      return;
    }
  }

  // BackForwardCache:
  // Navigate immediately if the document is in the BackForwardCache.
  if (back_forward_cache_.GetEntry(nav_entry_id)) {
    TRACE_EVENT0("navigation", "BackForwardCache_CreateNavigationRequest");
    DCHECK_EQ(reload_type, ReloadType::NONE);
    auto navigation_request = CreateNavigationRequestFromEntry(
        root, pending_entry_, pending_entry_->GetFrameEntry(root),
        ReloadType::NONE, false /* is_same_document_history_load */,
        false /* is_history_navigation_in_new_child */);
    root->navigator().Navigate(std::move(navigation_request), ReloadType::NONE,
                               RestoreType::NONE);

    return;
  }

  // History navigation might try to reuse a specific BrowsingInstance, already
  // used by a page in the cache. To avoid having two different main frames that
  // live in the same BrowsingInstance, evict the all pages with this
  // BrowsingInstance from the cache.
  //
  // For example, take the following scenario:
  //
  // A1 = Some page on a.com
  // A2 = Some other page on a.com
  // B3 = An uncacheable page on b.com
  //
  // Then the following navigations occur:
  // A1->A2->B3->A1
  // On the navigation from B3 to A1, A2 will remain in the cache (B3 doesn't
  // take its place) and A1 will be created in the same BrowsingInstance (and
  // SiteInstance), as A2.
  //
  // If we didn't do anything, both A1 and A2 would remain alive in the same
  // BrowsingInstance/SiteInstance, which is unsupported by
  // RenderFrameHostManager::CommitPending(). To avoid this conundrum, we evict
  // A2 from the cache.
  if (pending_entry_->site_instance()) {
    back_forward_cache_.EvictFramesInRelatedSiteInstances(
        pending_entry_->site_instance());
  }

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;

  // It is not possible to delete the pending NavigationEntry while navigating
  // to it. Grab a reference to delay potential deletion until the end of this
  // function.
  std::unique_ptr<PendingEntryRef> pending_entry_ref = ReferencePendingEntry();

  // Send all the same document frame loads before the different document loads.
  for (auto& item : same_document_loads) {
    FrameTreeNode* frame = item->frame_tree_node();
    frame->navigator().Navigate(std::move(item), reload_type,
                                pending_entry_->restore_type());
  }
  for (auto& item : different_document_loads) {
    FrameTreeNode* frame = item->frame_tree_node();
    frame->navigator().Navigate(std::move(item), reload_type,
                                pending_entry_->restore_type());
  }

  in_navigate_to_pending_entry_ = false;
}

NavigationControllerImpl::HistoryNavigationAction
NavigationControllerImpl::DetermineActionForHistoryNavigation(
    FrameTreeNode* frame,
    ReloadType reload_type) {
  // Only active frames can navigate:
  // - If the frame is in pending deletion, the browser already committed to
  // destroying this RenderFrameHost. See https://crbug.com/930278.
  // - If the frame is in back-forward cache, it's not allowed to navigate as it
  // should remain frozen. Ignore the request and evict the document from
  // back-forward cache.
  //
  // If the frame is inactive, there's no need to recurse into subframes, which
  // should all be inactive as well.
  if (frame->current_frame_host()->IsInactiveAndDisallowReactivation())
    return HistoryNavigationAction::kStopLooking;

  // If there's no last committed entry, there is no previous history entry to
  // compare against, so fall back to a different-document load.  Note that we
  // should only reach this case for the root frame and not descend further
  // into subframes.
  if (!GetLastCommittedEntry()) {
    DCHECK(frame->IsMainFrame());
    return HistoryNavigationAction::kDifferentDocument;
  }

  // Reloads should result in a different-document load.  Note that reloads may
  // also happen via the |needs_reload_| mechanism where the reload_type is
  // NONE, so detect this by comparing whether we're going to the same
  // entry that we're currently on.  Similarly to above, only main frames
  // should reach this.  Note that subframes support reloads, but that's done
  // via a different path that doesn't involve FindFramesToNavigate (see
  // RenderFrameHost::Reload()).
  if (reload_type != ReloadType::NONE ||
      pending_entry_index_ == last_committed_entry_index_) {
    DCHECK(frame->IsMainFrame());
    return HistoryNavigationAction::kDifferentDocument;
  }

  // If there is no new FrameNavigationEntry for the frame, ignore the
  // load.  For example, this may happen when going back to an entry before a
  // frame was created.  Suppose we commit a same-document navigation that also
  // results in adding a new subframe somewhere in the tree.  If we go back,
  // the new subframe will be missing a FrameNavigationEntry in the previous
  // NavigationEntry, but we shouldn't delete or change what's loaded in
  // it.
  //
  // Note that in this case, there is no need to keep looking for navigations
  // in subframes, which would be missing FrameNavigationEntries as well.
  //
  // It's important to check this before checking |old_item| below, since both
  // might be null, and in that case we still shouldn't change what's loaded in
  // this frame.  Note that scheduling any loads assumes that |new_item| is
  // non-null.  See https://crbug.com/1088354.
  FrameNavigationEntry* new_item = pending_entry_->GetFrameEntry(frame);
  if (!new_item)
    return HistoryNavigationAction::kStopLooking;

  // If there is no old FrameNavigationEntry, schedule a different-document
  // load.
  //
  // TODO(creis): Store the last committed FrameNavigationEntry to use here,
  // rather than assuming the NavigationEntry has up to date info on subframes.
  // Note that this may require sharing FrameNavigationEntries between
  // NavigationEntries (https://crbug.com/373041) as a prerequisite, since
  // otherwise the stored FrameNavigationEntry might get stale (e.g., after
  // subframe navigations, or in the case where we don't find any frames to
  // navigate and ignore a back/forward navigation while moving to a different
  // NavigationEntry, as in https://crbug.com/705550).
  FrameNavigationEntry* old_item =
      GetLastCommittedEntry()->GetFrameEntry(frame);
  if (!old_item)
    return HistoryNavigationAction::kDifferentDocument;

  // If the new item is not in the same SiteInstance, schedule a
  // different-document load.  Newly restored items may not have a SiteInstance
  // yet, in which case it will be assigned on first commit.
  if (new_item->site_instance() &&
      new_item->site_instance() != old_item->site_instance())
    return HistoryNavigationAction::kDifferentDocument;

  // Schedule a different-document load if the current RenderFrameHost is not
  // live.  This case can happen for Ctrl+Back or after
  // a renderer crash.
  if (!frame->current_frame_host()->IsRenderFrameLive())
    return HistoryNavigationAction::kDifferentDocument;

  if (new_item->item_sequence_number() != old_item->item_sequence_number()) {
    // Same document loads happen if the previous item has the same document
    // sequence number but different item sequence number.
    if (new_item->document_sequence_number() ==
        old_item->document_sequence_number())
      return HistoryNavigationAction::kSameDocument;

    // Otherwise, if both item and document sequence numbers differ, this
    // should be a different document load.
    return HistoryNavigationAction::kDifferentDocument;
  }

  // If the item sequence numbers match, there is no need to navigate this
  // frame.  Keep looking for navigations in this frame's children.
  DCHECK_EQ(new_item->document_sequence_number(),
            old_item->document_sequence_number());
  return HistoryNavigationAction::kKeepLooking;
}

void NavigationControllerImpl::FindFramesToNavigate(
    FrameTreeNode* frame,
    ReloadType reload_type,
    std::vector<std::unique_ptr<NavigationRequest>>* same_document_loads,
    std::vector<std::unique_ptr<NavigationRequest>>* different_document_loads) {
  DCHECK(pending_entry_);
  FrameNavigationEntry* new_item = pending_entry_->GetFrameEntry(frame);

  auto action = DetermineActionForHistoryNavigation(frame, reload_type);

  if (action == HistoryNavigationAction::kSameDocument) {
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            frame, pending_entry_, new_item, reload_type,
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
  } else if (action == HistoryNavigationAction::kDifferentDocument) {
    std::unique_ptr<NavigationRequest> navigation_request =
        CreateNavigationRequestFromEntry(
            frame, pending_entry_, new_item, reload_type,
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
  } else if (action == HistoryNavigationAction::kStopLooking) {
    return;
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
  ui::PageTransition transition_type = params.transition_type;

  // Javascript URLs should not create NavigationEntries. All other navigations
  // do, including navigations to chrome renderer debug URLs.
  if (!params.url.SchemeIs(url::kJavaScriptScheme)) {
    std::unique_ptr<NavigationEntryImpl> entry =
        CreateNavigationEntryFromLoadParams(node, params, override_user_agent,
                                            should_replace_current_entry,
                                            has_user_gesture);
    // CreateNavigationEntryFromLoadParams() may modify the transition type.
    transition_type = entry->GetTransitionType();
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
    // Renderer-debug URLs won't go through NavigationThrottlers so we have to
    // check them explicitly. See bug 913334.
    if (GetContentClient()->browser()->ShouldBlockRendererDebugURL(
            params.url, browser_context_)) {
      DiscardPendingEntry(false);
      return;
    }

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
  ReloadType reload_type = params.reload_type;
  if (reload_type == ReloadType::NONE &&
      ShouldTreatNavigationAsReload(
          node, params.url, pending_entry_->GetVirtualURL(),
          params.base_url_for_data_url, params.transition_type,
          params.load_type ==
              NavigationController::LOAD_TYPE_HTTP_POST /* is_post */,
          false /* is_reload */, false /* is_navigation_to_existing_entry */,
          GetLastCommittedEntry())) {
    reload_type = ReloadType::NORMAL;
  }

  // navigation_ui_data should only be present for main frame navigations.
  DCHECK(node->IsMainFrame() || !params.navigation_ui_data);

  DCHECK(pending_entry_);
  std::unique_ptr<NavigationRequest> request =
      CreateNavigationRequestFromLoadParams(
          node, params, override_user_agent, should_replace_current_entry,
          has_user_gesture, NavigationDownloadPolicy(), reload_type,
          pending_entry_, pending_entry_->GetFrameEntry(node), transition_type);

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

  // This call does not support re-entrancy.  See http://crbug.com/347742.
  CHECK(!in_navigate_to_pending_entry_);
  in_navigate_to_pending_entry_ = true;

  // It is not possible to delete the pending NavigationEntry while navigating
  // to it. Grab a reference to delay potential deletion until the end of this
  // function.
  std::unique_ptr<PendingEntryRef> pending_entry_ref = ReferencePendingEntry();

  node->navigator().Navigate(std::move(request), reload_type,
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
    // The current frame is always a main frame. If IsInitialNavigation() is
    // true then there have been no navigations and any frames of this tab must
    // be in the same renderer process. If that has crashed then the only frame
    // that can be revived is the main frame.
    frame_tree_node->render_manager()
        ->InitializeMainRenderFrameForImmediateUse();
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
  if (!blob_url_loader_factory && params.url.SchemeIsBlob()) {
    // Resolve the blob URL in the storage partition associated with the target
    // frame. This is the storage partition the URL will be loaded in, and only
    // URLs that can be resolved by it should be able to access its data.
    blob_url_loader_factory = ChromeBlobStorageContext::URLLoaderFactoryForUrl(
        node->current_frame_host()->GetStoragePartition(), params.url);
  }

  std::unique_ptr<NavigationEntryImpl> entry;
  // extra_headers in params are \n separated; navigation entries want \r\n.
  std::string extra_headers_crlf;
  base::ReplaceChars(params.extra_headers, "\n", "\r\n", &extra_headers_crlf);

  // For subframes, create a pending entry with a corresponding frame entry.
  if (!node->IsMainFrame()) {
    if (GetLastCommittedEntry()) {
      // Create an identical NavigationEntry with a new FrameNavigationEntry for
      // the target subframe.
      entry = GetLastCommittedEntry()->Clone();
    } else {
      // If there's no last committed entry, create an entry for about:blank
      // with a subframe entry for our destination.
      // TODO(creis): Ensure this case can't exist in https://crbug.com/524208.
      entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
          GURL(url::kAboutBlankURL), params.referrer, params.initiator_origin,
          params.source_site_instance.get(), params.transition_type,
          params.is_renderer_initiated, extra_headers_crlf, browser_context_,
          blob_url_loader_factory, should_replace_current_entry,
          GetWebContents()));
    }

    entry->AddOrUpdateFrameEntry(
        node, -1, -1, nullptr,
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()),
        params.url, base::nullopt, params.referrer, params.initiator_origin,
        params.redirect_chain, PageState(), "GET", -1, blob_url_loader_factory,
        nullptr /* web_bundle_navigation_info */);
  } else {
    // Otherwise, create a pending entry for the main frame.
    entry = NavigationEntryImpl::FromNavigationEntry(CreateNavigationEntry(
        params.url, params.referrer, params.initiator_origin,
        params.source_site_instance.get(), params.transition_type,
        params.is_renderer_initiated, extra_headers_crlf, browser_context_,
        blob_url_loader_factory, should_replace_current_entry,
        GetWebContents()));
    entry->set_source_site_instance(
        static_cast<SiteInstanceImpl*>(params.source_site_instance.get()));
    entry->SetRedirectChain(params.redirect_chain);
  }

  // Set the FTN ID (only used in non-site-per-process, for tests).
  entry->set_frame_tree_node_id(node->frame_tree_node_id());
  entry->set_should_clear_history_list(params.should_clear_history_list);
  entry->SetIsOverridingUserAgent(override_user_agent);
  entry->set_has_user_gesture(has_user_gesture);
  entry->set_reload_type(params.reload_type);

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
    NavigationDownloadPolicy download_policy,
    ReloadType reload_type,
    NavigationEntryImpl* entry,
    FrameNavigationEntry* frame_entry,
    ui::PageTransition transition_type) {
  DCHECK_EQ(-1, GetIndexOfEntry(entry));
  DCHECK(frame_entry);
  // All renderer-initiated navigations must have an initiator_origin.
  DCHECK(!params.is_renderer_initiated || params.initiator_origin.has_value());

  GURL url_to_load;
  GURL virtual_url;
  base::Optional<url::Origin> origin_to_commit =
      frame_entry ? frame_entry->committed_origin() : base::nullopt;

  // For main frames, rewrite the URL if necessary and compute the virtual URL
  // that should be shown in the address bar.
  if (node->IsMainFrame()) {
    bool ignored_reverse_on_redirect = false;
    RewriteUrlForNavigation(params.url, browser_context_, &url_to_load,
                            &virtual_url, &ignored_reverse_on_redirect);

    // For DATA loads, override the virtual URL.
    if (params.load_type == LOAD_TYPE_DATA)
      virtual_url = params.virtual_url_for_data_url;

    if (virtual_url.is_empty())
      virtual_url = url_to_load;

    CHECK(virtual_url == entry->GetVirtualURL());

    // This is a LOG and not a CHECK/DCHECK as URL rewrite has non-deterministic
    // behavior: it is possible for two calls to RewriteUrlForNavigation to
    // return different results, leading to a different URL in the
    // NavigationRequest and FrameEntry. This will be fixed once we remove the
    // pending NavigationEntry, as we'll only make one call to
    // RewriteUrlForNavigation.
    VLOG_IF(1, (url_to_load != frame_entry->url()))
        << "NavigationRequest and FrameEntry have different URLs: "
        << url_to_load << " vs " << frame_entry->url();

    // TODO(clamy): In order to remove the pending NavigationEntry,
    // |virtual_url| and |ignored_reverse_on_redirect| should be stored in the
    // NavigationRequest.
  } else {
    url_to_load = params.url;
    virtual_url = params.url;
    CHECK(!frame_entry || url_to_load == frame_entry->url());
  }

  if (node->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this node is now preparing for
    // attaching an inner delegate.
    return nullptr;
  }

  if (!IsValidURLForNavigation(node->IsMainFrame(), virtual_url, url_to_load))
    return nullptr;

  if (!DoesURLMatchOriginForNavigation(url_to_load, origin_to_commit)) {
    DCHECK(false) << " url:" << url_to_load
                  << " origin:" << origin_to_commit.value();
    return nullptr;
  }

  // Determine if Previews should be used for the navigation.
  blink::PreviewsState previews_state =
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;
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

  mojom::NavigationType navigation_type =
      GetNavigationType(node->current_url(),  // old_url
                        url_to_load,          // new_url
                        reload_type,          // reload_type
                        entry,                // entry
                        *frame_entry,         // frame_entry
                        false);               // is_same_document_history_load

  // Create the NavigationParams based on |params|.

  bool is_view_source_mode = virtual_url.SchemeIs(kViewSourceScheme);

  // Update |download_policy| if the virtual URL is view-source. Why do this
  // now? Possibly the URL could be rewritten to a view-source via some URL
  // handler.
  if (is_view_source_mode)
    download_policy.SetDisallowed(NavigationDownloadType::kViewSource);

  const GURL& history_url_for_data_url =
      params.base_url_for_data_url.is_empty() ? GURL() : virtual_url;
  // Don't use |params.transition_type| as calling code may supply a different
  // value.
  mojom::CommonNavigationParamsPtr common_params =
      mojom::CommonNavigationParams::New(
          url_to_load, params.initiator_origin,
          blink::mojom::Referrer::New(params.referrer.url,
                                      params.referrer.policy),
          transition_type, navigation_type, download_policy,
          should_replace_current_entry, params.base_url_for_data_url,
          history_url_for_data_url, previews_state, navigation_start,
          params.load_type == LOAD_TYPE_HTTP_POST ? "POST" : "GET",
          params.post_data, network::mojom::SourceLocation::New(),
          params.started_from_context_menu, has_user_gesture,
          false /* has_text_fragment_token */, CreateInitiatorCSPInfo(),
          std::vector<int>(), params.href_translate,
          false /* is_history_navigation_in_new_child_frame */,
          params.input_start);

  mojom::CommitNavigationParamsPtr commit_params =
      mojom::CommitNavigationParams::New(
          frame_entry->committed_origin(), override_user_agent,
          params.redirect_chain,
          std::vector<network::mojom::URLResponseHeadPtr>(),
          std::vector<net::RedirectInfo>(),
          std::string() /* post_content_type */, common_params->url,
          common_params->method, params.can_load_local_resources,
          frame_entry->page_state(), entry->GetUniqueID(),
          entry->GetSubframeUniqueNames(node), true /* intended_as_new_entry */,
          -1 /* pending_history_list_offset */,
          params.should_clear_history_list ? -1 : GetLastCommittedEntryIndex(),
          params.should_clear_history_list ? 0 : GetEntryCount(),
          false /* was_discarded */, is_view_source_mode,
          params.should_clear_history_list, mojom::NavigationTiming::New(),
          base::nullopt /* appcache_host_id */,
          mojom::WasActivatedOption::kUnknown,
          base::UnguessableToken::Create() /* navigation_token */,
          std::vector<mojom::PrefetchedSignedExchangeInfoPtr>(),
#if defined(OS_ANDROID)
          std::string(), /* data_url_as_string */
#endif
          !params.is_renderer_initiated, /* is_browser_initiated */
          network::mojom::IPAddressSpace::kUnknown,
          GURL() /* web_bundle_physical_url */,
          GURL() /* base_url_override_for_web_bundle */,
          ukm::kInvalidSourceId /* document_ukm_source_id */,
          node->pending_frame_policy(),
          std::vector<std::string>() /* force_enabled_origin_trials */,
          false /* origin_isolated */,
          std::vector<
              network::mojom::WebClientHintsType>() /* enabled_client_hints */,
          false /* is_cross_browsing_instance */,
          std::vector<std::string>() /* forced_content_security_policies */,
          nullptr /* old_page_info */);
#if defined(OS_ANDROID)
  if (ValidateDataURLAsString(params.data_url_as_string)) {
    commit_params->data_url_as_string = params.data_url_as_string->data();
  }
#endif

  commit_params->was_activated = params.was_activated;

  // A form submission may happen here if the navigation is a renderer-initiated
  // form submission that took the OpenURL path.
  scoped_refptr<network::ResourceRequestBody> request_body = params.post_data;

  // extra_headers in params are \n separated; NavigationRequests want \r\n.
  std::string extra_headers_crlf;
  base::ReplaceChars(params.extra_headers, "\n", "\r\n", &extra_headers_crlf);

  auto navigation_request = NavigationRequest::CreateBrowserInitiated(
      node, std::move(common_params), std::move(commit_params),
      !params.is_renderer_initiated, params.initiator_routing_id,
      extra_headers_crlf, frame_entry, entry, request_body,
      params.navigation_ui_data ? params.navigation_ui_data->Clone() : nullptr,
      params.impression);
  navigation_request->set_from_download_cross_origin_redirect(
      params.from_download_cross_origin_redirect);
  return navigation_request;
}

std::unique_ptr<NavigationRequest>
NavigationControllerImpl::CreateNavigationRequestFromEntry(
    FrameTreeNode* frame_tree_node,
    NavigationEntryImpl* entry,
    FrameNavigationEntry* frame_entry,
    ReloadType reload_type,
    bool is_same_document_history_load,
    bool is_history_navigation_in_new_child_frame) {
  DCHECK(frame_entry);
  GURL dest_url = frame_entry->url();
  base::Optional<url::Origin> origin_to_commit =
      frame_entry->committed_origin();

  Referrer dest_referrer = frame_entry->referrer();
  if (reload_type == ReloadType::ORIGINAL_REQUEST_URL &&
      entry->GetOriginalRequestURL().is_valid() && !entry->GetHasPostData()) {
    // We may have been redirected when navigating to the current URL.
    // Use the URL the user originally intended to visit as signaled by the
    // ReloadType, if it's valid and if a POST wasn't involved; the latter
    // case avoids issues with sending data to the wrong page.
    dest_url = entry->GetOriginalRequestURL();
    dest_referrer = Referrer();
    origin_to_commit.reset();
  }

  if (frame_tree_node->render_manager()->is_attaching_inner_delegate()) {
    // Avoid starting any new navigations since this node is now preparing for
    // attaching an inner delegate.
    return nullptr;
  }

  if (!IsValidURLForNavigation(frame_tree_node->IsMainFrame(),
                               entry->GetVirtualURL(), dest_url)) {
    return nullptr;
  }

  if (!DoesURLMatchOriginForNavigation(dest_url, origin_to_commit)) {
    DCHECK(false) << " url:" << dest_url
                  << " origin:" << origin_to_commit.value();
    return nullptr;
  }

  // Determine if Previews should be used for the navigation.
  blink::PreviewsState previews_state =
      blink::PreviewsTypes::PREVIEWS_UNSPECIFIED;
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

  mojom::NavigationType navigation_type = GetNavigationType(
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
  mojom::CommonNavigationParamsPtr common_params =
      entry->ConstructCommonNavigationParams(
          *frame_entry, request_body, dest_url,
          blink::mojom::Referrer::New(dest_referrer.url, dest_referrer.policy),
          navigation_type, previews_state, navigation_start,
          base::TimeTicks() /* input_start */);
  common_params->is_history_navigation_in_new_child_frame =
      is_history_navigation_in_new_child_frame;

  // TODO(clamy): |intended_as_new_entry| below should always be false once
  // Reload no longer leads to this being called for a pending NavigationEntry
  // of index -1.
  mojom::CommitNavigationParamsPtr commit_params =
      entry->ConstructCommitNavigationParams(
          *frame_entry, common_params->url, origin_to_commit,
          common_params->method, entry->GetSubframeUniqueNames(frame_tree_node),
          GetPendingEntryIndex() == -1 /* intended_as_new_entry */,
          GetIndexOfEntry(entry), GetLastCommittedEntryIndex(), GetEntryCount(),
          frame_tree_node->pending_frame_policy());
  commit_params->post_content_type = post_content_type;

  return NavigationRequest::CreateBrowserInitiated(
      frame_tree_node, std::move(common_params), std::move(commit_params),
      !entry->is_renderer_initiated(),
      GlobalFrameRoutingId() /* initiator_routing_id */, entry->extra_headers(),
      frame_entry, entry, request_body, nullptr /* navigation_ui_data */,
      base::nullopt /* impression */);
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
  NotificationService::current()->Notify(NOTIFICATION_NAV_ENTRY_COMMITTED,
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

  UMA_HISTOGRAM_ENUMERATION("Navigation.LoadIfNecessaryType",
                            needs_reload_type_);

  // Calling Reload() results in ignoring state, and not loading.
  // Explicitly use NavigateToPendingEntry so that the renderer uses the
  // cached state.
  if (pending_entry_) {
    NavigateToExistingPendingEntry(ReloadType::NONE,
                                   FrameTreeNode::kFrameTreeNodeInvalidId);
  } else if (last_committed_entry_index_ != -1) {
    pending_entry_ = entries_[last_committed_entry_index_].get();
    pending_entry_index_ = last_committed_entry_index_;
    NavigateToExistingPendingEntry(ReloadType::NONE,
                                   FrameTreeNode::kFrameTreeNodeInvalidId);
  } else {
    // If there is something to reload, the successful reload will clear the
    // |needs_reload_| flag. Otherwise, just do it here.
    needs_reload_ = false;
  }
}

void NavigationControllerImpl::LoadPostCommitErrorPage(
    RenderFrameHost* render_frame_host,
    const GURL& url,
    const std::string& error_page_html,
    net::Error error) {
  // Only active frames can load post-commit error pages:
  // - If the frame is in pending deletion, the browser already committed to
  // destroying this RenderFrameHost so ignore loading the error page.
  // - If the frame is in back-forward cache, it's not allowed to navigate as it
  // should remain frozen. Ignore the request and evict the document from
  // back-forward cache.
  if (static_cast<RenderFrameHostImpl*>(render_frame_host)
          ->IsInactiveAndDisallowReactivation()) {
    return;
  }

  FrameTreeNode* node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();

  mojom::CommonNavigationParamsPtr common_params =
      CreateCommonNavigationParams();
  common_params->url = url;
  mojom::CommitNavigationParamsPtr commit_params =
      CreateCommitNavigationParams();

  // Error pages have a fully permissive FramePolicy.
  // TODO(arthursonzogni): Consider providing the minimal capabilities to the
  // error pages.
  commit_params->frame_policy = blink::FramePolicy();

  std::unique_ptr<NavigationRequest> navigation_request =
      NavigationRequest::CreateBrowserInitiated(
          node, std::move(common_params), std::move(commit_params),
          true /* browser_initiated */,
          GlobalFrameRoutingId() /* initiator_routing_id */,
          "" /* extra_headers */, nullptr /* frame_entry */,
          nullptr /* entry */, nullptr /* post_body */,
          nullptr /* navigation_ui_data */, base::nullopt /* impression */);
  navigation_request->set_post_commit_error_page_html(error_page_html);
  navigation_request->set_net_error(error);
  node->CreatedNavigationRequest(std::move(navigation_request));
  DCHECK(node->navigation_request());
  node->navigation_request()->BeginNavigation();
}

void NavigationControllerImpl::NotifyEntryChanged(NavigationEntry* entry) {
  EntryChangedDetails det;
  det.changed_entry = entry;
  det.index = GetIndexOfEntry(NavigationEntryImpl::FromNavigationEntry(entry));
  delegate_->NotifyNavigationEntryChanged(det);
}

void NavigationControllerImpl::FinishRestore(int selected_index,
                                             RestoreType type) {
  DCHECK(selected_index >= 0 && selected_index < GetEntryCount());
  ConfigureEntriesForRestore(&entries_, type);

  last_committed_entry_index_ = selected_index;
}

void NavigationControllerImpl::DiscardNonCommittedEntries() {
  // Avoid sending a notification if there is nothing to discard.
  // TODO(mthiesse): Temporarily checking failed_pending_entry_id_ to help
  // diagnose https://bugs.chromium.org/p/chromium/issues/detail?id=1007570.
  if (!pending_entry_ && failed_pending_entry_id_ == 0) {
    return;
  }
  DiscardPendingEntry(false);
  if (delegate_)
    delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_ALL);
}

int NavigationControllerImpl::GetEntryIndexWithUniqueID(
    int nav_entry_id) const {
  for (int i = static_cast<int>(entries_.size()) - 1; i >= 0; --i) {
    if (entries_[i]->GetUniqueID() == nav_entry_id)
      return i;
  }
  return -1;
}

void NavigationControllerImpl::InsertEntriesFrom(
    NavigationControllerImpl* source,
    int max_index) {
  DCHECK_LE(max_index, source->GetEntryCount());
  size_t insert_index = 0;
  for (int i = 0; i < max_index; i++) {
    // When cloning a tab, copy all entries except interstitial pages.
    if (source->entries_[i]->GetPageType() != PAGE_TYPE_INTERSTITIAL) {
      // TODO(creis): Once we start sharing FrameNavigationEntries between
      // NavigationEntries, it will not be safe to share them with another tab.
      // Must have a version of Clone that recreates them.
      entries_.insert(entries_.begin() + insert_index++,
                      source->entries_[i]->Clone());
    }
  }
  DCHECK(pending_entry_index_ == -1 ||
         pending_entry_ == GetEntryAtIndex(pending_entry_index_));
}

void NavigationControllerImpl::SetGetTimestampCallbackForTest(
    const base::RepeatingCallback<base::Time()>& get_timestamp_callback) {
  get_timestamp_callback_ = get_timestamp_callback;
}

// History manipulation intervention:
void NavigationControllerImpl::SetShouldSkipOnBackForwardUIIfNeeded(
    bool replace_entry,
    bool previous_document_was_activated,
    bool is_renderer_initiated,
    ukm::SourceId previous_page_load_ukm_source_id) {
  // Note that for a subframe, previous_document_was_activated is true if the
  // gesture happened in any subframe (propagated to main frame) or in the main
  // frame itself.
  if (replace_entry || previous_document_was_activated ||
      !is_renderer_initiated) {
    if (last_committed_entry_index_ != -1) {
      // This histogram always counts when navigating away from an entry,
      // irrespective of whether the skippable flag was changed or not, and
      // whether this entry is being reused or not.
      UMA_HISTOGRAM_BOOLEAN(
          "Navigation.BackForward.SetShouldSkipOnBackForwardUI",
          GetEntryAtIndex(last_committed_entry_index_)
              ->should_skip_on_back_forward_ui());
    }
    return;
  }
  if (last_committed_entry_index_ == -1)
    return;

  SetSkippableForSameDocumentEntries(last_committed_entry_index_, true);
  UMA_HISTOGRAM_BOOLEAN("Navigation.BackForward.SetShouldSkipOnBackForwardUI",
                        true);

  // Log UKM with the URL we are navigating away from.
  ukm::builders::HistoryManipulationIntervention(
      previous_page_load_ukm_source_id)
      .Record(ukm::UkmRecorder::Get());
}

void NavigationControllerImpl::SetSkippableForSameDocumentEntries(
    int reference_index,
    bool skippable) {
  auto* reference_entry = GetEntryAtIndex(reference_index);
  reference_entry->set_should_skip_on_back_forward_ui(skippable);

  int64_t document_sequence_number =
      reference_entry->root_node()->frame_entry->document_sequence_number();
  for (int index = 0; index < GetEntryCount(); index++) {
    auto* entry = GetEntryAtIndex(index);
    if (entry->root_node()->frame_entry->document_sequence_number() ==
        document_sequence_number) {
      entry->set_should_skip_on_back_forward_ui(skippable);
    }
  }
}

std::unique_ptr<NavigationControllerImpl::PendingEntryRef>
NavigationControllerImpl::ReferencePendingEntry() {
  DCHECK(pending_entry_);
  auto pending_entry_ref =
      std::make_unique<PendingEntryRef>(weak_factory_.GetWeakPtr());
  pending_entry_refs_.insert(pending_entry_ref.get());
  return pending_entry_ref;
}

void NavigationControllerImpl::PendingEntryRefDeleted(PendingEntryRef* ref) {
  // Ignore refs that don't correspond to the current pending entry.
  auto it = pending_entry_refs_.find(ref);
  if (it == pending_entry_refs_.end())
    return;
  pending_entry_refs_.erase(it);

  if (!pending_entry_refs_.empty())
    return;

  // The pending entry may be deleted before the last PendingEntryRef.
  if (!pending_entry_)
    return;

  // We usually clear the pending entry when the matching NavigationRequest
  // fails, so that an arbitrary URL isn't left visible above a committed page.
  //
  // However, we do preserve the pending entry in some cases, such as on the
  // initial navigation of an unmodified blank tab. We also allow the delegate
  // to say when it's safe to leave aborted URLs in the omnibox, to let the
  // user edit the URL and try again. This may be useful in cases that the
  // committed page cannot be attacker-controlled. In these cases, we still
  // allow the view to clear the pending entry and typed URL if the user
  // requests (e.g., hitting Escape with focus in the address bar).
  //
  // Do not leave the pending entry visible if it has an invalid URL, since this
  // might be formatted in an unexpected or unsafe way.
  // TODO(creis): Block navigations to invalid URLs in https://crbug.com/850824.
  bool should_preserve_entry =
      (pending_entry_ == GetVisibleEntry()) &&
      pending_entry_->GetURL().is_valid() &&
      (IsUnmodifiedBlankTab() || delegate_->ShouldPreserveAbortedURLs());
  if (should_preserve_entry)
    return;

  DiscardPendingEntry(true);
  delegate_->NotifyNavigationStateChanged(INVALIDATE_TYPE_URL);
}

}  // namespace content
