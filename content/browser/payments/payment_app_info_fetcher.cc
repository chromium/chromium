// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_info_fetcher.h"

#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/optional.h"
#include "base/task/post_task.h"
#include "components/payments/content/icon/icon_size.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

namespace content {

PaymentAppInfoFetcher::PaymentAppInfo::PaymentAppInfo() {}

PaymentAppInfoFetcher::PaymentAppInfo::~PaymentAppInfo() {}

void PaymentAppInfoFetcher::Start(
    const GURL& context_url,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentAppInfoFetchCallback callback) {
  DCHECK_CURRENTLY_ON(ServiceWorkerContext::GetCoreThreadId());

  std::unique_ptr<std::vector<GlobalFrameRoutingId>> frame_routing_ids =
      service_worker_context->GetWindowClientFrameRoutingIds(
          context_url.GetOrigin());

  RunOrPostTaskOnThread(
      FROM_HERE, BrowserThread::UI,
      base::BindOnce(&PaymentAppInfoFetcher::StartOnUI, context_url,
                     std::move(frame_routing_ids), std::move(callback)));
}

void PaymentAppInfoFetcher::StartOnUI(
    const GURL& context_url,
    const std::unique_ptr<std::vector<GlobalFrameRoutingId>>& frame_routing_ids,
    PaymentAppInfoFetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SelfDeleteFetcher* fetcher = new SelfDeleteFetcher(std::move(callback));
  fetcher->Start(context_url, std::move(frame_routing_ids));
}

PaymentAppInfoFetcher::WebContentsHelper::WebContentsHelper(
    WebContents* web_contents)
    : WebContentsObserver(web_contents) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PaymentAppInfoFetcher::WebContentsHelper::~WebContentsHelper() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PaymentAppInfoFetcher::SelfDeleteFetcher::SelfDeleteFetcher(
    PaymentAppInfoFetchCallback callback)
    : fetched_payment_app_info_(std::make_unique<PaymentAppInfo>()),
      callback_(std::move(callback)) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

PaymentAppInfoFetcher::SelfDeleteFetcher::~SelfDeleteFetcher() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

void PaymentAppInfoFetcher::SelfDeleteFetcher::Start(
    const GURL& context_url,
    const std::unique_ptr<std::vector<GlobalFrameRoutingId>>&
        frame_routing_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (frame_routing_ids->empty()) {
    // Cannot print this error to the developer console, because the appropriate
    // developer console has not been found.
    LOG(ERROR)
        << "Unable to find the top level web content for retrieving the web "
           "app manifest of a payment handler for \""
        << context_url << "\".";
    RunCallbackAndDestroy();
    return;
  }

  for (const auto& frame : *frame_routing_ids) {
    // Find out the render frame host registering the payment app. Although a
    // service worker can manage instruments, the first instrument must be set
    // on a page that has a link to a web app manifest, so it can be fetched
    // here.
    RenderFrameHostImpl* render_frame_host =
        RenderFrameHostImpl::FromID(frame.child_id, frame.frame_routing_id);
    if (!render_frame_host ||
        context_url.spec().compare(
            render_frame_host->GetLastCommittedURL().spec()) != 0) {
      continue;
    }

    // Get the main frame since web app manifest is only available in the main
    // frame's document by definition. The main frame's document must come from
    // the same origin.
    RenderFrameHostImpl* top_level_render_frame_host = render_frame_host;
    while (top_level_render_frame_host->GetParent() != nullptr) {
      top_level_render_frame_host = top_level_render_frame_host->GetParent();
    }
    WebContentsImpl* top_level_web_content = static_cast<WebContentsImpl*>(
        WebContents::FromRenderFrameHost(top_level_render_frame_host));
    if (!top_level_web_content) {
      top_level_render_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Unable to find the web page for \"" + context_url.spec() +
              "\" to fetch payment handler manifest (for name and icon).");
      continue;
    }

    if (top_level_web_content->IsHidden()) {
      top_level_render_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Unable to fetch payment handler manifest (for name and icon) for "
          "\"" +
              context_url.spec() + "\" from a hidden top level web page \"" +
              top_level_web_content->GetLastCommittedURL().spec() + "\".");
      continue;
    }

    if (!url::IsSameOriginWith(context_url,
                               top_level_web_content->GetLastCommittedURL())) {
      top_level_render_frame_host->AddMessageToConsole(
          blink::mojom::ConsoleMessageLevel::kError,
          "Unable to fetch payment handler manifest (for name and icon) for "
          "\"" +
              context_url.spec() +
              "\" from a cross-origin top level web page \"" +
              top_level_web_content->GetLastCommittedURL().spec() + "\".");
      continue;
    }

    web_contents_helper_ =
        std::make_unique<WebContentsHelper>(top_level_web_content);

    top_level_web_content->GetManifest(
        base::BindOnce(&PaymentAppInfoFetcher::SelfDeleteFetcher::
                           FetchPaymentAppManifestCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // Cannot print this error to the developer console, because the appropriate
  // developer console has not been found.
  LOG(ERROR)
      << "Unable to find the top level web content for retrieving the web "
         "app manifest of a payment handler for \""
      << context_url << "\".";

  RunCallbackAndDestroy();
}

void PaymentAppInfoFetcher::SelfDeleteFetcher::RunCallbackAndDestroy() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTask(FROM_HERE, {ServiceWorkerContext::GetCoreThreadId()},
                 base::BindOnce(std::move(callback_),
                                std::move(fetched_payment_app_info_)));
  delete this;
}

void PaymentAppInfoFetcher::SelfDeleteFetcher::FetchPaymentAppManifestCallback(
    const GURL& url,
    const blink::Manifest& manifest) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  manifest_url_ = url;
  if (manifest_url_.is_empty()) {
    WarnIfPossible(
        "The page that installed the payment handler does not contain a web "
        "app manifest link: <link rel=\"manifest\" "
        "href=\"some-file-name-here\">. This manifest defines the payment "
        "handler's name and icon. User may not recognize this payment handler "
        "in UI, because it will be labeled only by its origin.");
    RunCallbackAndDestroy();
    return;
  }

