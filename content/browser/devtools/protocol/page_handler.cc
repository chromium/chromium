// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/protocol/page_handler.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/numerics/safe_conversions.h"
#include "base/process/process_handle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/to_string.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/back_forward_cache/disabled_reason_id.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/devtools/devtools_agent_host_impl.h"
#include "content/browser/devtools/protocol/browser_handler.h"
#include "content/browser/devtools/protocol/devtools_mhtml_helper.h"
#include "content/browser/devtools/protocol/emulation_handler.h"
#include "content/browser/devtools/protocol/handler_helpers.h"
#include "content/browser/manifest/manifest_manager_host.h"
#include "content/browser/preloading/prerender/prerender_final_status.h"
#include "content/browser/renderer_host/back_forward_cache_can_store_document_result.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/back_forward_cache_metrics.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/navigation_entry_impl.h"
#include "content/browser/renderer_host/navigation_request.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/common/referrer.h"
#include "content/public/common/result_codes.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/url_utils.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/manifest/manifest_util.h"
#include "third_party/blink/public/mojom/back_forward_cache_not_restored_reasons.mojom.h"
#include "third_party/blink/public/mojom/frame/back_forward_cache_controller.mojom.h"
#include "third_party/blink/public/mojom/manifest/manifest.mojom.h"
#include "third_party/blink/public/mojom/script_source_location.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/page_transition_types.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/codec/webp_codec.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/skbitmap_operations.h"
#include "ui/snapshot/snapshot.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_ANDROID)
#include "content/browser/renderer_host/compositor_impl_android.h"
#endif

namespace content {
namespace protocol {

namespace {

constexpr const char* kMhtml = "mhtml";
constexpr int kDefaultScreenshotQuality = 80;
constexpr int kMaxScreencastFramesInFlight = 2;
constexpr char kCommandIsOnlyAvailableAtTopTarget[] =
    "Command can only be executed on top-level targets";
constexpr char kErrorNotAttached[] = "Not attached to a page";
constexpr char kErrorInactivePage[] = "Not attached to an active page";

using BitmapEncoder =
    base::RepeatingCallback<bool(const SkBitmap& bitmap,
                                 std::vector<uint8_t>& output)>;

bool EncodeBitmapAsPngSlow(const SkBitmap& bitmap,
                           std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsPngSlow");
  return gfx::PNGCodec::EncodeBGRASkBitmap(bitmap, false, &output);
}

bool EncodeBitmapAsPngFast(const SkBitmap& bitmap,
                           std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsPngFast");
  return gfx::PNGCodec::FastEncodeBGRASkBitmap(bitmap, false, &output);
}

bool EncodeBitmapAsJpeg(int quality,
                        const SkBitmap& bitmap,
                        std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsJpeg");
  return gfx::JPEGCodec::Encode(bitmap, quality, &output);
}

bool EncodeBitmapAsWebp(int quality,
                        const SkBitmap& bitmap,
                        std::vector<uint8_t>& output) {
  TRACE_EVENT0("devtools", "EncodeBitmapAsWebp");
  return gfx::WebpCodec::Encode(bitmap, quality, &output);
}

absl::variant<protocol::Response, BitmapEncoder>
GetEncoder(const std::string& format, int quality, bool optimize_for_speed) {
  if (quality < 0 || quality > 100)
    quality = kDefaultScreenshotQuality;

  if (format == protocol::Page::CaptureScreenshot::FormatEnum::Png) {
    return base::BindRepeating(optimize_for_speed ? EncodeBitmapAsPngFast
                                                  : EncodeBitmapAsPngSlow);
  }
  if (format == protocol::Page::CaptureScreenshot::FormatEnum::Jpeg)
    return base::BindRepeating(&EncodeBitmapAsJpeg, quality);
  if (format == protocol::Page::CaptureScreenshot::FormatEnum::Webp)
    return base::BindRepeating(&EncodeBitmapAsWebp, quality);
  return protocol::Response::InvalidParams("Invalid image format");
}

std::unique_ptr<Page::ScreencastFrameMetadata> BuildScreencastFrameMetadata(
    const gfx::Size& surface_size,
    float device_scale_factor,
    float page_scale_factor,
    const gfx::PointF& root_scroll_offset,
    float top_controls_visible_height) {
  if (surface_size.IsEmpty() || device_scale_factor == 0)
    return nullptr;

  const gfx::SizeF content_size_dip =
      gfx::ScaleSize(gfx::SizeF(surface_size), 1 / device_scale_factor);
  float top_offset_dip = top_controls_visible_height;
  gfx::PointF root_scroll_offset_dip = root_scroll_offset;
  top_offset_dip /= device_scale_factor;
  root_scroll_offset_dip.Scale(1 / device_scale_factor);
  std::unique_ptr<Page::ScreencastFrameMetadata> page_metadata =
      Page::ScreencastFrameMetadata::Create()
          .SetPageScaleFactor(page_scale_factor)
          .SetOffsetTop(top_offset_dip)
          .SetDeviceWidth(content_size_dip.width())
          .SetDeviceHeight(content_size_dip.height())
          .SetScrollOffsetX(root_scroll_offset_dip.x())
          .SetScrollOffsetY(root_scroll_offset_dip.y())
          .SetTimestamp(base::Time::Now().InSecondsFSinceUnixEpoch())
          .Build();
  return page_metadata;
}

// Determines the snapshot size that best-fits the Surface's content to the
// remote's requested image size.
gfx::Size DetermineSnapshotSize(const gfx::Size& surface_size,
                                int screencast_max_width,
                                int screencast_max_height) {
  if (surface_size.IsEmpty())
    return gfx::Size();  // Nothing to copy (and avoid divide-by-zero below).

  double scale = 1;
  if (screencast_max_width > 0) {
    scale = std::min(scale, static_cast<double>(screencast_max_width) /
                                surface_size.width());
  }
  if (screencast_max_height > 0) {
    scale = std::min(scale, static_cast<double>(screencast_max_height) /
                                surface_size.height());
  }
  return gfx::ToRoundedSize(gfx::ScaleSize(gfx::SizeF(surface_size), scale));
}

void GetMetadataFromFrame(const media::VideoFrame& frame,
                          double* device_scale_factor,
                          double* page_scale_factor,
                          gfx::PointF* root_scroll_offset,
                          double* top_controls_visible_height) {
  // Get metadata from |frame|. This will CHECK if metadata is missing.
  *device_scale_factor = *frame.metadata().device_scale_factor;
  *page_scale_factor = *frame.metadata().page_scale_factor;
  root_scroll_offset->set_x(*frame.metadata().root_scroll_offset_x);
  root_scroll_offset->set_y(*frame.metadata().root_scroll_offset_y);
  *top_controls_visible_height = *frame.metadata().top_controls_visible_height;
}

template <typename ProtocolCallback>
bool CanExecuteGlobalCommands(
    PageHandler* page_handler,
    const std::unique_ptr<ProtocolCallback>& callback) {
  Response response = page_handler->AssureTopLevelActiveFrame();
  if (!response.IsError())
    return true;
  callback->sendFailure(response);
  return false;
}

void GotManifest(protocol::Maybe<std::string> manifest_id,
                 std::unique_ptr<PageHandler::GetAppManifestCallback> callback,
                 const GURL& manifest_url,
                 ::blink::mojom::ManifestPtr input_manifest,
                 blink::mojom::ManifestDebugInfoPtr debug_info) {
  if (manifest_id &&
      manifest_id.value() != input_manifest->id.possibly_invalid_spec()) {
    std::move(callback)->sendFailure(protocol::Response::InvalidParams(
        std::string("Page manifest id ") +
        input_manifest->id.possibly_invalid_spec() +
        " does not match the input " + manifest_id.value()));
    return;
  }

  auto errors = std::make_unique<protocol::Array<Page::AppManifestError>>();
  bool failed = true;
  if (debug_info) {
    failed = false;
    for (const auto& error : debug_info->errors) {
      errors->emplace_back(Page::AppManifestError::Create()
                               .SetMessage(error->message)
                               .SetCritical(error->critical)
                               .SetLine(error->line)
                               .SetColumn(error->column)
                               .Build());
      if (error->critical) {
        failed = true;
      }
    }
  }

  auto convert_icon = [](const blink::Manifest::ImageResource& input_icon)
      -> std::unique_ptr<Page::ImageResource> {
    auto icon = Page::ImageResource::Create();
    std::vector<std::string> size_strings;
    base::ranges::transform(input_icon.sizes, std::back_inserter(size_strings),
                            &gfx::Size::ToString);
    icon.SetSizes(base::JoinString(size_strings, " "));
    icon.SetType(base::UTF16ToUTF8(input_icon.type));
    return icon.SetUrl(input_icon.src.possibly_invalid_spec()).Build();
  };

  auto convert_icons =
      [convert_icon](
          const std::vector<blink::Manifest::ImageResource>& input_icons)
      -> std::unique_ptr<protocol::Array<Page::ImageResource>> {
    auto icons = std::make_unique<protocol::Array<Page::ImageResource>>();
    for (const auto& input_icon : input_icons) {
      icons->push_back(convert_icon(input_icon));
    }
    return icons;
  };

  auto manifest = Page::WebAppManifest::Create();
  if (input_manifest->has_background_color) {
    manifest.SetBackgroundColor(color_utils::SkColorToRgbaString(
        static_cast<SkColor>(input_manifest->background_color)));
  }
  if (input_manifest->description) {
    manifest.SetDescription(
        base::UTF16ToUTF8(input_manifest->description.value()));
  }
  // TODO(crbug.com/331214986): Fill the WebAppManifest.dir (direction).
  manifest.SetDisplay(base::ToString(input_manifest->display));
  if (!input_manifest->display_override.empty()) {
    auto display_overrides = std::make_unique<protocol::Array<std::string>>();
    for (const auto& display_override : input_manifest->display_override) {
      display_overrides->push_back(base::ToString(display_override));
    }
    manifest.SetDisplayOverrides(std::move(display_overrides));
  }
  if (!input_manifest->file_handlers.empty()) {
    auto file_handlers = std::make_unique<protocol::Array<Page::FileHandler>>();
    for (const auto& input_file_handler : input_manifest->file_handlers) {
      auto file_handler = Page::FileHandler::Create();
      if (!input_file_handler->icons.empty()) {
        file_handler.SetIcons(convert_icons(input_file_handler->icons));
      }
      if (!input_file_handler->accept.empty()) {
        auto accepts = std::make_unique<protocol::Array<Page::FileFilter>>();
        for (const auto& input_accept : input_file_handler->accept) {
          auto accept = Page::FileFilter::Create();
          accept.SetName(base::UTF16ToUTF8(input_accept.first));
          if (!input_accept.second.empty()) {
            auto accept_strs = std::make_unique<protocol::Array<std::string>>();
            for (const auto& accept_str : input_accept.second) {
              accept_strs->push_back(base::UTF16ToUTF8(accept_str));
            }
            accept.SetAccepts(std::move(accept_strs));
          }
          accepts->push_back(accept.Build());
        }
        file_handler.SetAccepts(std::move(accepts));
      }
      file_handlers->push_back(
          file_handler
              .SetAction(input_file_handler->action.possibly_invalid_spec())
              .SetName(base::UTF16ToUTF8(input_file_handler->name))
              .SetLaunchType(base::ToString(input_file_handler->launch_type))
              .Build());
    }
  }
  if (!input_manifest->icons.empty()) {
    manifest.SetIcons(convert_icons(input_manifest->icons));
  }
  manifest.SetId(input_manifest->id.possibly_invalid_spec());
  // TODO(crbug.com/331214986): Fill the WebAppManifest.lang.
  if (input_manifest->launch_handler) {
    manifest.SetLaunchHandler(
        Page::LaunchHandler::Create()
            .SetClientMode(base::ToString(
                input_manifest->launch_handler.value().client_mode))
            .Build());
  }
  if (input_manifest->name) {
    manifest.SetName(base::UTF16ToUTF8(input_manifest->name.value()));
  }
  manifest.SetOrientation(base::ToString(input_manifest->orientation));
  manifest.SetPreferRelatedApplications(
      input_manifest->prefer_related_applications);
  if (!input_manifest->protocol_handlers.empty()) {
    auto protocol_handlers =
        std::make_unique<protocol::Array<Page::ProtocolHandler>>();
    for (const auto& input_protocol_handler :
         input_manifest->protocol_handlers) {
      protocol_handlers->push_back(
          Page::ProtocolHandler::Create()
              .SetProtocol(base::UTF16ToUTF8(input_protocol_handler->protocol))
              .SetUrl(input_protocol_handler->url.possibly_invalid_spec())
              .Build());
    }
    manifest.SetProtocolHandlers(std::move(protocol_handlers));
  }
  if (!input_manifest->scope_extensions.empty()) {
    auto scope_extensions =
        std::make_unique<protocol::Array<Page::ScopeExtension>>();
    for (const auto& input_scope_extension : input_manifest->scope_extensions) {
      scope_extensions->push_back(
          Page::ScopeExtension::Create()
              .SetOrigin(input_scope_extension->origin.Serialize())
              .SetHasOriginWildcard(input_scope_extension->has_origin_wildcard)
              .Build());
    }
    manifest.SetScopeExtensions(std::move(scope_extensions));
  }
  if (!input_manifest->screenshots.empty()) {
    auto screenshots = std::make_unique<protocol::Array<Page::Screenshot>>();
    for (const auto& input_screenshot : input_manifest->screenshots) {
      auto screenshot = Page::Screenshot::Create();
      if (input_screenshot->label) {
        screenshot.SetLabel(base::UTF16ToUTF8(input_screenshot->label.value()));
      }
      screenshots->push_back(
          screenshot.SetImage(convert_icon(input_screenshot->image))
              .SetFormFactor(base::ToString(input_screenshot->form_factor))
              .Build());
    }
    manifest.SetScreenshots(std::move(screenshots));
  }
  if (input_manifest->share_target) {
    const auto& input_share_target = input_manifest->share_target.value();
    auto share_target = Page::ShareTarget::Create();
    if (input_share_target.params.title) {
      share_target.SetTitle(
          base::UTF16ToUTF8(input_share_target.params.title.value()));
    }
    if (input_share_target.params.text) {
      share_target.SetTitle(
          base::UTF16ToUTF8(input_share_target.params.text.value()));
    }
    if (input_share_target.params.url) {
      share_target.SetTitle(
          base::UTF16ToUTF8(input_share_target.params.url.value()));
    }
    manifest.SetShareTarget(
        share_target
            .SetAction(
                input_manifest->share_target->action.possibly_invalid_spec())
            .SetMethod(base::ToString(input_manifest->share_target->method))
            .SetEnctype(base::ToString(input_manifest->share_target->action))
            .Build());
  }
  if (!input_manifest->related_applications.empty()) {
    auto related_apps =
        std::make_unique<protocol::Array<Page::RelatedApplication>>();
    for (const auto& input_related_app : input_manifest->related_applications) {
      auto related_app = Page::RelatedApplication::Create();
      if (input_related_app.id) {
        related_app.SetId(base::UTF16ToUTF8(input_related_app.id.value()));
      }
      related_apps->push_back(
          related_app.SetUrl(input_related_app.url.possibly_invalid_spec())
              .Build());
    }
    manifest.SetRelatedApplications(std::move(related_apps));
  }
  manifest.SetScope(input_manifest->scope.possibly_invalid_spec());
  if (!input_manifest->shortcuts.empty()) {
    auto shortcuts = std::make_unique<protocol::Array<Page::Shortcut>>();
    for (const auto& input_shortcut : input_manifest->shortcuts) {
      shortcuts->push_back(
          Page::Shortcut::Create()
              .SetName(base::UTF16ToUTF8(input_shortcut.name))
              .SetUrl(input_shortcut.url.possibly_invalid_spec())
              .Build());
    }
    manifest.SetShortcuts(std::move(shortcuts));
  }
  manifest.SetStartUrl(input_manifest->start_url.possibly_invalid_spec());
  if (input_manifest->has_theme_color) {
    manifest.SetThemeColor(color_utils::SkColorToRgbaString(
        static_cast<SkColor>(input_manifest->theme_color)));
  }

  std::unique_ptr<Page::AppManifestParsedProperties> parsed;
  if (!blink::IsEmptyManifest(input_manifest)) {
    parsed = Page::AppManifestParsedProperties::Create()
                 .SetScope(input_manifest->scope.possibly_invalid_spec())
                 .Build();
  }

  std::move(callback)->sendSuccess(
      manifest_url.possibly_invalid_spec(), std::move(errors),
      failed ? Maybe<std::string>() : debug_info->raw_manifest,
      std::move(parsed), manifest.Build());
}

}  // namespace

struct PageHandler::PendingScreenshotRequest {
  PendingScreenshotRequest(base::ScopedClosureRunner capturer_handle,
                           PageHandler::BitmapEncoder encoder,
                           std::unique_ptr<CaptureScreenshotCallback> callback)
      : capturer_handle(std::move(capturer_handle)),
        encoder(std::move(encoder)),
        callback(std::move(callback)) {}

