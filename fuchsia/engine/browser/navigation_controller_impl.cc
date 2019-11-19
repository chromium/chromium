// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/navigation_controller_impl.h"

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/was_activated_option.mojom.h"
#include "fuchsia/base/string_util.h"
#include "net/http/http_util.h"
#include "ui/base/page_transition_types.h"

namespace {

void UpdateNavigationStateFromNavigationEntry(
    content::NavigationEntry* entry,
    content::WebContents* web_contents,
    fuchsia::web::NavigationState* navigation_state) {
  DCHECK(entry);
  DCHECK(web_contents);
  DCHECK(navigation_state);

  navigation_state->set_title(base::UTF16ToUTF8(entry->GetTitleForDisplay()));
  navigation_state->set_url(entry->GetURL().spec());

  switch (entry->GetPageType()) {
    case content::PageType::PAGE_TYPE_NORMAL:
    case content::PageType::PAGE_TYPE_INTERSTITIAL:
      navigation_state->set_page_type(fuchsia::web::PageType::NORMAL);
      break;
    case content::PageType::PAGE_TYPE_ERROR:
      navigation_state->set_page_type(fuchsia::web::PageType::ERROR);
      break;
  }

  navigation_state->set_can_go_back(web_contents->GetController().CanGoBack());
  navigation_state->set_can_go_forward(
      web_contents->GetController().CanGoForward());
}

}  // namespace

NavigationControllerImpl::NavigationControllerImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents), weak_factory_(this) {
  Observe(web_contents_);
}

NavigationControllerImpl::~NavigationControllerImpl() = default;

void NavigationControllerImpl::AddBinding(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  controller_bindings_.AddBinding(this, std::move(controller));
}

void NavigationControllerImpl::SetEventListener(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener) {
  // Reset the event buffer state.
  waiting_for_navigation_event_ack_ = false;
  previous_navigation_state_ = {};
  pending_navigation_event_ = {};

  // Simply unbind if no new listener was set.
  if (!listener) {
    navigation_listener_.Unbind();
    return;
  }

  navigation_listener_.Bind(std::move(listener));
  navigation_listener_.set_error_handler(
      [this](zx_status_t status) { SetEventListener(nullptr); });

  // Immediately send the current navigation state, even if it is empty.
  if (web_contents_->GetController().GetVisibleEntry() == nullptr) {
    waiting_for_navigation_event_ack_ = true;
    navigation_listener_->OnNavigationStateChanged(
        fuchsia::web::NavigationState(), [this]() {
          waiting_for_navigation_event_ack_ = false;
          MaybeSendNavigationEvent();
        });
  } else {
    OnNavigationEntryChanged();
  }
}

void NavigationControllerImpl::OnNavigationEntryChanged() {
  fuchsia::web::NavigationState new_state;
  new_state.set_is_main_document_loaded(is_main_document_loaded_);
  UpdateNavigationStateFromNavigationEntry(
      web_contents_->GetController().GetVisibleEntry(), web_contents_,
      &new_state);

  DiffNavigationEntries(previous_navigation_state_, new_state,
                        &pending_navigation_event_);
  previous_navigation_state_ = std::move(new_state);

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&NavigationControllerImpl::MaybeSendNavigationEvent,
                     weak_factory_.GetWeakPtr()));
}

void NavigationControllerImpl::MaybeSendNavigationEvent() {
  if (!navigation_listener_)
    return;

  if (pending_navigation_event_.IsEmpty() ||
      waiting_for_navigation_event_ack_) {
    return;
  }

  waiting_for_navigation_event_ack_ = true;

  // Send the event to the observer and, upon acknowledgement, revisit this
  // function to send another update.
  navigation_listener_->OnNavigationStateChanged(
      std::move(pending_navigation_event_), [this]() {
        waiting_for_navigation_event_ack_ = false;
        MaybeSendNavigationEvent();
      });

  pending_navigation_event_ = {};
}

