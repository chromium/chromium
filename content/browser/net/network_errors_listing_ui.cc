// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/net/network_errors_listing_ui.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted_memory.h"
#include "base/values.h"
#include "content/grit/dev_ui_content_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_util.h"

static const char kNetworkErrorDataFile[] = "network-error-data.json";
static const char kErrorCodeField[]  = "errorCode";
static const char kErrorCodesDataName[] = "errorCodes";
static const char kErrorIdField[]  = "errorId";
static const char kNetworkErrorKey[] = "netError";

namespace content {

namespace {

std::unique_ptr<base::ListValue> GetNetworkErrorData() {
  base::Value error_codes = net::GetNetConstants();
  const base::DictionaryValue* net_error_codes_dict = nullptr;

  for (auto item : error_codes.DictItems()) {
    if (item.first == kNetworkErrorKey) {
      item.second.GetAsDictionary(&net_error_codes_dict);
      break;
    }
  }

  std::unique_ptr<base::ListValue> error_list(new base::ListValue());

  for (base::DictionaryValue::Iterator itr(*net_error_codes_dict);
            !itr.IsAtEnd(); itr.Advance()) {
    const int error_code = itr.value().GetInt();
    // Exclude the aborted and pending codes as these don't return a page.
    if (error_code != net::Error::ERR_IO_PENDING &&
        error_code != net::Error::ERR_ABORTED) {
      std::unique_ptr<base::DictionaryValue> error(new base::DictionaryValue());
      error->SetInteger(kErrorIdField, error_code);
      error->SetString(kErrorCodeField, itr.key());
      error_list->Append(std::move(error));
    }
  }
  return error_list;
}

bool ShouldHandleWebUIRequestCallback(const std::string& path) {
  return path == kNetworkErrorDataFile;
}

void HandleWebUIRequestCallback(BrowserContext* current_context,
                                const std::string& path,
                                WebUIDataSource::GotDataCallback callback) {
  DCHECK(ShouldHandleWebUIRequestCallback(path));

  base::DictionaryValue data;
  data.SetKey(kErrorCodesDataName,
              base::Value::FromUniquePtrValue(GetNetworkErrorData()));
  std::string json_string;
  base::JSONWriter::Write(data, &json_string);
  std::move(callback).Run(base::RefCountedString::TakeString(&json_string));
}

} // namespace

NetworkErrorsListingUI::NetworkErrorsListingUI(WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://network-errors source.
  WebUIDataSource* html_source = WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      kChromeUINetworkErrorsListingHost);

  // Add required resources.
  html_source->UseStringsJs();
  html_source->AddResourcePath("network_errors_listing.css",
                               IDR_NETWORK_ERROR_LISTING_CSS);
  html_source->AddResourcePath("network_errors_listing.js",
                               IDR_NETWORK_ERROR_LISTING_JS);
  html_source->SetDefaultResource(IDR_NETWORK_ERROR_LISTING_HTML);
  html_source->SetRequestFilter(
      base::BindRepeating(&ShouldHandleWebUIRequestCallback),
      base::BindRepeating(&HandleWebUIRequestCallback,
                          web_ui->GetWebContents()->GetBrowserContext()));
}

}  // namespace content