  base::ScopedClosureRunner capturer_handle;
  PageHandler::BitmapEncoder encoder;
  std::unique_ptr<CaptureScreenshotCallback> callback;
  blink::DeviceEmulationParams original_emulation_params;
  std::optional<blink::web_pref::WebPreferences> original_web_prefs;
  gfx::Size original_view_size;
  gfx::Size requested_image_size;
};

PageHandler::PageHandler(EmulationHandler* emulation_handler,
                         BrowserHandler* browser_handler,
                         bool allow_unsafe_operations,
                         bool is_trusted,
                         std::optional<url::Origin> navigation_initiator_origin,
                         bool may_read_local_files)
    : DevToolsDomainHandler(Page::Metainfo::domainName),
      allow_unsafe_operations_(allow_unsafe_operations),
      is_trusted_(is_trusted),
      navigation_initiator_origin_(navigation_initiator_origin),
      may_read_local_files_(may_read_local_files),
      enabled_(false),
      screencast_max_width_(-1),
      screencast_max_height_(-1),
      capture_every_nth_frame_(1),
      session_id_(0),
      frame_counter_(0),
      frames_in_flight_(0),
      host_(nullptr),
      emulation_handler_(emulation_handler),
      browser_handler_(browser_handler) {
#if BUILDFLAG(IS_ANDROID)
  constexpr auto kScreencastPixelFormat = media::PIXEL_FORMAT_I420;
#else
  constexpr auto kScreencastPixelFormat = media::PIXEL_FORMAT_ARGB;
#endif
  video_consumer_ = std::make_unique<DevToolsVideoConsumer>(base::BindRepeating(
      &PageHandler::OnFrameFromVideoConsumer, weak_factory_.GetWeakPtr()));
  video_consumer_->SetFormat(kScreencastPixelFormat);
  DCHECK(emulation_handler_);
}

PageHandler::~PageHandler() = default;

// static
std::vector<PageHandler*> PageHandler::EnabledForWebContents(
    WebContentsImpl* contents) {
  if (!DevToolsAgentHost::HasFor(contents))
    return std::vector<PageHandler*>();
  std::vector<PageHandler*> result;
  for (auto* handler :
       PageHandler::ForAgentHost(static_cast<DevToolsAgentHostImpl*>(
           DevToolsAgentHost::GetOrCreateFor(contents).get()))) {
    if (handler->enabled_)
      result.push_back(handler);
  }
  return result;
}

// static
std::vector<PageHandler*> PageHandler::ForAgentHost(
    DevToolsAgentHostImpl* host) {
  return host->HandlersByName<PageHandler>(Page::Metainfo::domainName);
}

void PageHandler::SetRenderer(int process_host_id,
                              RenderFrameHostImpl* frame_host) {
  if (host_ == frame_host)
    return;

  RenderWidgetHostImpl* widget_host =
      host_ ? host_->GetRenderWidgetHost() : nullptr;
  if (widget_host && observation_.IsObservingSource(widget_host))
    observation_.Reset();

  host_ = frame_host;
  widget_host = host_ ? host_->GetRenderWidgetHost() : nullptr;

  if (widget_host)
    observation_.Observe(widget_host);

  if (frame_host) {
    video_consumer_->SetFrameSinkId(
        frame_host->GetRenderWidgetHost()->GetFrameSinkId());
  }
}

void PageHandler::Wire(UberDispatcher* dispatcher) {
  frontend_ = std::make_unique<Page::Frontend>(dispatcher->channel());
  Page::Dispatcher::wire(dispatcher, this);
}

void PageHandler::RenderWidgetHostVisibilityChanged(
    RenderWidgetHost* widget_host,
    bool became_visible) {
  if (!screencast_encoder_)
    return;
  NotifyScreencastVisibility(became_visible);
}

void PageHandler::RenderWidgetHostDestroyed(RenderWidgetHost* widget_host) {
  DCHECK(observation_.IsObservingSource(widget_host));
  observation_.Reset();
}

void PageHandler::DidAttachInterstitialPage() {
  if (!enabled_)
    return;
  frontend_->InterstitialShown();
}

void PageHandler::DidDetachInterstitialPage() {
  if (!enabled_)
    return;
  frontend_->InterstitialHidden();
}

void PageHandler::DidRunJavaScriptDialog(const GURL& url,
                                         const std::u16string& message,
                                         const std::u16string& default_prompt,
                                         JavaScriptDialogType dialog_type,
                                         bool has_non_devtools_handlers,
                                         JavaScriptDialogCallback callback) {
  if (!enabled_)
    return;
  DCHECK(pending_dialog_.is_null());
  pending_dialog_ = std::move(callback);
  std::string type = Page::DialogTypeEnum::Alert;
  if (dialog_type == JAVASCRIPT_DIALOG_TYPE_CONFIRM)
    type = Page::DialogTypeEnum::Confirm;
  if (dialog_type == JAVASCRIPT_DIALOG_TYPE_PROMPT)
    type = Page::DialogTypeEnum::Prompt;
  frontend_->JavascriptDialogOpening(url.spec(), base::UTF16ToUTF8(message),
                                     type, has_non_devtools_handlers,
                                     base::UTF16ToUTF8(default_prompt));
}

void PageHandler::DidRunBeforeUnloadConfirm(const GURL& url,
                                            bool has_non_devtools_handlers,
                                            JavaScriptDialogCallback callback) {
  if (!enabled_)
    return;
  DCHECK(pending_dialog_.is_null());
  pending_dialog_ = std::move(callback);
  frontend_->JavascriptDialogOpening(url.spec(), std::string(),
                                     Page::DialogTypeEnum::Beforeunload,
                                     has_non_devtools_handlers, std::string());
}

void PageHandler::DidCloseJavaScriptDialog(bool success,
                                           const std::u16string& user_input) {
  if (!enabled_)
    return;
  pending_dialog_.Reset();
  frontend_->JavascriptDialogClosed(success, base::UTF16ToUTF8(user_input));
}

Response PageHandler::Enable() {
  enabled_ = true;
  return Response::FallThrough();
}

Response PageHandler::Disable() {
  enabled_ = false;
  bypass_csp_ = false;

  StopScreencast();

  if (!pending_dialog_.is_null()) {
    ResponseOrWebContents result = GetWebContentsForTopLevelActiveFrame();
    // Only a top level frame can have a dialog.
    DCHECK(absl::holds_alternative<WebContentsImpl*>(result));
    WebContentsImpl* web_contents = absl::get<WebContentsImpl*>(result);
    // Leave dialog hanging if there is a manager that can take care of it,
    // cancel and send ack otherwise.
    bool has_dialog_manager =
        web_contents && web_contents->GetDelegate() &&
        web_contents->GetDelegate()->GetJavaScriptDialogManager(web_contents);
    if (!has_dialog_manager)
      std::move(pending_dialog_).Run(false, std::u16string());
    pending_dialog_.Reset();
  }

  for (download::DownloadItem* item : pending_downloads_) {
    item->RemoveObserver(this);
  }
  pending_downloads_.clear();
  navigate_callbacks_.clear();
  SetPrerenderingAllowed(true);

  return Response::FallThrough();
}

Response PageHandler::Crash() {
  // Can be called in a subframe.
  WebContents* web_contents = WebContents::FromRenderFrameHost(host_);
  if (!web_contents)
    return Response::ServerError(kErrorNotAttached);
  if (web_contents->IsCrashed())
    return Response::ServerError("The target has already crashed");
  if (host_->frame_tree_node()->navigation_request())
    return Response::ServerError("Page has pending navigations, not killing");
  return Response::FallThrough();
}

Response PageHandler::Close() {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;

  host_->DispatchBeforeUnload(RenderFrameHostImpl::BeforeUnloadType::TAB_CLOSE,
                              false);
  return Response::Success();
}

void PageHandler::Reload(Maybe<bool> bypassCache,
                         Maybe<std::string> script_to_evaluate_on_load,
                         Maybe<std::string> loader_id,
                         std::unique_ptr<ReloadCallback> callback) {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError()) {
    callback->sendFailure(response);
    return;
  }

