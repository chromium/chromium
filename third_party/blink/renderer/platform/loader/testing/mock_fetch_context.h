// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_MOCK_FETCH_CONTEXT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_TESTING_MOCK_FETCH_CONTEXT_H_

#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_factory.h"
#include "third_party/blink/renderer/platform/exported/wrapped_resource_request.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/fetch_parameters.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_timing_info.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/test/fake_task_runner.h"

#include <memory>

namespace blink {

class KURL;
class ResourceRequest;
struct ResourceLoaderOptions;

// Mocked FetchContext for testing.
class MockFetchContext : public FetchContext {
 public:
  enum LoadPolicy {
    kShouldLoadNewResource,
    kShouldNotLoadNewResource,
  };
  static MockFetchContext* Create(
      LoadPolicy load_policy,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner = nullptr,
      std::unique_ptr<WebURLLoaderFactory> url_loader_factory = nullptr) {
    return new MockFetchContext(load_policy, std::move(loading_task_runner),
                                std::move(url_loader_factory));
  }

  ~MockFetchContext() override = default;

  void SetLoadComplete(bool complete) { complete_ = complete; }
  long long GetTransferSize() const { return transfer_size_; }

  const SecurityOrigin* GetSecurityOrigin() const override {
    return security_origin_.get();
  }

  void SetSecurityOrigin(scoped_refptr<const SecurityOrigin> security_origin) {
    security_origin_ = security_origin;
  }

  // The last ResourceRequest passed to DispatchWillSendRequest.
  base::Optional<ResourceRequest> RequestFromWillSendRequest() const {
    return will_send_request_;
  }

  // FetchContext:
  void DispatchWillSendRequest(
      unsigned long identifier,
      ResourceRequest& request,
      const ResourceResponse& redirect_response,
      ResourceType,
      const FetchInitiatorInfo& = FetchInitiatorInfo()) override {
    will_send_request_ = request;
  }
  bool AllowImage(bool images_enabled, const KURL&) const override {
    return true;
  }
  base::Optional<ResourceRequestBlockedReason> CanRequest(
      ResourceType,
      const ResourceRequest&,
      const KURL&,
      const ResourceLoaderOptions&,
      SecurityViolationReportingPolicy,
      ResourceRequest::RedirectStatus redirect_status) const override {
    return base::nullopt;
  }
  base::Optional<ResourceRequestBlockedReason> CheckCSPForRequest(
      mojom::RequestContextType,
      const KURL& url,
      const ResourceLoaderOptions& options,
      SecurityViolationReportingPolicy reporting_policy,
      ResourceRequest::RedirectStatus redirect_status) const override {
    return base::nullopt;
  }
  bool ShouldLoadNewResource(ResourceType) const override {
    return load_policy_ == kShouldLoadNewResource;
  }
  bool IsLoadComplete() const override { return complete_; }
  void AddResourceTiming(
      const ResourceTimingInfo& resource_timing_info) override {
    transfer_size_ = resource_timing_info.TransferSize();
  }

  std::unique_ptr<WebURLLoader> CreateURLLoader(
      const ResourceRequest& request,
      const ResourceLoaderOptions&) override {
    if (!url_loader_factory_) {
      url_loader_factory_ =
          Platform::Current()->CreateDefaultURLLoaderFactory();
    }
    WrappedResourceRequest wrapped(request);
    return url_loader_factory_->CreateURLLoader(
        wrapped, CreateResourceLoadingTaskRunnerHandle());
  }

  ResourceLoadScheduler::ThrottlingPolicy InitialLoadThrottlingPolicy()
      const override {
    return ResourceLoadScheduler::ThrottlingPolicy::kTight;
  }

  FrameScheduler* GetFrameScheduler() const override {
    return frame_scheduler_.get();
  }

  std::unique_ptr<blink::scheduler::WebResourceLoadingTaskRunnerHandle>
  CreateResourceLoadingTaskRunnerHandle() override {
    return scheduler::WebResourceLoadingTaskRunnerHandle::CreateUnprioritized(
        GetLoadingTaskRunner());
  }

 private:
  class MockFrameScheduler final : public scheduler::FakeFrameScheduler {
   public:
    explicit MockFrameScheduler(
        scoped_refptr<base::SingleThreadTaskRunner> runner)
        : runner_(std::move(runner)) {}
    scoped_refptr<base::SingleThreadTaskRunner> GetTaskRunner(
        TaskType) override {
      return runner_;
    }

   private:
    scoped_refptr<base::SingleThreadTaskRunner> runner_;
  };

  MockFetchContext(
      LoadPolicy load_policy,
      scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner,
      std::unique_ptr<WebURLLoaderFactory> url_loader_factory)
      : FetchContext(loading_task_runner
                         ? std::move(loading_task_runner)
                         : base::MakeRefCounted<scheduler::FakeTaskRunner>()),
        load_policy_(load_policy),
        security_origin_(SecurityOrigin::CreateUniqueOpaque()),
        frame_scheduler_(new MockFrameScheduler(GetLoadingTaskRunner())),
        url_loader_factory_(std::move(url_loader_factory)),
        complete_(false),
        transfer_size_(-1) {}

  enum LoadPolicy load_policy_;
  scoped_refptr<const SecurityOrigin> security_origin_;
  std::unique_ptr<FrameScheduler> frame_scheduler_;
  std::unique_ptr<WebURLLoaderFactory> url_loader_factory_;
  bool complete_;
  long long transfer_size_;
  base::Optional<ResourceRequest> will_send_request_;
};

}  // namespace blink

#endif
