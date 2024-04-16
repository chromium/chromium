// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/navigation_controller_impl.h"

#include <fuchsia/mem/cpp/fidl.h>
#include <lib/fpromise/result.h>

#include <string_view>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/page_size.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
#include "components/favicon/content/content_favicon_driver.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "fuchsia_web/common/string_util.h"
#include "fuchsia_web/webengine/browser/trace_event.h"
#include "net/base/net_errors.h"
#include "net/http/http_util.h"
#include "third_party/blink/public/mojom/navigation/was_activated_option.mojom.h"
#include "third_party/perfetto/include/perfetto/tracing/track_event_args.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/image/image.h"

namespace {

// Converts a gfx::Image to a fuchsia::web::Favicon.
fuchsia::web::Favicon GfxImageToFidlFavicon(gfx::Image gfx_image) {
  fuchsia::web::Favicon favicon;

  if (gfx_image.IsEmpty())
    return favicon;

  int height = gfx_image.AsBitmap().pixmap().height();
  int width = gfx_image.AsBitmap().pixmap().width();

  size_t stride = width * SkColorTypeBytesPerPixel(kRGBA_8888_SkColorType);

  // Create VMO.
  fuchsia::mem::Buffer buffer;
  buffer.size = stride * height;
  zx_status_t status = zx::vmo::create(buffer.size, 0, &buffer.vmo);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmo_create";

  // Map the VMO.
  uintptr_t addr;
  size_t mapped_size = base::bits::AlignUp(buffer.size, base::GetPageSize());
  zx_vm_option_t options = ZX_VM_PERM_READ | ZX_VM_PERM_WRITE;
  status = zx::vmar::root_self()->map(options, /*vmar_offset=*/0, buffer.vmo,
                                      /*vmo_offset=*/0, mapped_size, &addr);
  ZX_CHECK(status == ZX_OK, status) << "zx_vmar_map";

  // Copy the data to the mapped VMO.
  gfx_image.AsBitmap().readPixels(
      SkImageInfo::Make(width, height, kRGBA_8888_SkColorType,
                        kPremul_SkAlphaType),
      reinterpret_cast<void*>(addr), stride, 0, 0);

  // Unmap the VMO.
  status = zx::vmar::root_self()->unmap(addr, mapped_size);
  ZX_DCHECK(status == ZX_OK, status) << "zx_vmar_unmap";

  favicon.set_data(std::move(buffer));
  favicon.set_height(height);
  favicon.set_width(width);

  return favicon;
}

}  // namespace

