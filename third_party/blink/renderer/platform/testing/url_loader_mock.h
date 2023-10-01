// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_URL_LOADER_MOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_URL_LOADER_MOCK_H_

#include <memory>
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader.h"

namespace blink {

class WebURLRequestExtraData;
class URLLoaderClient;
class URLLoaderMockFactoryImpl;
class URLLoaderTestDelegate;
class WebURLRequest;
class WebURLResponse;

const uint32_t kRedirectResponseOverheadBytes = 300;

// A simple class for mocking URLLoader.
// If the URLLoaderMockFactory it is associated with has been configured to
// mock the request it gets, it serves the mocked resource.  Otherwise it just
// forwards it to the default loader.
class URLLoaderMock : public URLLoader {
 public:
  explicit URLLoaderMock(URLLoaderMockFactoryImpl* factory);
  URLLoaderMock(const URLLoaderMock&) = delete;
  URLLoaderMock& operator=(const URLLoaderMock&) = delete;
  ~URLLoaderMock() override;

  // Simulates the asynchronous request being served.
  void ServeAsynchronousRequest(URLLoaderTestDelegate* delegate,
                                const WebURLResponse& response,
                                const scoped_refptr<SharedBuffer>& data,
                                const absl::optional<WebURLError>& error);

  // Simulates the redirect being served.
  WebURL ServeRedirect(const WebString& method,
                       const WebURLResponse& redirect_response);

  // URLLoader methods:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      URLLoaderClient* client,
      WebURLResponse&,
      absl::optional<WebURLError>&,
      scoped_refptr<SharedBuffer>&,
      int64_t& encoded_data_length,
      uint64_t& encoded_body_length,
      scoped_refptr<BlobDataHandle>& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      URLLoaderClient* client) override;
  void Freeze(LoaderFreezeMode mode) override;
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override;

  bool is_deferred() { return is_deferred_; }
  bool is_cancelled() { return !client_; }

  base::WeakPtr<URLLoaderMock> GetWeakPtr();

 private:
  void Cancel();

  raw_ptr<URLLoaderMockFactoryImpl, ExperimentalRenderer> factory_ = nullptr;
  raw_ptr<URLLoaderClient, ExperimentalRenderer> client_ = nullptr;
  bool is_deferred_ = false;

  base::WeakPtrFactory<URLLoaderMock> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_URL_LOADER_MOCK_H_
