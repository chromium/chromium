// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/webstore_install_helper.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/memory/ref_counted.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/image_fetcher/core/image_decoder.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_impl.h"
#include "components/image_fetcher/core/request_metadata.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/buildflags/buildflags.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "ui/gfx/image/image.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

using content::BrowserThread;

namespace extensions {

namespace {

constexpr char kImageDecodeError[] = "Image decode failed";

class WebstoreInstallHelper : public base::RefCounted<WebstoreInstallHelper> {
 public:
  WebstoreInstallHelper(const std::string& id,
                        const std::string& manifest,
                        const GURL& icon_url,
                        WebstoreParseCallback callback)
      : id_(id),
        manifest_(manifest),
        icon_url_(icon_url),
        callback_(std::move(callback)) {}

  void Start(scoped_refptr<network::SharedURLLoaderFactory> loader_factory) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

    auto parse_result = base::JSONReader::ReadAndReturnValueWithError(
        manifest_, base::JSON_PARSE_RFC);

    base::DictValue manifest_dict;
    std::string error_message;
    if (parse_result.has_value()) {
      if (parse_result->is_dict()) {
        manifest_dict = std::move(*parse_result).TakeDict();
      } else {
        error_message = "Manifest is not a dictionary";
      }
    } else {
      error_message = parse_result.error().message;
    }

    if (!error_message.empty()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(
              std::move(callback_),
              base::unexpected(WebstoreParseError{
                  .error_code = WebstoreInstallHelperResultCode::kManifestError,
                  .error_message = std::move(error_message),
              })));
      return;
    }

    if (icon_url_.is_empty()) {
      content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback_),
                                    WebstoreParsedData{
                                        .manifest = std::move(manifest_dict),
                                    }));
      return;
    }

    net::NetworkTrafficAnnotationTag traffic_annotation =
        net::DefineNetworkTrafficAnnotation("webstore_install_helper", R"(
          semantics {
            sender: "Webstore Install Helper"
            description:
              "Fetches the bitmap corresponding to an extension icon."
            trigger:
              "This can happen in a few different circumstances: "
              "1-User initiated an install from the Chrome Web Store."
              "2-User initiated an inline installation from another website."
              "3-Loading of kiosk app data on Chrome OS (provided that the "
              "kiosk app is a Web Store app)."
            data:
              "The url of the icon for the extension, which includes the "
              "extension id."
            destination: GOOGLE_OWNED_SERVICE
            internal {
              contacts {
                owners: "//extensions/OWNERS"
              }
            }
            user_data {
              type: NONE
            }
            last_reviewed: "2026-04-03"
          }
          policy {
            cookies_allowed: NO
            setting:
              "There's no direct Chromium's setting to disable this, but you "
              "could uninstall all extensions and not install (or begin the "
              "installation flow for) any more."
            policy_exception_justification:
              "Not implemented, considered not useful."
          })");
    icon_fetcher_ = std::make_unique<image_fetcher::ImageFetcherImpl>(
        ExtensionsBrowserClient::Get()->CreateImageDecoder(), loader_factory);
    image_fetcher::ImageFetcherParams params(traffic_annotation,
                                             "WebstoreInstallHelper");
    icon_fetcher_->FetchImage(
        icon_url_,
        base::BindOnce(&WebstoreInstallHelper::OnFetchComplete, this,
                       std::move(manifest_dict)),
        std::move(params));
  }

 private:
  friend class base::RefCounted<WebstoreInstallHelper>;
  ~WebstoreInstallHelper() = default;

  void OnFetchComplete(base::DictValue manifest_dict,
                       const gfx::Image& fetched_image,
                       const image_fetcher::RequestMetadata& metadata) {
    CHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    CHECK(icon_fetcher_.get());

    // `ImageFetcher` is on the stack while invoking the completion callback, so
    // defer its deletion since ImageFetcher is not robust against reentrant
    // destruction.
    content::GetUIThreadTaskRunner({})->DeleteSoon(FROM_HERE,
                                                   std::move(icon_fetcher_));

    if (metadata.http_response_code ==
            image_fetcher::RequestMetadata::ResponseCode::
                RESPONSE_CODE_INVALID ||
        fetched_image.IsEmpty()) {
      std::move(callback_).Run(base::unexpected(WebstoreParseError{
          .error_code = WebstoreInstallHelperResultCode::kIconError,
          .error_message = kImageDecodeError,
      }));
      return;
    }

    std::move(callback_).Run(WebstoreParsedData{
        .icon = fetched_image.AsBitmap(),
        .manifest = std::move(manifest_dict),
    });
  }

  const std::string id_;
  const std::string manifest_;
  const GURL icon_url_;
  WebstoreParseCallback callback_;
  std::unique_ptr<image_fetcher::ImageFetcher> icon_fetcher_;
};

}  // namespace

void ParseWebstoreData(
    scoped_refptr<network::SharedURLLoaderFactory> loader_factory,
    const std::string& id,
    const std::string& manifest,
    const GURL& icon_url,
    WebstoreParseCallback callback) {
  auto helper = base::MakeRefCounted<WebstoreInstallHelper>(
      id, manifest, icon_url, std::move(callback));
  helper->Start(loader_factory);
}

}  // namespace extensions