  // In the case of inspecting a GuestView (e.g. a PDF), we should reload
  // the outer web contents (embedder), since otherwise reloading the guest by
  // itself will fail.
  RenderFrameHostImpl* outermost_main_frame =
      host_->GetOutermostMainFrameOrEmbedder();

  if (loader_id.has_value()) {
    auto navigation_token = outermost_main_frame->GetDevToolsNavigationToken();
    if (!navigation_token.has_value() ||
        *loader_id != navigation_token->ToString()) {
      callback->sendFailure(Response::InvalidParams(
          "Reload was discarded because the page already navigated"));
      return;
    }
  }

  // It is important to fallback before triggering reload, so that
  // renderer could prepare beforehand.
  callback->fallThrough();
  outermost_main_frame->frame_tree()->controller().Reload(
      bypassCache.value_or(false) ? ReloadType::BYPASSING_CACHE
                                  : ReloadType::NORMAL,
      false);
}

static network::mojom::ReferrerPolicy ParsePolicyFromString(
    const std::string& policy) {
  if (policy == Page::ReferrerPolicyEnum::NoReferrer)
    return network::mojom::ReferrerPolicy::kNever;
  if (policy == Page::ReferrerPolicyEnum::NoReferrerWhenDowngrade)
    return network::mojom::ReferrerPolicy::kNoReferrerWhenDowngrade;
  if (policy == Page::ReferrerPolicyEnum::Origin)
    return network::mojom::ReferrerPolicy::kOrigin;
  if (policy == Page::ReferrerPolicyEnum::OriginWhenCrossOrigin)
    return network::mojom::ReferrerPolicy::kOriginWhenCrossOrigin;
  if (policy == Page::ReferrerPolicyEnum::SameOrigin)
    return network::mojom::ReferrerPolicy::kSameOrigin;
  if (policy == Page::ReferrerPolicyEnum::StrictOrigin)
    return network::mojom::ReferrerPolicy::kStrictOrigin;
  if (policy == Page::ReferrerPolicyEnum::StrictOriginWhenCrossOrigin) {
    return network::mojom::ReferrerPolicy::kStrictOriginWhenCrossOrigin;
  }
  if (policy == Page::ReferrerPolicyEnum::UnsafeUrl)
    return network::mojom::ReferrerPolicy::kAlways;

  DCHECK(policy.empty());
  return network::mojom::ReferrerPolicy::kDefault;
}

namespace {

void DispatchNavigateCallback(
    NavigationRequest* request,
    std::unique_ptr<PageHandler::NavigateCallback> callback) {
  std::string frame_id = request->frame_tree_node()
                             ->current_frame_host()
                             ->devtools_frame_token()
                             .ToString();
  // A new NavigationRequest may have been created before |request|
  // started, in which case it is not marked as aborted. We report this as an
  // abort to DevTools anyway.
  if (!request->IsNavigationStarted()) {
    callback->sendSuccess(frame_id, Maybe<std::string>(),
                          net::ErrorToString(net::ERR_ABORTED));
    return;
  }
  Maybe<std::string> opt_error;
  if (request->GetNetErrorCode() != net::OK)
    opt_error = net::ErrorToString(request->GetNetErrorCode());
  Maybe<std::string> loader_id =
      request->IsSameDocument()
          ? Maybe<std::string>()
          : request->devtools_navigation_token().ToString();
  callback->sendSuccess(frame_id, std::move(loader_id), std::move(opt_error));
}

}  // namespace

void PageHandler::Navigate(const std::string& url,
                           Maybe<std::string> referrer,
                           Maybe<std::string> maybe_transition_type,
                           Maybe<std::string> frame_id,
                           Maybe<std::string> referrer_policy,
                           std::unique_ptr<NavigateCallback> callback) {
  GURL gurl(url);
  if (!gurl.is_valid()) {
    callback->sendFailure(
        Response::ServerError("Cannot navigate to invalid URL"));
    return;
  }
  if (gurl.SchemeIsFile() && !may_read_local_files_) {
    callback->sendFailure(
        Response::ServerError("Navigating to local URL is not allowed"));
    return;
  }

  if (!host_) {
    callback->sendFailure(Response::InternalError());
    return;
  }

  // chrome-untrusted:// WebUIs might perform high-priviledged actions on
  // navigation, disallow navigation to them unless the client is trusted.
  if (gurl.SchemeIs(kChromeUIUntrustedScheme) && !is_trusted_) {
    callback->sendFailure(Response::ServerError(
        "Navigating to a URL with a privileged scheme is not allowed"));
    return;
  }

  ui::PageTransition type;
  std::string transition_type =
      maybe_transition_type.value_or(Page::TransitionTypeEnum::Typed);
  if (transition_type == Page::TransitionTypeEnum::Link)
    type = ui::PAGE_TRANSITION_LINK;
  else if (transition_type == Page::TransitionTypeEnum::Typed)
    type = ui::PAGE_TRANSITION_TYPED;
  else if (transition_type == Page::TransitionTypeEnum::Address_bar)
    type = ui::PAGE_TRANSITION_FROM_ADDRESS_BAR;
  else if (transition_type == Page::TransitionTypeEnum::Auto_bookmark)
    type = ui::PAGE_TRANSITION_AUTO_BOOKMARK;
  else if (transition_type == Page::TransitionTypeEnum::Auto_subframe)
    type = ui::PAGE_TRANSITION_AUTO_SUBFRAME;
  else if (transition_type == Page::TransitionTypeEnum::Manual_subframe)
    type = ui::PAGE_TRANSITION_MANUAL_SUBFRAME;
  else if (transition_type == Page::TransitionTypeEnum::Generated)
    type = ui::PAGE_TRANSITION_GENERATED;
  else if (transition_type == Page::TransitionTypeEnum::Auto_toplevel)
    type = ui::PAGE_TRANSITION_AUTO_TOPLEVEL;
  else if (transition_type == Page::TransitionTypeEnum::Form_submit)
    type = ui::PAGE_TRANSITION_FORM_SUBMIT;
  else if (transition_type == Page::TransitionTypeEnum::Reload)
    type = ui::PAGE_TRANSITION_RELOAD;
  else if (transition_type == Page::TransitionTypeEnum::Keyword)
    type = ui::PAGE_TRANSITION_KEYWORD;
  else if (transition_type == Page::TransitionTypeEnum::Keyword_generated)
    type = ui::PAGE_TRANSITION_KEYWORD_GENERATED;
  else
    type = ui::PAGE_TRANSITION_TYPED;

  type = ui::PageTransitionFromInt(type | ui::PAGE_TRANSITION_FROM_API);

  std::string out_frame_id =
      frame_id.value_or(host_->devtools_frame_token().ToString());
  FrameTreeNode* frame_tree_node = FrameTreeNodeFromDevToolsFrameToken(
      host_->frame_tree_node(), out_frame_id);

  if (!frame_tree_node) {
    callback->sendFailure(
        Response::ServerError("No frame with given id found"));
    return;
  }

  NavigationController::LoadURLParams params(gurl);
  network::mojom::ReferrerPolicy policy =
      ParsePolicyFromString(referrer_policy.value_or(""));
  params.referrer = Referrer(GURL(referrer.value_or("")), policy);
  params.transition_type = type;
  params.frame_tree_node_id = frame_tree_node->frame_tree_node_id();
  if (navigation_initiator_origin_.has_value()) {
    // When this agent has an initiator origin defined, ensure that its
    // navigations are considered renderer-initiated by that origin, such that
    // URL spoof defenses are in effect. (crbug.com/1192417)
    params.is_renderer_initiated = true;
    params.initiator_origin = *navigation_initiator_origin_;
    params.source_site_instance = SiteInstance::CreateForURL(
        host_->GetBrowserContext(), navigation_initiator_origin_->GetURL());
  }
  // Handler may be destroyed while navigating if the session
  // gets disconnected as a result of access checks.
  base::WeakPtr<PageHandler> weak_self = weak_factory_.GetWeakPtr();
  base::WeakPtr<NavigationHandle> navigation_handle =
      frame_tree_node->navigator().controller().LoadURLWithParams(params);
  // TODO(caseq): should we still dispatch callback here?
  if (!weak_self)
    return;
  if (!navigation_handle) {
    callback->sendSuccess(out_frame_id, Maybe<std::string>(),
                          net::ErrorToString(net::ERR_ABORTED));
    return;
  }
  auto* navigation_request =
      static_cast<NavigationRequest*>(navigation_handle.get());
  if (frame_tree_node->navigation_request() != navigation_request) {
    // The ownership of the navigation request should have been transferred to
    // RFH at this point, so we won't get `NavigationReset` for it any more --
    // fire the callback now!
    DispatchNavigateCallback(navigation_request, std::move(callback));
    return;
  }
  // At this point, we expect the callback to get dispatched upon
  // `NavigationReset()` is called when `NavigationRequest` is taken from
  // `FrameTreeNode`.
  const base::UnguessableToken& navigation_token =
      navigation_request->devtools_navigation_token();
  navigate_callbacks_[navigation_token] = std::move(callback);
}

void PageHandler::NavigationReset(NavigationRequest* navigation_request) {
  auto it =
      navigate_callbacks_.find(navigation_request->devtools_navigation_token());
  if (it == navigate_callbacks_.end())
    return;
  DispatchNavigateCallback(navigation_request, std::move(it->second));
  navigate_callbacks_.erase(it);
}

void PageHandler::DownloadWillBegin(FrameTreeNode* ftn,
                                    download::DownloadItem* item) {
  if (!enabled_)
    return;

  // The filename the end user sees may differ. This is an attempt to eagerly
  // determine the filename at the beginning of the download; see
  // DownloadTargetDeterminer:DownloadTargetDeterminer::Result
  // and DownloadTargetDeterminer::GenerateFileName in
  // chrome/browser/download/download_target_determiner.cc
  // for the more comprehensive logic.
  const std::u16string likely_filename = net::GetSuggestedFilename(
      item->GetURL(), item->GetContentDisposition(), std::string(),
      item->GetSuggestedFilename(), item->GetMimeType(), "download");

  frontend_->DownloadWillBegin(
      ftn->current_frame_host()->devtools_frame_token().ToString(),
      item->GetGuid(), item->GetURL().spec(),
      base::UTF16ToUTF8(likely_filename));

  item->AddObserver(this);
  pending_downloads_.insert(item);
}

void PageHandler::OnFrameDetached(const base::UnguessableToken& frame_id) {
  if (!enabled_)
    return;
  frontend_->FrameDetached(frame_id.ToString(), "remove");
}

void PageHandler::DidChangeFrameLoadingState(const FrameTreeNode& ftn) {
  if (!enabled_) {
    return;
  }
  const std::string& frame_id =
      ftn.current_frame_host()->devtools_frame_token().ToString();
  if (ftn.IsLoading()) {
    frontend_->FrameStartedLoading(frame_id);
  } else {
    frontend_->FrameStoppedLoading(frame_id);
  }
}

void PageHandler::OnDownloadDestroyed(download::DownloadItem* item) {
  pending_downloads_.erase(item);
}

void PageHandler::OnDownloadUpdated(download::DownloadItem* item) {
  if (!enabled_)
    return;
  std::string state;
  switch (item->GetState()) {
    case download::DownloadItem::IN_PROGRESS:
      state = Page::DownloadProgress::StateEnum::InProgress;
      break;
    case download::DownloadItem::COMPLETE:
      state = Page::DownloadProgress::StateEnum::Completed;
      break;
    case download::DownloadItem::CANCELLED:
    case download::DownloadItem::INTERRUPTED:
      state = Page::DownloadProgress::StateEnum::Canceled;
      break;
    case download::DownloadItem::MAX_DOWNLOAD_STATE:
      NOTREACHED_IN_MIGRATION();
  }
  frontend_->DownloadProgress(item->GetGuid(), item->GetTotalBytes(),
                              item->GetReceivedBytes(), state);
  if (state != Page::DownloadProgress::StateEnum::InProgress) {
    item->RemoveObserver(this);
    pending_downloads_.erase(item);
  }
}

static const char* TransitionTypeName(ui::PageTransition type) {
  int32_t t = type & ~ui::PAGE_TRANSITION_QUALIFIER_MASK;
  switch (t) {
    case ui::PAGE_TRANSITION_LINK:
      return Page::TransitionTypeEnum::Link;
    case ui::PAGE_TRANSITION_TYPED:
      return Page::TransitionTypeEnum::Typed;
    case ui::PAGE_TRANSITION_AUTO_BOOKMARK:
      return Page::TransitionTypeEnum::Auto_bookmark;
    case ui::PAGE_TRANSITION_AUTO_SUBFRAME:
      return Page::TransitionTypeEnum::Auto_subframe;
    case ui::PAGE_TRANSITION_MANUAL_SUBFRAME:
      return Page::TransitionTypeEnum::Manual_subframe;
    case ui::PAGE_TRANSITION_GENERATED:
      return Page::TransitionTypeEnum::Generated;
    case ui::PAGE_TRANSITION_AUTO_TOPLEVEL:
      return Page::TransitionTypeEnum::Auto_toplevel;
    case ui::PAGE_TRANSITION_FORM_SUBMIT:
      return Page::TransitionTypeEnum::Form_submit;
    case ui::PAGE_TRANSITION_RELOAD:
      return Page::TransitionTypeEnum::Reload;
    case ui::PAGE_TRANSITION_KEYWORD:
      return Page::TransitionTypeEnum::Keyword;
    case ui::PAGE_TRANSITION_KEYWORD_GENERATED:
      return Page::TransitionTypeEnum::Keyword_generated;
    default:
      return Page::TransitionTypeEnum::Other;
  }
}

Response PageHandler::GetNavigationHistory(
    int* current_index,
    std::unique_ptr<NavigationEntries>* entries) {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;

  NavigationController& controller = host_->frame_tree()->controller();
  *current_index = controller.GetCurrentEntryIndex();
  *entries = std::make_unique<NavigationEntries>();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    auto* entry = controller.GetEntryAtIndex(i);
    (*entries)->emplace_back(
        Page::NavigationEntry::Create()
            .SetId(entry->GetUniqueID())
            .SetUrl(entry->GetURL().spec())
            .SetUserTypedURL(entry->GetUserTypedURL().spec())
            .SetTitle(base::UTF16ToUTF8(entry->GetTitle()))
            .SetTransitionType(TransitionTypeName(entry->GetTransitionType()))
            .Build());
  }
  return Response::Success();
}

