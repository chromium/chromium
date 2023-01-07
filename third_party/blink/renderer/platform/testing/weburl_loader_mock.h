// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_WEBURL_LOADER_MOCK_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_WEBURL_LOADER_MOCK_H_

#include <memory>
#include "base/memory/weak_ptr.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_loader.h"

namespace blink {

class WebData;
class WebURLRequestExtraData;
class WebURLLoaderClient;
class WebURLLoaderMockFactoryImpl;
class WebURLLoaderTestDelegate;
class WebURLRequest;
class WebURLResponse;

const uint32_t kRedirectResponseOverheadBytes = 300;

// A simple class for mocking WebURLLoader.
// If the WebURLLoaderMockFactory it is associated with has been configured to
// mock the request it gets, it serves the mocked resource.  Otherwise it just
// forwards it to the default loader.
class WebURLLoaderMock : public WebURLLoader {
 public:
  explicit WebURLLoaderMock(WebURLLoaderMockFactoryImpl* factory);
  WebURLLoaderMock(const WebURLLoaderMock&) = delete;
  WebURLLoaderMock& operator=(const WebURLLoaderMock&) = delete;
  ~WebURLLoaderMock() override;

  // Simulates the asynchronous request being served.
  void ServeAsynchronousRequest(WebURLLoaderTestDelegate* delegate,
                                const WebURLResponse& response,
                                const WebData& data,
                                const absl::optional<WebURLError>& error);

  // Simulates the redirect being served.
  WebURL ServeRedirect(const WebString& method,
                       const WebURLResponse& redirect_response);

  // WebURLLoader methods:
  void LoadSynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool pass_response_pipe_to_client,
      bool no_mime_sniffing,
      base::TimeDelta timeout_interval,
      WebURLLoaderClient* client,
      WebURLResponse&,
      absl::optional<WebURLError>&,
      WebData&,
      int64_t& encoded_data_length,
      int64_t& encoded_body_length,
      blink::WebBlobInfo& downloaded_blob,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper) override;
  void LoadAsynchronously(
      std::unique_ptr<network::ResourceRequest> request,
      scoped_refptr<WebURLRequestExtraData> url_request_extra_data,
      bool no_mime_sniffing,
      std::unique_ptr<blink::ResourceLoadInfoNotifierWrapper>
          resource_load_info_notifier_wrapper,
      WebURLLoaderClient* client) override;
  void Freeze(WebLoaderFreezeMode mode) override;
  void DidChangePriority(WebURLRequest::Priority new_priority,
                         int intra_priority_value) override;
  scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunnerForBodyLoader()
      override;

  bool is_deferred() { return is_deferred_; }
  bool is_cancelled() { return !client_; }

  base::WeakPtr<WebURLLoaderMock> GetWeakPtr();

 private:
  void Cancel();

  WebURLLoaderMockFactoryImpl* factory_ = nullptr;
  WebURLLoaderClient* client_ = nullptr;
  bool is_deferred_ = false;

  base::WeakPtrFactory<WebURLLoaderMock> weak_factory_{this};
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_TESTING_WEBURL_LOADER_MOCK_H_