namespace {

// For each field that differs between |old_entry| and |new_entry|, the field
// is set to its new value in |difference|. All other fields in |difference| are
// left unchanged, such that a series of DiffNavigationEntries() calls may be
// used to accumulate differences across a progression of NavigationStates.
void DiffNavigationEntries(const fuchsia::web::NavigationState& old_entry,
                           const fuchsia::web::NavigationState& new_entry,
                           fuchsia::web::NavigationState* difference) {
  DCHECK(difference);

  // NavigationStates will only be empty for "initial" navigation entries, so
  // if |new_entry| is empty then |old_entry| must necessarily also be empty,
  // and there is no difference to report.
  if (new_entry.IsEmpty()) {
    CHECK(old_entry.IsEmpty());
    return;
  }

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

}  // namespace

NavigationControllerImpl::NavigationControllerImpl(
    content::WebContents* web_contents,
    void* parent_for_trace_flow)
    : parent_for_trace_flow_(parent_for_trace_flow),
      web_contents_(web_contents),
      weak_factory_(this) {
  DCHECK(parent_for_trace_flow_);

  Observe(web_contents_);
}

NavigationControllerImpl::~NavigationControllerImpl() = default;

void NavigationControllerImpl::AddBinding(
    fidl::InterfaceRequest<fuchsia::web::NavigationController> controller) {
  controller_bindings_.AddBinding(this, std::move(controller));
}

void NavigationControllerImpl::SetEventListener(
    fidl::InterfaceHandle<fuchsia::web::NavigationEventListener> listener,
    fuchsia::web::NavigationEventListenerFlags flags) {
  // Reset the event buffer state.
  waiting_for_navigation_event_ack_ = false;
  previous_navigation_state_ = {};
  pending_navigation_event_ = {};

  // Simply unbind if no new listener was set.
  if (!listener) {
    navigation_listener_.Unbind();
    return;
  }

  send_favicon_ =
      (flags & fuchsia::web::NavigationEventListenerFlags::FAVICON) ==
      fuchsia::web::NavigationEventListenerFlags::FAVICON;

  favicon::ContentFaviconDriver* favicon_driver =
      favicon::ContentFaviconDriver::FromWebContents(web_contents_);
  if (send_favicon_) {
    if (!favicon_driver) {
      favicon::ContentFaviconDriver::CreateForWebContents(
          web_contents_,
          /*favicon_service=*/nullptr);
      favicon_driver =
          favicon::ContentFaviconDriver::FromWebContents(web_contents_);
    }
    favicon_driver->AddObserver(this);
  } else {
    if (favicon_driver)
      favicon_driver->RemoveObserver(this);
  }

  navigation_listener_.Bind(std::move(listener));
  navigation_listener_.set_error_handler(
      [this](zx_status_t status) { SetEventListener(nullptr, {}); });

  // Send the current navigation state to the listener immediately.
  waiting_for_navigation_event_ack_ = true;
  previous_navigation_state_ = GetVisibleNavigationState();
  fuchsia::web::NavigationState initial_state;
  DiffNavigationEntries({}, previous_navigation_state_, &initial_state);
  navigation_listener_->OnNavigationStateChanged(
      std::move(initial_state), [this]() {
        waiting_for_navigation_event_ack_ = false;
        MaybeSendNavigationEvent();
      });
}

fuchsia::web::NavigationState
NavigationControllerImpl::GetVisibleNavigationState() const {
  content::NavigationEntry* const entry =
      web_contents_->GetController().GetVisibleEntry();
  if (entry->IsInitialEntry())
    return fuchsia::web::NavigationState();

  fuchsia::web::NavigationState state;

  // Populate some fields directly from the NavigationEntry, if possible.
  state.set_title(base::UTF16ToUTF8(entry->GetTitleForDisplay()));
  state.set_url(entry->GetURL().spec());

  if (web_contents_->IsCrashed()) {
    // TODO(https:://crbug.com/1092506): Add an explicit crashed indicator to
    // NavigationState, separate from PageType::ERROR.
    state.set_page_type(fuchsia::web::PageType::ERROR);
  } else if (uncommitted_load_error_) {
    // If there was a loading error which prevented the navigation entry from
    // being committed, then report PageType::ERROR.
    state.set_page_type(fuchsia::web::PageType::ERROR);
  } else {
    switch (entry->GetPageType()) {
      case content::PageType::PAGE_TYPE_NORMAL:
        state.set_page_type(fuchsia::web::PageType::NORMAL);
        break;
      case content::PageType::PAGE_TYPE_ERROR:
        state.set_page_type(fuchsia::web::PageType::ERROR);
        break;
    }
  }

  state.set_is_main_document_loaded(is_main_document_loaded_);
  state.set_can_go_back(web_contents_->GetController().CanGoBack());
  state.set_can_go_forward(web_contents_->GetController().CanGoForward());

  return state;
}

void NavigationControllerImpl::OnNavigationEntryChanged() {
  fuchsia::web::NavigationState new_state = GetVisibleNavigationState();
  DiffNavigationEntries(previous_navigation_state_, new_state,
                        &pending_navigation_event_);
  previous_navigation_state_ = std::move(new_state);

  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
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

  // Note that the events is logged to the parent Frame's flow.
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/NavigationEventListener.OnNavigationStateChanged",
              perfetto::Flow::FromPointer(parent_for_trace_flow_), "url",
              previous_navigation_state_.url(), "title",
              previous_navigation_state_.title().data(), "is_loaded",
              is_main_document_loaded_);

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
  // Note that the event is logged to the parent Frame's flow.
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/NavigationController.LoadUrl",
              perfetto::Flow::FromPointer(parent_for_trace_flow_), "url", url);

  GURL validated_url(url);
  if (!validated_url.is_valid()) {
    callback(
        fpromise::error(fuchsia::web::NavigationControllerError::INVALID_URL));
    return;
  }

  content::NavigationController::LoadURLParams params_converted(validated_url);
  if (params.has_headers()) {
    std::vector<std::string> extra_headers;
    extra_headers.reserve(params.headers().size());
    for (const auto& header : params.headers()) {
      std::string_view header_name = BytesAsString(header.name);
      std::string_view header_value = BytesAsString(header.value);
      if (!net::HttpUtil::IsValidHeaderName(header_name) ||
          !net::HttpUtil::IsValidHeaderValue(header_value)) {
        callback(fpromise::error(
            fuchsia::web::NavigationControllerError::INVALID_HEADER));
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
    params_converted.was_activated = blink::mojom::WasActivatedOption::kYes;
  } else {
    params_converted.was_activated = blink::mojom::WasActivatedOption::kNo;
  }

  web_contents_->GetController().LoadURLWithParams(params_converted);
  callback(fpromise::ok());
}

void NavigationControllerImpl::GoBack() {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/NavigationController.GoBack",
              perfetto::Flow::FromPointer(parent_for_trace_flow_));

  if (web_contents_->GetController().CanGoBack())
    web_contents_->GetController().GoBack();
}