Response PageHandler::NavigateToHistoryEntry(int entry_id) {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;

  NavigationController& controller = host_->frame_tree()->controller();
  for (int i = 0; i != controller.GetEntryCount(); ++i) {
    if (controller.GetEntryAtIndex(i)->GetUniqueID() == entry_id) {
      controller.GoToIndex(i);
      return Response::Success();
    }
  }

  return Response::InvalidParams("No entry with passed id");
}

static bool ReturnTrue(NavigationEntry* entry) {
  return true;
}

Response PageHandler::ResetNavigationHistory() {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;

  NavigationController& controller = host_->frame_tree()->controller();
  if (controller.CanPruneAllButLastCommitted()) {
    controller.DeleteNavigationEntries(base::BindRepeating(&ReturnTrue));
    return Response::Success();
  } else {
    return Response::ServerError("History cannot be pruned");
  }
}

void PageHandler::CaptureSnapshot(
    Maybe<std::string> format,
    std::unique_ptr<CaptureSnapshotCallback> callback) {
  if (!CanExecuteGlobalCommands(this, callback))
    return;
  std::string snapshot_format = format.value_or(kMhtml);
  if (snapshot_format != kMhtml) {
    callback->sendFailure(Response::ServerError("Unsupported snapshot format"));
    return;
  }

  DCHECK(host_);
  DevToolsMHTMLHelper::Capture(
      base::BindRepeating(&WebContents::FromFrameTreeNodeId,
                          host_->frame_tree_node()->frame_tree_node_id()),
      std::move(callback));
}

// Sets a clip with full page dimensions. Calls CaptureScreenshot with updated
// value to proceed with capturing the full page screenshot.
// TODO(crbug.com/40238745): at the point this method is called, the page could
// have changed its size.
void PageHandler::CaptureFullPageScreenshot(
    Maybe<std::string> format,
    Maybe<int> quality,
    Maybe<bool> optimize_for_speed,
    std::unique_ptr<CaptureScreenshotCallback> callback,
    const gfx::Size& full_page_size) {
  // check width and height for validity
  // max_size is needed to respect the limit of 16K of the headless mode
  const int kMaxDimension = 128 * 1024;
  if (full_page_size.width() >= kMaxDimension ||
      full_page_size.height() >= kMaxDimension) {
    callback->sendFailure(Response::ServerError("Page is too large."));
    return;
  }

  auto clip = Page::Viewport::Create()
                  .SetX(0)
                  .SetY(0)
                  .SetWidth(full_page_size.width())
                  .SetHeight(full_page_size.height())
                  .SetScale(1)
                  .Build();
  CaptureScreenshot(std::move(format), std::move(quality), std::move(clip),
                    /*from_surface=*/true, /*capture_beyond_viewport=*/true,
                    std::move(optimize_for_speed), std::move(callback));
}

