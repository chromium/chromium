// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/test/host_list_fetcher.h"

#include <utility>

#include "base/bind.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/http/http_status_code.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_fetcher.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {
namespace test {

HostListFetcher::HostListFetcher() = default;

HostListFetcher::~HostListFetcher() = default;

void HostListFetcher::RetrieveHostlist(const std::string& access_token,
                                       const std::string& target_url,
                                       const HostlistCallback& callback) {
  VLOG(2) << "HostListFetcher::RetrieveHostlist() called";

  DCHECK(!access_token.empty());
  DCHECK(!callback.is_null());
  DCHECK(hostlist_callback_.is_null());

  hostlist_callback_ = callback;

  request_context_getter_ = new remoting::URLRequestContextGetter(
      /*network_runner=*/base::ThreadTaskRunnerHandle::Get());

  request_ = net::URLFetcher::Create(GURL(target_url), net::URLFetcher::GET,
                                     this, TRAFFIC_ANNOTATION_FOR_TESTS);
  request_->SetRequestContext(request_context_getter_.get());
  request_->AddExtraRequestHeader("Authorization: OAuth " + access_token);
  request_->Start();
}

bool HostListFetcher::ProcessResponse(
    std::vector<HostInfo>* hostlist) {
  int response_code = request_->GetResponseCode();
  if (response_code != net::HTTP_OK) {
    LOG(ERROR) << "Hostlist request failed with error code: " << response_code;
    return false;
  }

  std::string response_string;
  if (!request_->GetResponseAsString(&response_string)) {
    LOG(ERROR) << "Failed to retrieve Hostlist response data";
    return false;
  }

  base::Optional<base::Value> response =
      base::JSONReader::Read(response_string);
  if (!response) {
    LOG(ERROR) << "Failed to parse response string to JSON";
    return false;
  }

  if (!response->is_dict()) {
    LOG(ERROR) << "Parsed JSON is not a dictionary object";
    return false;
  }

  const base::Value* data = response->FindDictKey("data");
  if (!data) {
    LOG(ERROR) << "Hostlist response data is empty";
    return false;
  }

  const base::Value* hosts = data->FindListKey("items");
  if (!hosts) {
    LOG(ERROR) << "Failed to find hosts in Hostlist response data";
    return false;
  }

  // Any host_info with malformed data will not be added to the hostlist.
  for (const base::Value& host_info : hosts->GetList()) {
    HostInfo host;
    if (host_info.is_dict() && host.ParseHostInfo(host_info)) {
      hostlist->push_back(host);
    }
  }
  return true;
}

void HostListFetcher::OnURLFetchComplete(
    const net::URLFetcher* source) {
  DCHECK(source);
  VLOG(2) << "URL Fetch Completed for: " << source->GetOriginalURL();

  std::vector<HostInfo> hostlist;

  if (!ProcessResponse(&hostlist)) {
    hostlist.clear();
  }
  std::move(hostlist_callback_).Run(hostlist);
}

}  // namespace test
}  // namespace remoting