void NavigationControllerImpl::GoForward() {
  TRACE_EVENT(kWebEngineFidlCategory,
              "fuchsia.web/NavigationController.GoForward",
              perfetto::Flow::FromPointer(parent_for_trace_flow_));

  if (web_contents_->GetController().CanGoForward())
    web_contents_->GetController().GoForward();
}

void NavigationControllerImpl::Stop() {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/NavigationController.Stop",
              perfetto::Flow::FromPointer(parent_for_trace_flow_));

  web_contents_->Stop();
}

void NavigationControllerImpl::Reload(fuchsia::web::ReloadType type) {
  TRACE_EVENT(kWebEngineFidlCategory, "fuchsia.web/NavigationController.Reload",
              perfetto::Flow::FromPointer(parent_for_trace_flow_));

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

void NavigationControllerImpl::TitleWasSet(content::NavigationEntry* entry) {
  // The title was changed after the document was loaded.
  OnNavigationEntryChanged();
}

void NavigationControllerImpl::PrimaryMainDocumentElementAvailable() {
  // The main document is loaded, but not necessarily all the subresources. Some
  // fields like "title" will change here.

  OnNavigationEntryChanged();
}

void NavigationControllerImpl::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  // The current document and its statically-declared subresources are loaded.

  // Don't process load completion on the current document if the WebContents
  // is already in the process of navigating to a different page.
  if (active_navigation_)
    return;

  // Only allow the primary main frame to transition this state.
  if (!render_frame_host->IsInPrimaryMainFrame())
    return;

  is_main_document_loaded_ = true;
  OnNavigationEntryChanged();
}

void NavigationControllerImpl::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // If the current RenderProcess terminates then trigger a NavigationState
  // change to let the caller know that something is wrong.
  LOG(WARNING) << "RenderProcess gone, TerminationStatus=" << status;
  OnNavigationEntryChanged();
}

void NavigationControllerImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  // If favicons are enabled then reset favicon in the pending navigation.
  if (send_favicon_)
    pending_navigation_event_.set_favicon({});

  uncommitted_load_error_ = false;
  active_navigation_ = navigation_handle;
  is_main_document_loaded_ = false;
  OnNavigationEntryChanged();
}

void NavigationControllerImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle != active_navigation_)
    return;

  active_navigation_ = nullptr;
  uncommitted_load_error_ = !navigation_handle->HasCommitted() &&
                            navigation_handle->GetNetErrorCode() != net::OK;

  OnNavigationEntryChanged();
}

void NavigationControllerImpl::OnFaviconUpdated(
    favicon::FaviconDriver* favicon_driver,
    NotificationIconType notification_icon_type,
    const GURL& icon_url,
    bool icon_url_changed,
    const gfx::Image& image) {
  // Currently FaviconDriverImpl loads only 16 DIP images, except on Android and
  // iOS.
  DCHECK_EQ(notification_icon_type, FaviconDriverObserver::NON_TOUCH_16_DIP);

  pending_navigation_event_.set_favicon(GfxImageToFidlFavicon(image));

  OnNavigationEntryChanged();
}

void DiffNavigationEntriesForTest(  // IN-TEST
    const fuchsia::web::NavigationState& old_entry,
    const fuchsia::web::NavigationState& new_entry,
    fuchsia::web::NavigationState* difference) {
  DiffNavigationEntries(old_entry, new_entry, difference);
}