void PageHandler::CaptureScreenshot(
    Maybe<std::string> format,
    Maybe<int> quality,
    Maybe<Page::Viewport> clip,
    Maybe<bool> from_surface,
    Maybe<bool> capture_beyond_viewport,
    Maybe<bool> optimize_for_speed,
    std::unique_ptr<CaptureScreenshotCallback> callback) {
  if (!host_ || !host_->GetRenderWidgetHost() ||
      !host_->GetRenderWidgetHost()->GetView()) {
    callback->sendFailure(Response::InternalError());
    return;
  }
  if (!CanExecuteGlobalCommands(this, callback))
    return;

  // Check if full page screenshot is expected and get dimensions accordingly.
  if (from_surface.value_or(true) && capture_beyond_viewport.value_or(false) &&
      !clip.has_value()) {
    blink::mojom::LocalMainFrame* main_frame =
        host_->GetAssociatedLocalMainFrame();
    main_frame->GetFullPageSize(base::BindOnce(
        &PageHandler::CaptureFullPageScreenshot, weak_factory_.GetWeakPtr(),
        std::move(format), std::move(quality), std::move(optimize_for_speed),
        std::move(callback)));
    return;
  }
  if (clip.has_value()) {
    if (clip->GetWidth() == 0) {
      callback->sendFailure(
          Response::ServerError("Cannot take screenshot with 0 width."));
      return;
    }
    if (clip->GetHeight() == 0) {
      callback->sendFailure(
          Response::ServerError("Cannot take screenshot with 0 height."));
      return;
    }
  }

  RenderWidgetHostImpl* widget_host = host_->GetRenderWidgetHost();
  auto encoder =
      GetEncoder(format.value_or(Page::CaptureScreenshot::FormatEnum::Png),
                 quality.value_or(kDefaultScreenshotQuality),
                 optimize_for_speed.value_or(false));
  if (absl::holds_alternative<Response>(encoder)) {
    callback->sendFailure(absl::get<Response>(encoder));
    return;
  }

  base::ScopedClosureRunner capturer_handle;
  if (auto* wc = WebContents::FromRenderFrameHost(host_)) {
    // Tell page it needs to produce frames even if it doesn't want to (e.g. is
    // not currently visible).
    capturer_handle =
        wc->IncrementCapturerCount(gfx::Size(), /*stay_hidden=*/true,
                                   /*stay_awake=*/true, /*is_activity=*/false);
  }

  auto pending_request = std::make_unique<PendingScreenshotRequest>(
      std::move(capturer_handle), std::move(absl::get<BitmapEncoder>(encoder)),
      std::move(callback));

  // We don't support clip/emulation when capturing from window, bail out.
  if (!from_surface.value_or(true)) {
    if (!is_trusted_) {
      pending_request->callback->sendFailure(
          Response::ServerError("Only screenshots from surface are allowed."));
      return;
    }
    widget_host->GetSnapshotFromBrowser(
        base::BindOnce(&PageHandler::ScreenshotCaptured,
                       weak_factory_.GetWeakPtr(), std::move(pending_request)),
        false);
    return;
  }

  // Welcome to the neural net of capturing screenshot while emulating device
  // metrics!
  bool emulation_enabled = emulation_handler_->device_emulation_enabled();
  pending_request->original_emulation_params =
      emulation_handler_->GetDeviceEmulationParams();
  const blink::DeviceEmulationParams& original_params =
      pending_request->original_emulation_params;
  blink::DeviceEmulationParams modified_params = original_params;

  // Capture original view size if we know we are going to destroy it. We use
  // it in ScreenshotCaptured to restore.
  const gfx::Size original_view_size =
      emulation_enabled || clip.has_value()
          ? widget_host->GetView()->GetViewBounds().size()
          : gfx::Size();
  pending_request->original_view_size = original_view_size;
  gfx::Size emulated_view_size = modified_params.view_size;

  double dpfactor = 1;
  float widget_host_device_scale_factor = widget_host->GetDeviceScaleFactor();
  if (emulation_enabled) {
    // When emulating, emulate again and scale to make resulting image match
    // physical DP resolution. If view_size is not overriden, use actual view
    // size.
    float original_scale =
        original_params.scale > 0 ? original_params.scale : 1;
    if (!modified_params.view_size.width()) {
      emulated_view_size.set_width(
          ceil(original_view_size.width() / original_scale));
    }
    if (!modified_params.view_size.height()) {
      emulated_view_size.set_height(
          ceil(original_view_size.height() / original_scale));
    }

    dpfactor = modified_params.device_scale_factor
                   ? modified_params.device_scale_factor /
                         widget_host_device_scale_factor
                   : 1;
    // When clip is specified, we scale viewport via clip, otherwise we use
    // scale.
    modified_params.scale = clip.has_value() ? 1 : dpfactor;
    modified_params.view_size = emulated_view_size;
  } else if (clip.has_value()) {
    // When not emulating, still need to emulate the page size.
    modified_params.view_size = original_view_size;
    modified_params.screen_size = gfx::Size();
    modified_params.device_scale_factor = 0;
    modified_params.scale = 1;
  }

  // Set up viewport in renderer.
  if (clip) {
    modified_params.viewport_offset.SetPoint(clip.value().GetX(),
                                             clip.value().GetY());
    modified_params.viewport_scale = clip.value().GetScale() * dpfactor;
    modified_params.viewport_offset.Scale(widget_host_device_scale_factor);
  }

  if (capture_beyond_viewport.value_or(false)) {
    pending_request->original_web_prefs =
        host_->render_view_host()->GetDelegate()->GetOrCreateWebPreferences();
    const blink::web_pref::WebPreferences& original_web_prefs =
        *pending_request->original_web_prefs;
    blink::web_pref::WebPreferences modified_web_prefs = original_web_prefs;

    // Hiding scrollbar is needed to avoid scrollbar artefacts on the
    // screenshot. Details: https://crbug.com/1003629.
    modified_web_prefs.hide_scrollbars = true;
    modified_web_prefs.record_whole_document = true;
    host_->render_view_host()->GetDelegate()->SetWebPreferences(
        modified_web_prefs);

    {
      // TODO(crbug.com/40727379): Remove the bug is fixed.
      // Walkaround for the bug. Emulated `view_size` has to be set twice,
      // otherwise the scrollbar will be on the screenshot present.
      blink::DeviceEmulationParams tmp_params = modified_params;
      tmp_params.view_size = gfx::Size(1, 1);
      emulation_handler_->SetDeviceEmulationParams(tmp_params);
    }
  }

  // We use DeviceEmulationParams to either emulate, set viewport or both.
  emulation_handler_->SetDeviceEmulationParams(modified_params);

  // Set view size for the screenshot right after emulating.
  if (clip.has_value()) {
    double scale = dpfactor * clip->GetScale();
    widget_host->GetView()->SetSize(
        gfx::Size(base::ClampRound(clip->GetWidth() * scale),
                  base::ClampRound(clip->GetHeight() * scale)));
  } else if (emulation_enabled) {
    widget_host->GetView()->SetSize(
        gfx::ScaleToFlooredSize(emulated_view_size, dpfactor));
  }
  if (emulation_enabled || clip.has_value()) {
    const gfx::Size requested_image_size =
        clip.has_value() ? gfx::Size(clip->GetWidth(), clip->GetHeight())
                         : emulated_view_size;
    double scale = widget_host_device_scale_factor * dpfactor;
    if (clip.has_value()) {
      scale *= clip->GetScale();
    }
    pending_request->requested_image_size =
        gfx::ScaleToRoundedSize(requested_image_size, scale);
  }

  widget_host->GetSnapshotFromBrowser(
      base::BindOnce(&PageHandler::ScreenshotCaptured,
                     weak_factory_.GetWeakPtr(), std::move(pending_request)),
      true);
}

Response PageHandler::StartScreencast(Maybe<std::string> format,
                                      Maybe<int> quality,
                                      Maybe<int> max_width,
                                      Maybe<int> max_height,
                                      Maybe<int> every_nth_frame) {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;
  RenderWidgetHostImpl* widget_host = host_->GetRenderWidgetHost();
  if (!widget_host)
    return Response::InternalError();

  auto encoder =
      GetEncoder(format.value_or(Page::CaptureScreenshot::FormatEnum::Png),
                 quality.value_or(kDefaultScreenshotQuality),
                 /* optimize_for_speed= */ true);
  if (absl::holds_alternative<Response>(encoder))
    return absl::get<Response>(encoder);

  screencast_encoder_ = absl::get<BitmapEncoder>(encoder);

  screencast_max_width_ = max_width.value_or(-1);
  screencast_max_height_ = max_height.value_or(-1);
  ++session_id_;
  frame_counter_ = 0;
  frames_in_flight_ = 0;
  capture_every_nth_frame_ = every_nth_frame.value_or(1);
  bool visible = !widget_host->is_hidden();
  NotifyScreencastVisibility(visible);

  gfx::Size surface_size = gfx::Size();
  RenderWidgetHostViewBase* const view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (view) {
    surface_size = view->GetCompositorViewportPixelSize();
    last_surface_size_ = surface_size;
  }

  gfx::Size snapshot_size = DetermineSnapshotSize(
      surface_size, screencast_max_width_, screencast_max_height_);
  if (!snapshot_size.IsEmpty())
    video_consumer_->SetMinAndMaxFrameSize(snapshot_size, snapshot_size);

  video_consumer_->StartCapture();

  return Response::FallThrough();
}

Response PageHandler::StopScreencast() {
  screencast_encoder_.Reset();
  if (video_consumer_)
    video_consumer_->StopCapture();
  return Response::FallThrough();
}

Response PageHandler::ScreencastFrameAck(int session_id) {
  if (session_id == session_id_)
    --frames_in_flight_;
  return Response::Success();
}

Response PageHandler::HandleJavaScriptDialog(bool accept,
                                             Maybe<std::string> prompt_text) {
  ResponseOrWebContents result = GetWebContentsForTopLevelActiveFrame();
  if (absl::holds_alternative<Response>(result))
    return absl::get<Response>(result);

  if (pending_dialog_.is_null())
    return Response::InvalidParams("No dialog is showing");

  std::u16string prompt_override;
  if (prompt_text.has_value()) {
    prompt_override = base::UTF8ToUTF16(prompt_text.value());
  }
  std::move(pending_dialog_).Run(accept, prompt_override);

  // Clean up the dialog UI if any.
  WebContentsImpl* web_contents = absl::get<WebContentsImpl*>(result);
  if (web_contents->GetDelegate()) {
    JavaScriptDialogManager* manager =
        web_contents->GetDelegate()->GetJavaScriptDialogManager(web_contents);
    if (manager) {
      manager->HandleJavaScriptDialog(
          web_contents, accept,
          prompt_text.has_value() ? &prompt_override : nullptr);
    }
  }

  return Response::Success();
}

Response PageHandler::BringToFront() {
  // Not using AssureTopLevelActiveFrame here because
  // we allow bringing WebContents to front that might have ongoing
  // lifecycle updates.
  if (!host_) {
    return Response::ServerError(kErrorNotAttached);
  }

  if (host_->GetParentOrOuterDocument()) {
    return Response::ServerError(kCommandIsOnlyAvailableAtTopTarget);
  }

  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_));
  web_contents->Activate();
  web_contents->GetOutermostWebContents()->Focus();
  return Response::Success();
}

Response PageHandler::SetDownloadBehavior(const std::string& behavior,
                                          Maybe<std::string> download_path) {
  BrowserContext* browser_context =
      host_ ? host_->GetProcess()->GetBrowserContext() : nullptr;
  if (!browser_context)
    return Response::ServerError("Could not fetch browser context");

  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;
  if (!browser_handler_)
    return Response::ServerError("Cannot not access browser-level commands");
  return browser_handler_->DoSetDownloadBehavior(behavior, browser_context,
                                                 std::move(download_path));
}

void PageHandler::GetAppManifest(
    protocol::Maybe<std::string> manifest_id,
    std::unique_ptr<GetAppManifestCallback> callback) {
  if (!CanExecuteGlobalCommands(this, callback))
    return;
  ManifestManagerHost::GetOrCreateForPage(host_->GetPage())
      ->RequestManifestDebugInfo(base::BindOnce(
          GotManifest, std::move(manifest_id), std::move(callback)));
}

PageHandler::ResponseOrWebContents
PageHandler::GetWebContentsForTopLevelActiveFrame() {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError())
    return response;

  return static_cast<WebContentsImpl*>(WebContents::FromRenderFrameHost(host_));
}

void PageHandler::NotifyScreencastVisibility(bool visible) {
  frontend_->ScreencastVisibilityChanged(visible);
}

bool PageHandler::ShouldCaptureNextScreencastFrame() {
  return frames_in_flight_ <= kMaxScreencastFramesInFlight &&
         !(++frame_counter_ % capture_every_nth_frame_);
}

void PageHandler::OnFrameFromVideoConsumer(
    scoped_refptr<media::VideoFrame> frame) {
  if (!host_)
    return;

  if (!ShouldCaptureNextScreencastFrame())
    return;

  RenderWidgetHostViewBase* const view =
      static_cast<RenderWidgetHostViewBase*>(host_->GetView());
  if (!view)
    return;

  const gfx::Size surface_size = view->GetCompositorViewportPixelSize();
  if (surface_size.IsEmpty())
    return;

  // If window has been resized, set the new dimensions.
  if (surface_size != last_surface_size_) {
    last_surface_size_ = surface_size;
    gfx::Size snapshot_size = DetermineSnapshotSize(
        surface_size, screencast_max_width_, screencast_max_height_);
    if (!snapshot_size.IsEmpty())
      video_consumer_->SetMinAndMaxFrameSize(snapshot_size, snapshot_size);
    return;
  }

  double device_scale_factor, page_scale_factor;
  double top_controls_visible_height;
  gfx::PointF root_scroll_offset;
  GetMetadataFromFrame(*frame, &device_scale_factor, &page_scale_factor,
                       &root_scroll_offset, &top_controls_visible_height);
  std::unique_ptr<Page::ScreencastFrameMetadata> page_metadata =
      BuildScreencastFrameMetadata(surface_size, device_scale_factor,
                                   page_scale_factor, root_scroll_offset,
                                   top_controls_visible_height);
  if (!page_metadata)
    return;

  frames_in_flight_++;
  ScreencastFrameCaptured(std::move(page_metadata),
                          DevToolsVideoConsumer::GetSkBitmapFromFrame(frame));
}

void PageHandler::ScreencastFrameCaptured(
    std::unique_ptr<Page::ScreencastFrameMetadata> page_metadata,
    const SkBitmap& bitmap) {
  if (bitmap.drawsNothing()) {
    --frames_in_flight_;
    return;
  }
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(
          [](const SkBitmap& bitmap,
             BitmapEncoder encoder) -> std::vector<uint8_t> {
            std::vector<uint8_t> result;
            encoder.Run(bitmap, result);
            return result;
          },
          bitmap, screencast_encoder_),
      base::BindOnce(&PageHandler::ScreencastFrameEncoded,
                     weak_factory_.GetWeakPtr(), std::move(page_metadata)));
}