void NavigationControllerImpl::LoadUrl(std::string url,
                                       fuchsia::web::LoadUrlParams params,
                                       LoadUrlCallback callback) {
  fuchsia::web::NavigationController_LoadUrl_Result result;
  GURL validated_url(url);
  if (!validated_url.is_valid()) {
    result.set_err(fuchsia::web::NavigationControllerError::INVALID_URL);
    callback(std::move(result));
    return;
  }

  content::NavigationController::LoadURLParams params_converted(validated_url);
  if (params.has_headers()) {
    std::vector<std::string> extra_headers;
    extra_headers.reserve(params.headers().size());
    for (const auto& header : params.headers()) {
      base::StringPiece header_name = cr_fuchsia::BytesAsString(header.name);
      base::StringPiece header_value = cr_fuchsia::BytesAsString(header.value);
      if (!net::HttpUtil::IsValidHeaderName(header_name) ||
          !net::HttpUtil::IsValidHeaderValue(header_value)) {
        result.set_err(fuchsia::web::NavigationControllerError::INVALID_HEADER);
        callback(std::move(result));
        return;
      }

      extra_headers.emplace_back(
          base::StrCat({header_name, ": ", header_value}));
    }
    params_converted.extra_headers = base::JoinString(extra_headers, "\n");
  }

  if (validated_url.scheme() == url::kDataScheme)
    params_converted.load_type = content::NavigationController::LOAD_TYPE_DATA;

  params_converted.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  if (params.has_was_user_activated() && params.was_user_activated()) {
    params_converted.was_activated = content::mojom::WasActivatedOption::kYes;
  } else {
    params_converted.was_activated = content::mojom::WasActivatedOption::kNo;
  }

  web_contents_->GetController().LoadURLWithParams(params_converted);
  result.set_response(fuchsia::web::NavigationController_LoadUrl_Response());
  callback(std::move(result));
}

void NavigationControllerImpl::GoBack() {
  if (web_contents_->GetController().CanGoBack())
    web_contents_->GetController().GoBack();
}

void NavigationControllerImpl::GoForward() {
  if (web_contents_->GetController().CanGoForward())
    web_contents_->GetController().GoForward();
}

void NavigationControllerImpl::Stop() {
  web_contents_->Stop();
}

void NavigationControllerImpl::Reload(fuchsia::web::ReloadType type) {
  content::ReloadType internal_reload_type;
  switch (type) {
    case fuchsia::web::ReloadType::PARTIAL_CACHE:
      internal_reload_type = content::ReloadType::NORMAL;
      break;
    case fuchsia::web::ReloadType::NO_CACHE:
      internal_reload_type = content::ReloadType::BYPASSING_CACHE;
      break;
  }
  web_contents_->GetController().Reload(internal_reload_type, false);
}

void NavigationControllerImpl::GetVisibleEntry(
    fuchsia::web::NavigationController::GetVisibleEntryCallback callback) {
  content::NavigationEntry* entry =
      web_contents_->GetController().GetVisibleEntry();
  if (!entry) {
    callback({});
    return;
  }

  fuchsia::web::NavigationState state;
  state.set_is_main_document_loaded(is_main_document_loaded_);
  UpdateNavigationStateFromNavigationEntry(entry, web_contents_, &state);
  callback(std::move(state));
}

void NavigationControllerImpl::TitleWasSet(content::NavigationEntry* entry) {
  // The title was changed after the document was loaded.
  OnNavigationEntryChanged();
}

void NavigationControllerImpl::DocumentAvailableInMainFrame() {
  // The main document is loaded, but not necessarily all the subresources. Some
  // fields like "title" will change here.

  OnNavigationEntryChanged();
}

void NavigationControllerImpl::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // The document and its statically-declared subresources are loaded.
  is_main_document_loaded_ = true;
  OnNavigationEntryChanged();
}

void NavigationControllerImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle->IsSameDocument())
    return;

  is_main_document_loaded_ = false;
  OnNavigationEntryChanged();
}

void DiffNavigationEntries(const fuchsia::web::NavigationState& old_entry,
                           const fuchsia::web::NavigationState& new_entry,
                           fuchsia::web::NavigationState* difference) {
  DCHECK(difference);

  DCHECK(new_entry.has_title());
  if (!old_entry.has_title() || (new_entry.title() != old_entry.title())) {
    difference->set_title(new_entry.title());
  }

  DCHECK(new_entry.has_url());
  if (!old_entry.has_url() || (new_entry.url() != old_entry.url())) {
    difference->set_url(new_entry.url());
  }

  DCHECK(new_entry.has_page_type());
  if (!old_entry.has_page_type() ||
      (new_entry.page_type() != old_entry.page_type())) {
    difference->set_page_type(new_entry.page_type());
  }

  DCHECK(new_entry.has_can_go_back());
  if (!old_entry.has_can_go_back() ||
      old_entry.can_go_back() != new_entry.can_go_back()) {
    difference->set_can_go_back(new_entry.can_go_back());
  }

  DCHECK(new_entry.has_can_go_forward());
  if (!old_entry.has_can_go_forward() ||
      old_entry.can_go_forward() != new_entry.can_go_forward()) {
    difference->set_can_go_forward(new_entry.can_go_forward());
  }

  DCHECK(new_entry.has_is_main_document_loaded());
  if (!old_entry.has_is_main_document_loaded() ||
      old_entry.is_main_document_loaded() !=
          new_entry.is_main_document_loaded()) {
    difference->set_is_main_document_loaded(
        new_entry.is_main_document_loaded());
  }
}
