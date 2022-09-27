// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_NOOP_WEB_URL_LOADER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_NOOP_WEB_URL_LOADER_H_

#include "third_party/blink/public/platform/web_url_loader.h"

namespace blink {

class NoopWebURLLoader final : public WebURLLoader {
 public:
  explicit NoopWebURLLoader(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}
  ~NoopWebURLLoader() override = default;
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient*,
      WebURLResponse&,
      absl::optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient*) override;

  void Freeze(WebLoaderFreezeMode) override {}
  void DidChangePriority(WebURLRequest::Priority, int) override {
    NOTREACHED();
  }
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override {
    return task_runner_;
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_NOOP_WEB_URL_LOADER_H_