void PageHandler::ScreencastFrameEncoded(
    std::unique_ptr<Page::ScreencastFrameMetadata> page_metadata,
    std::vector<uint8_t> data) {
  if (data.empty()) {
    --frames_in_flight_;
    return;  // Encode failed.
  }

  frontend_->ScreencastFrame(Binary::fromVector(std::move(data)),
                             std::move(page_metadata), session_id_);
}

void PageHandler::ScreenshotCaptured(
    std::unique_ptr<PendingScreenshotRequest> request,
    const gfx::Image& image) {
  if (request->original_view_size.width()) {
    RenderWidgetHostImpl* widget_host = host_->GetRenderWidgetHost();
    widget_host->GetView()->SetSize(request->original_view_size);
    emulation_handler_->SetDeviceEmulationParams(
        request->original_emulation_params);
  }

  if (request->original_web_prefs) {
    host_->render_view_host()->GetDelegate()->SetWebPreferences(
        *request->original_web_prefs);
  }

  if (image.IsEmpty()) {
    request->callback->sendFailure(
        Response::ServerError("Unable to capture screenshot"));
    return;
  }

  std::vector<uint8_t> encoded_bitmap;
  const SkBitmap& bitmap = *image.ToSkBitmap();

  if (!request->requested_image_size.IsEmpty() &&
      (image.Width() != request->requested_image_size.width() ||
       image.Height() != request->requested_image_size.height())) {
    SkBitmap cropped = SkBitmapOperations::CreateTiledBitmap(
        bitmap, 0, 0, request->requested_image_size.width(),
        request->requested_image_size.height());
    request->encoder.Run(cropped, encoded_bitmap);
  } else {
    request->encoder.Run(bitmap, encoded_bitmap);
  }
  // TODO(caseq): send failure if we fail to encode?
  request->callback->sendSuccess(Binary::fromVector(std::move(encoded_bitmap)));
}

Response PageHandler::StopLoading() {
  ResponseOrWebContents result = GetWebContentsForTopLevelActiveFrame();

  if (absl::holds_alternative<Response>(result))
    return absl::get<Response>(result);

  WebContentsImpl* web_contents = absl::get<WebContentsImpl*>(result);
  web_contents->Stop();
  return Response::Success();
}

Response PageHandler::SetWebLifecycleState(const std::string& state) {
  // Inactive pages(e.g., a prerendered or back-forward cached page) should not
  // affect the state.
  ResponseOrWebContents result = GetWebContentsForTopLevelActiveFrame();
  if (absl::holds_alternative<Response>(result))
    return absl::get<Response>(result);

  WebContentsImpl* web_contents = absl::get<WebContentsImpl*>(result);
  if (state == Page::SetWebLifecycleState::StateEnum::Frozen) {
    // TODO(fmeawad): Instead of forcing a visibility change, only allow
    // freezing a page if it was already hidden.
    web_contents->WasHidden();
    web_contents->SetPageFrozen(true);
    return Response::Success();
  }
  if (state == Page::SetWebLifecycleState::StateEnum::Active) {
    web_contents->SetPageFrozen(false);
    return Response::Success();
  }
  return Response::ServerError("Unidentified lifecycle state");
}

void PageHandler::GetInstallabilityErrors(
    std::unique_ptr<GetInstallabilityErrorsCallback> callback) {
  auto installability_errors =
      std::make_unique<protocol::Array<Page::InstallabilityError>>();
  // TODO: Use InstallableManager once it moves into content/.
  // Until then, this code is only used to return empty array in the tests.
  callback->sendSuccess(std::move(installability_errors));
}

void PageHandler::GetManifestIcons(
    std::unique_ptr<GetManifestIconsCallback> callback) {
  // TODO: Use InstallableManager once it moves into content/.
  // Until then, this code is only used to return no image data in the tests.
  callback->sendSuccess(Maybe<Binary>());
}

void PageHandler::GetAppId(std::unique_ptr<GetAppIdCallback> callback) {
  // TODO: Use InstallableManager once it moves into content/.
  // Until then, this code is only used to return no image data in the tests.
  callback->sendSuccess(protocol::Maybe<protocol::String>(),
                        protocol::Maybe<protocol::String>());
}

Response PageHandler::SetBypassCSP(bool enabled) {
  bypass_csp_ = enabled;
  return Response::FallThrough();
}

Page::BackForwardCacheNotRestoredReason NotRestoredReasonToProtocol(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;
  switch (reason) {
    case Reason::kNotPrimaryMainFrame:
      return Page::BackForwardCacheNotRestoredReasonEnum::NotPrimaryMainFrame;
    case Reason::kBackForwardCacheDisabled:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          BackForwardCacheDisabled;
    case Reason::kRelatedActiveContentsExist:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RelatedActiveContentsExist;
    case Reason::kHTTPStatusNotOK:
      return Page::BackForwardCacheNotRestoredReasonEnum::HTTPStatusNotOK;
    case Reason::kSchemeNotHTTPOrHTTPS:
      return Page::BackForwardCacheNotRestoredReasonEnum::SchemeNotHTTPOrHTTPS;
    case Reason::kLoading:
      return Page::BackForwardCacheNotRestoredReasonEnum::Loading;
    case Reason::kDisableForRenderFrameHostCalled:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          DisableForRenderFrameHostCalled;
    case Reason::kDomainNotAllowed:
      return Page::BackForwardCacheNotRestoredReasonEnum::DomainNotAllowed;
    case Reason::kHTTPMethodNotGET:
      return Page::BackForwardCacheNotRestoredReasonEnum::HTTPMethodNotGET;
    case Reason::kSubframeIsNavigating:
      return Page::BackForwardCacheNotRestoredReasonEnum::SubframeIsNavigating;
    case Reason::kTimeout:
      return Page::BackForwardCacheNotRestoredReasonEnum::Timeout;
    case Reason::kCacheLimit:
      return Page::BackForwardCacheNotRestoredReasonEnum::CacheLimit;
    case Reason::kJavaScriptExecution:
      return Page::BackForwardCacheNotRestoredReasonEnum::JavaScriptExecution;
    case Reason::kRendererProcessKilled:
      return Page::BackForwardCacheNotRestoredReasonEnum::RendererProcessKilled;
    case Reason::kRendererProcessCrashed:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RendererProcessCrashed;
    case Reason::kConflictingBrowsingInstance:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          ConflictingBrowsingInstance;
    case Reason::kCacheFlushed:
      return Page::BackForwardCacheNotRestoredReasonEnum::CacheFlushed;
    case Reason::kServiceWorkerVersionActivation:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          ServiceWorkerVersionActivation;
    case Reason::kSessionRestored:
      return Page::BackForwardCacheNotRestoredReasonEnum::SessionRestored;
    case Reason::kServiceWorkerPostMessage:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          ServiceWorkerPostMessage;
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          EnteredBackForwardCacheBeforeServiceWorkerHostAdded;
    case Reason::kServiceWorkerClaim:
      return Page::BackForwardCacheNotRestoredReasonEnum::ServiceWorkerClaim;
    case Reason::kIgnoreEventAndEvict:
      return Page::BackForwardCacheNotRestoredReasonEnum::IgnoreEventAndEvict;
    case Reason::kHaveInnerContents:
      return Page::BackForwardCacheNotRestoredReasonEnum::HaveInnerContents;
    case Reason::kTimeoutPuttingInCache:
      return Page::BackForwardCacheNotRestoredReasonEnum::TimeoutPuttingInCache;
    case Reason::kBackForwardCacheDisabledByLowMemory:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          BackForwardCacheDisabledByLowMemory;
    case Reason::kBackForwardCacheDisabledByCommandLine:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          BackForwardCacheDisabledByCommandLine;
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          NetworkRequestDatapipeDrainedAsBytesConsumer;
    case Reason::kNetworkRequestRedirected:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          NetworkRequestRedirected;
    case Reason::kNetworkRequestTimeout:
      return Page::BackForwardCacheNotRestoredReasonEnum::NetworkRequestTimeout;
    case Reason::kNetworkExceedsBufferLimit:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          NetworkExceedsBufferLimit;
    case Reason::kNavigationCancelledWhileRestoring:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          NavigationCancelledWhileRestoring;
    case Reason::kUserAgentOverrideDiffers:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          UserAgentOverrideDiffers;
    case Reason::kForegroundCacheLimit:
      return Page::BackForwardCacheNotRestoredReasonEnum::ForegroundCacheLimit;
    case Reason::kBrowsingInstanceNotSwapped:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          BrowsingInstanceNotSwapped;
    case Reason::kBackForwardCacheDisabledForDelegate:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          BackForwardCacheDisabledForDelegate;
    case Reason::kUnloadHandlerExistsInMainFrame:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          UnloadHandlerExistsInMainFrame;
    case Reason::kUnloadHandlerExistsInSubFrame:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          UnloadHandlerExistsInSubFrame;
    case Reason::kServiceWorkerUnregistration:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          ServiceWorkerUnregistration;
    case Reason::kCacheControlNoStore:
      return Page::BackForwardCacheNotRestoredReasonEnum::CacheControlNoStore;
    case Reason::kCacheControlNoStoreCookieModified:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          CacheControlNoStoreCookieModified;
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          CacheControlNoStoreHTTPOnlyCookieModified;
    case Reason::kErrorDocument:
      return Page::BackForwardCacheNotRestoredReasonEnum::ErrorDocument;
    case Reason::kCookieDisabled:
      return Page::BackForwardCacheNotRestoredReasonEnum::CookieDisabled;
    case Reason::kHTTPAuthRequired:
      return Page::BackForwardCacheNotRestoredReasonEnum::HTTPAuthRequired;
    case Reason::kCookieFlushed:
      return Page::BackForwardCacheNotRestoredReasonEnum::CookieFlushed;
    case Reason::kBroadcastChannelOnMessage:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          BroadcastChannelOnMessage;
    case Reason::kWebViewSettingsChanged:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          WebViewSettingsChanged;
    case Reason::kWebViewJavaScriptObjectChanged:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          WebViewJavaScriptObjectChanged;
    case Reason::kWebViewMessageListenerInjected:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          WebViewMessageListenerInjected;
    case Reason::kWebViewSafeBrowsingAllowlistChanged:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          WebViewSafeBrowsingAllowlistChanged;
    case Reason::kWebViewDocumentStartJavascriptChanged:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          WebViewDocumentStartJavascriptChanged;
    case Reason::kBlocklistedFeatures:
      // Blocklisted features should be handled separately and be broken down
      // into sub reasons.
      NOTREACHED_IN_MIGRATION();
      return Page::BackForwardCacheNotRestoredReasonEnum::Unknown;
    case Reason::kUnknown:
      return Page::BackForwardCacheNotRestoredReasonEnum::Unknown;
  }
}

