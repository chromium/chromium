// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/payments/payment_app_info_fetcher.h"

#include <utility>

#include "base/base64.h"
#include "base/bind_helpers.h"
#include "base/task/post_task.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/manifest_icon_downloader.h"
#include "content/public/common/console_message_level.h"
#include "third_party/blink/public/common/manifest/manifest_icon_selector.h"
#include "ui/gfx/codec/png_codec.h"
#include "url/origin.h"

namespace content {

PaymentAppInfoFetcher::PaymentAppInfo::PaymentAppInfo() {}

PaymentAppInfoFetcher::PaymentAppInfo::~PaymentAppInfo() {}

void PaymentAppInfoFetcher::Start(
    const GURL& context_url,
    scoped_refptr<ServiceWorkerContextWrapper> service_worker_context,
    PaymentAppInfoFetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  std::unique_ptr<std::vector<GlobalFrameRoutingId>> provider_hosts =
      service_worker_context->GetProviderHostIds(context_url.GetOrigin());

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&PaymentAppInfoFetcher::StartOnUI, context_url,
                     std::move(provider_hosts), std::move(callback)));
}

void PaymentAppInfoFetcher::StartOnUI(
    const GURL& context_url,
    const std::unique_ptr<std::vector<GlobalFrameRoutingId>>& provider_hosts,
    PaymentAppInfoFetchCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  SelfDeleteFetcher* fetcher = new SelfDeleteFetcher(std::move(callback));
  fetcher->Start(context_url, std::move(provider_hosts));
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
    const std::unique_ptr<std::vector<GlobalFrameRoutingId>>& provider_hosts) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (provider_hosts->size() == 0U) {
    RunCallbackAndDestroy();
    return;
  }

  for (const auto& frame : *provider_hosts) {
    // Find out the render frame host registering the payment app.
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
    if (!top_level_web_content || top_level_web_content->IsHidden() ||
        !url::IsSameOriginWith(context_url,
                               top_level_web_content->GetLastCommittedURL())) {
      continue;
    }

    web_contents_helper_ =
        std::make_unique<WebContentsHelper>(top_level_web_content);

    top_level_web_content->GetManifest(
        base::BindOnce(&PaymentAppInfoFetcher::SelfDeleteFetcher::
                           FetchPaymentAppManifestCallback,
                       base::Unretained(this)));
    return;
  }

  RunCallbackAndDestroy();
}

void PaymentAppInfoFetcher::SelfDeleteFetcher::RunCallbackAndDestroy() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
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
    if (!related_application.platform.is_null()) {
      base::UTF16ToUTF8(
          related_application.platform.string().c_str(),
          related_application.platform.string().length(),
          &(fetched_payment_app_info_->related_applications.back().platform));
    }
    if (!related_application.id.is_null()) {
      base::UTF16ToUTF8(
          related_application.id.string().c_str(),
          related_application.id.string().length(),
          &(fetched_payment_app_info_->related_applications.back().id));
    }
  }

  if (manifest.name.is_null()) {
    WarnIfPossible("The payment handler's web app manifest \"" +
                   manifest_url_.spec() +
                   "\" does not contain a \"name\" field. User may not "
                   "recognize this payment handler in UI, because it will be "
                   "labeled only by its origin.");
  } else if (manifest.name.string().empty()) {
    WarnIfPossible(
        "The \"name\" field in the payment handler's web app manifest \"" +
        manifest_url_.spec() +
        "\" is empty. User may not recognize this payment handler in UI, "
        "because it will be labeled only by its origin.");
  } else {
    base::UTF16ToUTF8(manifest.name.string().c_str(),
                      manifest.name.string().length(),
                      &(fetched_payment_app_info_->name));
  }

  // TODO(gogerald): Choose appropriate icon size dynamically on different
  // platforms.
  // Here we choose a large ideal icon size to be big enough for all platforms.
  // Note that we only scale down for this icon size but not scale up.
  const int kPaymentAppIdealIconSize = 0xFFFF;
  const int kPaymentAppMinimumIconSize = 0;

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

  icon_url_ = blink::ManifestIconSelector::FindBestMatchingIcon(
      manifest.icons, kPaymentAppIdealIconSize, kPaymentAppMinimumIconSize,
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

  if (!web_contents_helper_->web_contents()) {
    LOG(WARNING) << "Unable to download the payment handler's icon because no "
                    "renderer was found, possibly because the page was closed "
                    "or navigated away during installation. User may not "
                    "recognize this payment handler in UI, because it will be "
                    "labeled only by its name and origin.";
    RunCallbackAndDestroy();
    return;
  }

  bool can_download = ManifestIconDownloader::Download(
      web_contents_helper_->web_contents(), icon_url_, kPaymentAppIdealIconSize,
      kPaymentAppMinimumIconSize,
      base::Bind(&PaymentAppInfoFetcher::SelfDeleteFetcher::OnIconFetched,
                 base::Unretained(this)));
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
        CONSOLE_MESSAGE_LEVEL_WARNING, message);
  } else {
    LOG(WARNING) << message;
  }
}

}  // namespace content