  if (manifest.IsEmpty()) {
    WarnIfPossible(
        "Unable to download a valid payment handler web app manifest from \"" +
        manifest_url_.spec() +
        "\". This manifest cannot be empty and must in JSON format. The "
        "manifest defines the payment handler's name and icon. User may not "
        "recognize this payment handler in UI, because it will be labeled only "
        "by its origin.");
    RunCallbackAndDestroy();
    return;
  }

  fetched_payment_app_info_->prefer_related_applications =
      manifest.prefer_related_applications;
  for (const auto& related_application : manifest.related_applications) {
    fetched_payment_app_info_->related_applications.emplace_back(
        StoredRelatedApplication());
    if (related_application.platform) {
      base::UTF16ToUTF8(
          related_application.platform->c_str(),
          related_application.platform->length(),
          &(fetched_payment_app_info_->related_applications.back().platform));
    }
    if (related_application.id) {
      base::UTF16ToUTF8(
          related_application.id->c_str(), related_application.id->length(),
          &(fetched_payment_app_info_->related_applications.back().id));
    }
  }

  if (!manifest.name) {
    WarnIfPossible("The payment handler's web app manifest \"" +
                   manifest_url_.spec() +
                   "\" does not contain a \"name\" field. User may not "
                   "recognize this payment handler in UI, because it will be "
                   "labeled only by its origin.");
  } else if (manifest.name->empty()) {
    WarnIfPossible(
        "The \"name\" field in the payment handler's web app manifest \"" +
        manifest_url_.spec() +
        "\" is empty. User may not recognize this payment handler in UI, "
        "because it will be labeled only by its origin.");
  } else {
    base::UTF16ToUTF8(manifest.name->c_str(), manifest.name->length(),
                      &(fetched_payment_app_info_->name));
  }

  if (manifest.icons.empty()) {
    WarnIfPossible(
        "Unable to download the payment handler's icon, because the web app "
        "manifest \"" +
        manifest_url_.spec() +
        "\" does not contain an \"icons\" field with a valid URL in \"src\" "
        "sub-field. User may not recognize this payment handler in UI.");
    RunCallbackAndDestroy();
    return;
  }

  WebContents* web_contents = web_contents_helper_->web_contents();
  if (!web_contents) {
    LOG(WARNING) << "Unable to download the payment handler's icon because no "
                    "renderer was found, possibly because the page was closed "
                    "or navigated away during installation. User may not "
                    "recognize this payment handler in UI, because it will be "
                    "labeled only by its name and origin.";
    RunCallbackAndDestroy();
    return;
  }
  gfx::NativeView native_view = web_contents->GetNativeView();

  icon_url_ = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest.icons,
      payments::IconSizeCalculator::IdealIconHeight(native_view),
      payments::IconSizeCalculator::MinimumIconHeight(),
      ManifestIconDownloader::kMaxWidthToHeightRatio,
      blink::Manifest::ImageResource::Purpose::ANY);
  if (!icon_url_.is_valid()) {
    WarnIfPossible(
        "No suitable payment handler icon found in the \"icons\" field defined "
        "in the web app manifest \"" +
        manifest_url_.spec() +
        "\". This is most likely due to unsupported MIME types in the "
        "\"icons\" field. User may not recognize this payment handler in UI.");
    RunCallbackAndDestroy();
    return;
  }

  bool can_download = ManifestIconDownloader::Download(
      web_contents, icon_url_,
      payments::IconSizeCalculator::IdealIconHeight(native_view),
      payments::IconSizeCalculator::MinimumIconHeight(),
      base::BindOnce(&PaymentAppInfoFetcher::SelfDeleteFetcher::OnIconFetched,
                     weak_ptr_factory_.GetWeakPtr()),
      false /* square_only */);
  // |can_download| is false only if web contents are  null or the icon URL is
  // not valid. Both of these conditions are manually checked above, so
  // |can_download| should never be false. The manual checks above are necessary
  // to provide more detailed error messages.
  DCHECK(can_download);
}

void PaymentAppInfoFetcher::SelfDeleteFetcher::OnIconFetched(
    const SkBitmap& icon) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (icon.drawsNothing()) {
    WarnIfPossible("Unable to download a valid payment handler icon from \"" +
                   icon_url_.spec() +
                   "\", which is defined in the web app manifest \"" +
                   manifest_url_.spec() +
                   "\". User may not recognize this payment handler in UI.");
    RunCallbackAndDestroy();
    return;
  }

  std::vector<unsigned char> bitmap_data;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(icon, false, &bitmap_data);
  DCHECK(success);
  base::Base64Encode(
      base::StringPiece(reinterpret_cast<const char*>(&bitmap_data[0]),
                        bitmap_data.size()),
      &(fetched_payment_app_info_->icon));
  RunCallbackAndDestroy();
}

void PaymentAppInfoFetcher::SelfDeleteFetcher::WarnIfPossible(
    const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(web_contents_helper_);

  if (web_contents_helper_->web_contents()) {
    web_contents_helper_->web_contents()->GetMainFrame()->AddMessageToConsole(
        blink::mojom::ConsoleMessageLevel::kWarning, message);
  } else {
    LOG(WARNING) << message;
  }
}

}  // namespace content