using blink::scheduler::WebSchedulerTrackedFeature;
Page::BackForwardCacheNotRestoredReason BlocklistedFeatureToProtocol(
    WebSchedulerTrackedFeature feature) {
  switch (feature) {
    case WebSchedulerTrackedFeature::kWebSocket:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebSocket;
    case WebSchedulerTrackedFeature::kWebSocketSticky:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebSocketSticky;
    case WebSchedulerTrackedFeature::kWebTransport:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebTransport;
    case WebSchedulerTrackedFeature::kWebTransportSticky:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebTransportSticky;
    case WebSchedulerTrackedFeature::kWebRTC:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebRTC;
    case WebSchedulerTrackedFeature::kWebRTCSticky:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebRTCSticky;
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          MainResourceHasCacheControlNoCache;
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          MainResourceHasCacheControlNoStore;
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          SubresourceHasCacheControlNoCache;
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          SubresourceHasCacheControlNoStore;
    case WebSchedulerTrackedFeature::kContainsPlugins:
      return Page::BackForwardCacheNotRestoredReasonEnum::ContainsPlugins;
    case WebSchedulerTrackedFeature::kDocumentLoaded:
      return Page::BackForwardCacheNotRestoredReasonEnum::DocumentLoaded;
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          OutstandingNetworkRequestOthers;
    case WebSchedulerTrackedFeature::kRequestedMIDIPermission:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RequestedMIDIPermission;
    case WebSchedulerTrackedFeature::kRequestedAudioCapturePermission:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RequestedAudioCapturePermission;
    case WebSchedulerTrackedFeature::kRequestedVideoCapturePermission:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RequestedVideoCapturePermission;
    case WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RequestedBackForwardCacheBlockedSensors;
    case WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RequestedBackgroundWorkPermission;
    case WebSchedulerTrackedFeature::kBroadcastChannel:
      return Page::BackForwardCacheNotRestoredReasonEnum::BroadcastChannel;
    case WebSchedulerTrackedFeature::kWebXR:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebXR;
    case WebSchedulerTrackedFeature::kSharedWorker:
      return Page::BackForwardCacheNotRestoredReasonEnum::SharedWorker;
    case WebSchedulerTrackedFeature::kWebLocks:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebLocks;
    case WebSchedulerTrackedFeature::kWebHID:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebHID;
    case WebSchedulerTrackedFeature::kWebShare:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebShare;
    case WebSchedulerTrackedFeature::kRequestedStorageAccessGrant:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          RequestedStorageAccessGrant;
    case WebSchedulerTrackedFeature::kWebNfc:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebNfc;
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          OutstandingNetworkRequestFetch;
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          OutstandingNetworkRequestXHR;
    case WebSchedulerTrackedFeature::kPrinting:
      return Page::BackForwardCacheNotRestoredReasonEnum::Printing;
    case WebSchedulerTrackedFeature::kWebDatabase:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebDatabase;
    case WebSchedulerTrackedFeature::kPictureInPicture:
      return Page::BackForwardCacheNotRestoredReasonEnum::PictureInPicture;
    case WebSchedulerTrackedFeature::kSpeechRecognizer:
      return Page::BackForwardCacheNotRestoredReasonEnum::SpeechRecognizer;
    case WebSchedulerTrackedFeature::kIdleManager:
      return Page::BackForwardCacheNotRestoredReasonEnum::IdleManager;
    case WebSchedulerTrackedFeature::kPaymentManager:
      return Page::BackForwardCacheNotRestoredReasonEnum::PaymentManager;
    case WebSchedulerTrackedFeature::kKeyboardLock:
      return Page::BackForwardCacheNotRestoredReasonEnum::KeyboardLock;
    case WebSchedulerTrackedFeature::kWebOTPService:
      return Page::BackForwardCacheNotRestoredReasonEnum::WebOTPService;
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          OutstandingNetworkRequestDirectSocket;
    case WebSchedulerTrackedFeature::kInjectedJavascript:
      return Page::BackForwardCacheNotRestoredReasonEnum::InjectedJavascript;
    case WebSchedulerTrackedFeature::kInjectedStyleSheet:
      return Page::BackForwardCacheNotRestoredReasonEnum::InjectedStyleSheet;
    case WebSchedulerTrackedFeature::kKeepaliveRequest:
      return Page::BackForwardCacheNotRestoredReasonEnum::KeepaliveRequest;
    case WebSchedulerTrackedFeature::kIndexedDBEvent:
      return Page::BackForwardCacheNotRestoredReasonEnum::IndexedDBEvent;
    case WebSchedulerTrackedFeature::kDummy:
      // This is a test only reason and should never be called.
      NOTREACHED_IN_MIGRATION();
      return Page::BackForwardCacheNotRestoredReasonEnum::Dummy;
    case WebSchedulerTrackedFeature::
        kJsNetworkRequestReceivedCacheControlNoStoreResource:
      return Page::BackForwardCacheNotRestoredReasonEnum::
          JsNetworkRequestReceivedCacheControlNoStoreResource;
    case WebSchedulerTrackedFeature::kWebSerial:
      // Currently we add WebSchedulerTrackedFeature::kWebSerial only for
      // disabling aggressive throttling.
      NOTREACHED();
    case WebSchedulerTrackedFeature::kSmartCard:
      return Page::BackForwardCacheNotRestoredReasonEnum::SmartCard;
    case WebSchedulerTrackedFeature::kLiveMediaStreamTrack:
      return Page::BackForwardCacheNotRestoredReasonEnum::LiveMediaStreamTrack;
    case WebSchedulerTrackedFeature::kUnloadHandler:
      return Page::BackForwardCacheNotRestoredReasonEnum::UnloadHandler;
    case WebSchedulerTrackedFeature::kParserAborted:
      return Page::BackForwardCacheNotRestoredReasonEnum::ParserAborted;
  }
}

std::unique_ptr<Page::BackForwardCacheBlockingDetails> SourceLocationToProtocol(
    const blink::mojom::ScriptSourceLocationPtr& source) {
  auto blocking_details = Page::BackForwardCacheBlockingDetails::Create();
  if (!source->url.is_empty()) {
    blocking_details.SetUrl(source->url.spec());
  }
  if (!source->function_name.empty()) {
    blocking_details.SetFunction(source->function_name);
  }
  CHECK(source->line_number > 0);
  CHECK(source->column_number > 0);
  return blocking_details.SetLineNumber(source->line_number - 1)
      .SetColumnNumber(source->column_number - 1)
      .Build();
}

Page::BackForwardCacheNotRestoredReason
DisableForRenderFrameHostReasonToProtocol(
    BackForwardCache::DisabledReason reason) {
  switch (reason.source) {
    case BackForwardCache::DisabledSource::kLegacy:
      NOTREACHED_IN_MIGRATION();
      return Page::BackForwardCacheNotRestoredReasonEnum::Unknown;
    case BackForwardCache::DisabledSource::kTesting:
      NOTREACHED_IN_MIGRATION();
      return Page::BackForwardCacheNotRestoredReasonEnum::Unknown;
    case BackForwardCache::DisabledSource::kContent:
      switch (
          static_cast<BackForwardCacheDisable::DisabledReasonId>(reason.id)) {
        case BackForwardCacheDisable::DisabledReasonId::kUnknown:
          return Page::BackForwardCacheNotRestoredReasonEnum::Unknown;
        case BackForwardCacheDisable::DisabledReasonId::kSecurityHandler:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentSecurityHandler;
        case BackForwardCacheDisable::DisabledReasonId::kWebAuthenticationAPI:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentWebAuthenticationAPI;
        case BackForwardCacheDisable::DisabledReasonId::kFileChooser:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentFileChooser;
        case BackForwardCacheDisable::DisabledReasonId::kSerial:
          return Page::BackForwardCacheNotRestoredReasonEnum::ContentSerial;
        case BackForwardCacheDisable::DisabledReasonId::
            kMediaDevicesDispatcherHost:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentMediaDevicesDispatcherHost;
        case BackForwardCacheDisable::DisabledReasonId::kWebBluetooth:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentWebBluetooth;
        case BackForwardCacheDisable::DisabledReasonId::kWebUSB:
          return Page::BackForwardCacheNotRestoredReasonEnum::ContentWebUSB;
        case BackForwardCacheDisable::DisabledReasonId::kMediaSessionService:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentMediaSessionService;
        case BackForwardCacheDisable::DisabledReasonId::kScreenReader:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              ContentScreenReader;
        case BackForwardCacheDisable::DisabledReasonId::kDiscarded:
          return Page::BackForwardCacheNotRestoredReasonEnum::ContentDiscarded;
      }
    case BackForwardCache::DisabledSource::kEmbedder:
      switch (static_cast<back_forward_cache::DisabledReasonId>(reason.id)) {
        case back_forward_cache::DisabledReasonId::kUnknown:
          return Page::BackForwardCacheNotRestoredReasonEnum::Unknown;
        case back_forward_cache::DisabledReasonId::kPopupBlockerTabHelper:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderPopupBlockerTabHelper;
        case back_forward_cache::DisabledReasonId::
            kSafeBrowsingTriggeredPopupBlocker:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderSafeBrowsingTriggeredPopupBlocker;
        case back_forward_cache::DisabledReasonId::kSafeBrowsingThreatDetails:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderSafeBrowsingThreatDetails;
        case back_forward_cache::DisabledReasonId::kDomDistillerViewerSource:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderDomDistillerViewerSource;
        case back_forward_cache::DisabledReasonId::
            kDomDistiller_SelfDeletingRequestDelegate:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderDomDistillerSelfDeletingRequestDelegate;
        case back_forward_cache::DisabledReasonId::kOomInterventionTabHelper:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderOomInterventionTabHelper;
        case back_forward_cache::DisabledReasonId::kOfflinePage:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderOfflinePage;
        case back_forward_cache::DisabledReasonId::
            kChromePasswordManagerClient_BindCredentialManager:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderChromePasswordManagerClientBindCredentialManager;
        case back_forward_cache::DisabledReasonId::kPermissionRequestManager:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderPermissionRequestManager;
        case back_forward_cache::DisabledReasonId::kModalDialog:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderModalDialog;
        case back_forward_cache::DisabledReasonId::kExtensionMessaging:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderExtensionMessaging;
        case back_forward_cache::DisabledReasonId::
            kExtensionSentMessageToCachedFrame:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              EmbedderExtensionSentMessageToCachedFrame;
        case back_forward_cache::DisabledReasonId::kRequestedByWebViewClient:
          return Page::BackForwardCacheNotRestoredReasonEnum::
              RequestedByWebViewClient;
      }
  }
}

Page::BackForwardCacheNotRestoredReasonType MapNotRestoredReasonToType(
    BackForwardCacheMetrics::NotRestoredReason reason) {
  using Reason = BackForwardCacheMetrics::NotRestoredReason;
  switch (reason) {
    case Reason::kNotPrimaryMainFrame:
    case Reason::kBackForwardCacheDisabled:
    case Reason::kRelatedActiveContentsExist:
    case Reason::kHTTPStatusNotOK:
    case Reason::kSchemeNotHTTPOrHTTPS:
    case Reason::kLoading:
    case Reason::kDisableForRenderFrameHostCalled:
    case Reason::kDomainNotAllowed:
    case Reason::kHTTPMethodNotGET:
    case Reason::kSubframeIsNavigating:
    case Reason::kTimeout:
    case Reason::kCacheLimit:
    case Reason::kJavaScriptExecution:
    case Reason::kRendererProcessKilled:
    case Reason::kRendererProcessCrashed:
    case Reason::kConflictingBrowsingInstance:
    case Reason::kCacheFlushed:
    case Reason::kServiceWorkerVersionActivation:
    case Reason::kSessionRestored:
    case Reason::kServiceWorkerPostMessage:
    case Reason::kEnteredBackForwardCacheBeforeServiceWorkerHostAdded:
    case Reason::kServiceWorkerClaim:
    case Reason::kIgnoreEventAndEvict:
    case Reason::kHaveInnerContents:
    case Reason::kTimeoutPuttingInCache:
    case Reason::kBackForwardCacheDisabledByLowMemory:
    case Reason::kBackForwardCacheDisabledByCommandLine:
    case Reason::kNetworkRequestRedirected:
    case Reason::kNetworkRequestTimeout:
    case Reason::kNetworkExceedsBufferLimit:
    case Reason::kNavigationCancelledWhileRestoring:
    case Reason::kForegroundCacheLimit:
    case Reason::kUserAgentOverrideDiffers:
    case Reason::kBrowsingInstanceNotSwapped:
    case Reason::kBackForwardCacheDisabledForDelegate:
    case Reason::kServiceWorkerUnregistration:
    case Reason::kErrorDocument:
    case Reason::kCookieDisabled:
    case Reason::kHTTPAuthRequired:
    case Reason::kCookieFlushed:
    case Reason::kBroadcastChannelOnMessage:
    case Reason::kWebViewSettingsChanged:
    case Reason::kWebViewJavaScriptObjectChanged:
    case Reason::kWebViewMessageListenerInjected:
    case Reason::kWebViewSafeBrowsingAllowlistChanged:
    case Reason::kWebViewDocumentStartJavascriptChanged:
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::Circumstantial;
    case Reason::kCacheControlNoStore:
    case Reason::kCacheControlNoStoreCookieModified:
    case Reason::kCacheControlNoStoreHTTPOnlyCookieModified:
    case Reason::kUnloadHandlerExistsInMainFrame:
    case Reason::kUnloadHandlerExistsInSubFrame:
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::PageSupportNeeded;
    case Reason::kNetworkRequestDatapipeDrainedAsBytesConsumer:
    case Reason::kUnknown:
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::SupportPending;
    case Reason::kBlocklistedFeatures:
      NOTREACHED_IN_MIGRATION();
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::PageSupportNeeded;
  }
}

Page::BackForwardCacheNotRestoredReasonType MapBlocklistedFeatureToType(
    WebSchedulerTrackedFeature feature) {
  switch (feature) {
    case WebSchedulerTrackedFeature::kWebRTC:
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestOthers:
    case WebSchedulerTrackedFeature::kBroadcastChannel:
    case WebSchedulerTrackedFeature::kWebXR:
    case WebSchedulerTrackedFeature::kSharedWorker:
    case WebSchedulerTrackedFeature::kWebHID:
    case WebSchedulerTrackedFeature::kWebShare:
    case WebSchedulerTrackedFeature::kWebDatabase:
    case WebSchedulerTrackedFeature::kPaymentManager:
    case WebSchedulerTrackedFeature::kKeyboardLock:
    case WebSchedulerTrackedFeature::kWebOTPService:
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestDirectSocket:
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestFetch:
    case WebSchedulerTrackedFeature::kOutstandingNetworkRequestXHR:
    case WebSchedulerTrackedFeature::kWebTransport:
    case WebSchedulerTrackedFeature::kIndexedDBEvent:
    case WebSchedulerTrackedFeature::kSmartCard:
    case WebSchedulerTrackedFeature::kLiveMediaStreamTrack:
    case WebSchedulerTrackedFeature::kUnloadHandler:
    case WebSchedulerTrackedFeature::kParserAborted:
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::PageSupportNeeded;
    case WebSchedulerTrackedFeature::kWebNfc:
    case WebSchedulerTrackedFeature::kRequestedStorageAccessGrant:
    case WebSchedulerTrackedFeature::kRequestedMIDIPermission:
    case WebSchedulerTrackedFeature::kRequestedAudioCapturePermission:
    case WebSchedulerTrackedFeature::kRequestedVideoCapturePermission:
    case WebSchedulerTrackedFeature::kRequestedBackForwardCacheBlockedSensors:
    case WebSchedulerTrackedFeature::kRequestedBackgroundWorkPermission:
    case WebSchedulerTrackedFeature::kContainsPlugins:
    case WebSchedulerTrackedFeature::kIdleManager:
    case WebSchedulerTrackedFeature::kSpeechRecognizer:
    case WebSchedulerTrackedFeature::kPrinting:
    case WebSchedulerTrackedFeature::kPictureInPicture:
    case WebSchedulerTrackedFeature::kWebLocks:
    case WebSchedulerTrackedFeature::kWebSocket:
    case WebSchedulerTrackedFeature::kKeepaliveRequest:
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::SupportPending;
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoStore:
    case WebSchedulerTrackedFeature::kMainResourceHasCacheControlNoCache:
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoCache:
    case WebSchedulerTrackedFeature::kSubresourceHasCacheControlNoStore:
    case WebSchedulerTrackedFeature::kInjectedStyleSheet:
    case WebSchedulerTrackedFeature::kInjectedJavascript:
    case WebSchedulerTrackedFeature::kDocumentLoaded:
    case WebSchedulerTrackedFeature::kDummy:
    case WebSchedulerTrackedFeature::
        kJsNetworkRequestReceivedCacheControlNoStoreResource:
    case WebSchedulerTrackedFeature::kWebRTCSticky:
    case WebSchedulerTrackedFeature::kWebTransportSticky:
    case WebSchedulerTrackedFeature::kWebSocketSticky:
      return Page::BackForwardCacheNotRestoredReasonTypeEnum::Circumstantial;
    case WebSchedulerTrackedFeature::kWebSerial:
      NOTREACHED();
  }
}

Page::BackForwardCacheNotRestoredReasonType
MapDisableForRenderFrameHostReasonToType(
    BackForwardCache::DisabledReason reason) {
  return Page::BackForwardCacheNotRestoredReasonTypeEnum::SupportPending;
}

using BlockingDetailsMap =
    std::map<blink::scheduler::WebSchedulerTrackedFeature,
             std::vector<blink::mojom::BlockingDetailsPtr>>;

std::unique_ptr<protocol::Array<Page::BackForwardCacheNotRestoredExplanation>>
CreateNotRestoredExplanation(
    const BackForwardCacheCanStoreDocumentResult::NotRestoredReasons
        not_restored_reasons,
    const blink::scheduler::WebSchedulerTrackedFeatures blocklisted_features,
    const BackForwardCacheCanStoreDocumentResult::DisabledReasonsMap&
        disabled_reasons,
    const BlockingDetailsMap& details) {
  auto reasons = std::make_unique<
      protocol::Array<Page::BackForwardCacheNotRestoredExplanation>>();

  for (BackForwardCacheMetrics::NotRestoredReason not_restored_reason :
       not_restored_reasons) {
    if (not_restored_reason ==
        BackForwardCacheMetrics::NotRestoredReason::kBlocklistedFeatures) {
      DCHECK(!blocklisted_features.empty());
      for (blink::scheduler::WebSchedulerTrackedFeature feature :
           blocklisted_features) {
        // Details are not always present for blocklisted features, because the
        // number of details reported is limited.
        auto details_list = std::make_unique<
            protocol::Array<Page::BackForwardCacheBlockingDetails>>();
        CHECK(details.contains(feature));
        for (const auto& detail : details.at(feature)) {
          if (detail->source) {
            details_list->push_back(SourceLocationToProtocol(detail->source));
          }
        }
        auto explanation =
            Page::BackForwardCacheNotRestoredExplanation::Create()
                .SetType(MapBlocklistedFeatureToType(feature))
                .SetReason(BlocklistedFeatureToProtocol(feature))
                .Build();

        if (!details_list->empty()) {
          explanation->SetDetails(std::move(details_list));
        }

        reasons->emplace_back(std::move(explanation));
      }
    } else if (not_restored_reason ==
               BackForwardCacheMetrics::NotRestoredReason::
                   kDisableForRenderFrameHostCalled) {
      for (const auto& [disabled_reason, _] : disabled_reasons) {
        auto reason =
            Page::BackForwardCacheNotRestoredExplanation::Create()
                .SetType(
                    MapDisableForRenderFrameHostReasonToType(disabled_reason))
                .SetReason(
                    DisableForRenderFrameHostReasonToProtocol(disabled_reason))
                .Build();
        if (!disabled_reason.context.empty())
          reason->SetContext(disabled_reason.context);
        reasons->emplace_back(std::move(reason));
      }
    } else {
      reasons->emplace_back(
          Page::BackForwardCacheNotRestoredExplanation::Create()
              .SetType(MapNotRestoredReasonToType(not_restored_reason))
              .SetReason(NotRestoredReasonToProtocol(not_restored_reason))
              .Build());
    }
  }
  return reasons;
}

std::unique_ptr<Page::BackForwardCacheNotRestoredExplanationTree>
CreateNotRestoredExplanationTree(
    const BackForwardCacheCanStoreTreeResult& tree_result) {
  auto explanation = CreateNotRestoredExplanation(
      tree_result.GetDocumentResult().not_restored_reasons(),
      tree_result.GetDocumentResult().blocklisted_features(),
      tree_result.GetDocumentResult().disabled_reasons(),
      tree_result.GetDocumentResult().blocking_details_map());

  auto children_array = std::make_unique<
      protocol::Array<Page::BackForwardCacheNotRestoredExplanationTree>>();
  for (auto& child : tree_result.GetChildren()) {
    children_array->emplace_back(
        CreateNotRestoredExplanationTree(*(child.get())));
  }
  return Page::BackForwardCacheNotRestoredExplanationTree::Create()
      .SetUrl(tree_result.GetUrl().spec())
      .SetExplanations(std::move(explanation))
      .SetChildren(std::move(children_array))
      .Build();
}

Response PageHandler::AddCompilationCache(const std::string& url,
                                          const Binary& data) {
  // We're just checking a permission here, the real business happens
  // in the renderer, if we fall through.
  if (allow_unsafe_operations_)
    return Response::FallThrough();
  return Response::ServerError("Permission denied");
}

void PageHandler::IsPrerenderingAllowed(bool& is_allowed) {
  is_allowed &= is_prerendering_allowed_;
}

Response PageHandler::SetPrerenderingAllowed(bool is_allowed) {
  Response response = AssureTopLevelActiveFrame();
  if (response.IsError()) {
    return response;
  }

  is_prerendering_allowed_ = is_allowed;

  return Response::Success();
}

Response PageHandler::AssureTopLevelActiveFrame() {
  if (!host_)
    return Response::ServerError(kErrorNotAttached);

  if (host_->GetParentOrOuterDocument())
    return Response::ServerError(kCommandIsOnlyAvailableAtTopTarget);

  if (!host_->IsActive())
    return Response::ServerError(kErrorInactivePage);

  return Response::Success();
}

void PageHandler::BackForwardCacheNotUsed(
    const NavigationRequest* navigation,
    const BackForwardCacheCanStoreDocumentResult* result,
    const BackForwardCacheCanStoreTreeResult* tree_result) {
  if (!enabled_)
    return;

  FrameTreeNode* ftn = navigation->frame_tree_node();
  std::string devtools_navigation_token =
      navigation->devtools_navigation_token().ToString();
  std::string frame_id =
      ftn->current_frame_host()->devtools_frame_token().ToString();

  auto explanation = CreateNotRestoredExplanation(
      result->not_restored_reasons(), result->blocklisted_features(),
      result->disabled_reasons(), result->blocking_details_map());

  // TODO(crbug.com/40812472): |tree_result| should not be nullptr when |result|
  // has the reasons.
  std::unique_ptr<Page::BackForwardCacheNotRestoredExplanationTree>
      explanation_tree =
          tree_result ? CreateNotRestoredExplanationTree(*tree_result)
                      : nullptr;
  frontend_->BackForwardCacheNotUsed(devtools_navigation_token, frame_id,
                                     std::move(explanation),
                                     std::move(explanation_tree));
}

bool PageHandler::ShouldBypassCSP() {
  return enabled_ && bypass_csp_;
}

}  // namespace protocol
}  // namespace content
