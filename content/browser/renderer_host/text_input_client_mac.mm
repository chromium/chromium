// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/text_input_client_mac.h"

#include <string_view>
#include <utility>
#include <variant>

#include "base/auto_reset.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "ui/base/mojom/attributed_string.mojom.h"

namespace content {

namespace {

class DefaultAsyncRequestDelegate final
    : public TextInputClientMac::AsyncRequestDelegate {
 public:
  DefaultAsyncRequestDelegate() = default;
  ~DefaultAsyncRequestDelegate() final = default;

  DefaultAsyncRequestDelegate(const DefaultAsyncRequestDelegate&) = delete;
  DefaultAsyncRequestDelegate& operator=(const DefaultAsyncRequestDelegate&) =
      delete;

  void GetCharacterIndexAtPoint(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Point& point) final {
    RenderFrameHostImpl::From(rfh)
        ->GetAssociatedLocalFrame()
        ->GetCharacterIndexAtPoint(request_token.value(), point);
  }

  void GetFirstRectForRange(
      RenderFrameHost* rfh,
      const TextInputClientMac::RequestToken& request_token,
      const gfx::Range& range) final {
    RenderFrameHostImpl::From(rfh)
        ->GetAssociatedLocalFrame()
        ->GetFirstRectForRange(request_token.value(), range);
  }
};

RenderFrameHostImpl* GetFocusedRenderFrameHostImpl(RenderWidgetHost* widget) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(widget);
  FrameTree* tree = rwhi->frame_tree();
  FrameTreeNode* focused_node = tree->GetFocusedFrame();
  return focused_node ? focused_node->current_frame_host() : nullptr;
}

base::WeakPtr<RenderFrameHostImpl> GetWeakFocusedRenderFrameHostImpl(
    RenderWidgetHost* widget) {
  if (RenderFrameHostImpl* rhfi = GetFocusedRenderFrameHostImpl(widget)) {
    return rhfi->GetWeakPtr();
  }
  return nullptr;
}

}  // namespace

TextInputClientMac::TextInputClientMac()
    : condition_(&lock_),
      async_request_delegate_(std::make_unique<DefaultAsyncRequestDelegate>()) {
}

TextInputClientMac::~TextInputClientMac() = default;

// static
TextInputClientMac* TextInputClientMac::GetInstance() {
  static base::NoDestructor<TextInputClientMac> client;
  return client.get();
}

void TextInputClientMac::GetStringAtPoint(RenderWidgetHost* rwh,
                                          const gfx::Point& point,
                                          GetStringCallback callback) {
  RenderWidgetHostImpl* rwhi = RenderWidgetHostImpl::From(rwh);
  if (rwhi && rwhi->GetAssociatedFrameWidget()) {
    rwhi->GetAssociatedFrameWidget()->GetStringAtPoint(point,
                                                       std::move(callback));
  } else {
    std::move(callback).Run(nullptr, gfx::Point());
  }
}

void TextInputClientMac::GetStringFromRange(RenderWidgetHost* rwh,
                                            const gfx::Range& range,
                                            GetStringCallback callback) {
  RenderFrameHostImpl* rfhi = GetFocusedRenderFrameHostImpl(rwh);
  // If it doesn't have a focused frame, it calls |callback| with an empty
  // string and point.
  if (!rfhi) {
    return std::move(callback).Run(nullptr, gfx::Point());
  }

  rfhi->GetAssociatedLocalFrame()->GetStringForRange(range,
                                                     std::move(callback));
}

uint32_t TextInputClientMac::GetCharacterIndexAtPoint(RenderWidgetHost* rwh,
                                                      const gfx::Point& point) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto result = SyncRequest(GetWeakFocusedRenderFrameHostImpl(rwh), point,
                            "CharacterIndex");
  // Return index 0 on failure, or a sentinel on timeout.
  return std::visit(absl::Overload{
                        [](NoResultYetTag) { return UINT32_MAX; },
                        [](FailedRequestTag) { return 0u; },
                        [](uint32_t index) { return index; },
                        [](const gfx::Rect&) -> uint32_t { NOTREACHED(); },
                    },
                    result);
}

gfx::Rect TextInputClientMac::GetFirstRectForRange(RenderWidgetHost* rwh,
                                                   const gfx::Range& range) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::WeakPtr<RenderFrameHostImpl> rfhi =
      GetWeakFocusedRenderFrameHostImpl(rwh);
  auto result = SyncRequest(rfhi, range, "FirstRect");
  return std::visit(
      absl::Overload{
          [](NoResultYetTag) { return gfx::Rect(); },
          [](FailedRequestTag) { return gfx::Rect(); },
          [](uint32_t index) -> gfx::Rect { NOTREACHED(); },
          [&rfhi](const gfx::Rect& rect) {
            // `rect` is in (child) frame coordinate and needs to be transformed
            // to the root frame coordinate. If `rfhi` has been deleted, it's
            // too late to do the transform but the result is moot anyway.
            return rfhi ? gfx::Rect(
                              rfhi->GetView()->TransformPointToRootCoordSpace(
                                  rect.origin()),
                              rect.size())
                        : gfx::Rect();
          },
      },
      result);
}

TextInputClientMac::ResultValue TextInputClientMac::SyncRequest(
    base::WeakPtr<RenderFrameHostImpl> rfhi,
    const RequestParams& params,
    std::string_view metrics_suffix) {
  if (!rfhi) {
    // No focused frame.
    return FailedRequestTag{};
  }

  base::UmaHistogramBoolean(
      base::StrCat({"TextInputClient.InSyncRequest.", metrics_suffix}),
      in_sync_request_);
  if (in_sync_request_) {
    return FailedRequestTag{};
  }
  base::AutoReset in_sync_request(&in_sync_request_, true);

  const base::TimeDelta wait_timeout =
      features::kTextInputClientIPCTimeout.Get();

  ResultValue result;
  const base::LiveTicks start = base::LiveTicks::Now();
  {
    base::AutoLock lock(lock_);
    base::UmaHistogramLongTimes("TextInputClient.LockWait2",
                                base::LiveTicks::Now() - start);

    CHECK(!current_request_.has_value());
    CHECK(std::holds_alternative<NoResultYetTag>(current_result_));
    base::AutoReset current_request(&current_request_, RequestToken{});

    async_request_delegate_->SendRequest(rfhi.get(), current_request_.value(),
                                         params);

    base::TimeDelta remaining_timeout = wait_timeout;
    while (std::holds_alternative<NoResultYetTag>(current_result_) &&
           remaining_timeout.is_positive()) {
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      condition_.TimedWait(remaining_timeout);
      remaining_timeout = start + wait_timeout - base::LiveTicks::Now();
    }

    // Only this function should be setting FailedRequestTag.
    CHECK(!std::holds_alternative<FailedRequestTag>(current_result_));
    base::UmaHistogramBoolean(
        base::StrCat({"TextInputClient.", metrics_suffix, ".TimedOut"}),
        std::holds_alternative<NoResultYetTag>(current_result_));

    // Take the result before releasing the lock.
    std::swap(result, current_result_);
  }

  base::UmaHistogramLongTimes(
      base::StrCat({"TextInputClient.", metrics_suffix, "2"}),
      base::LiveTicks::Now() - start);

  return result;
}

void TextInputClientMac::SetCharacterIndexAndSignal(
    const RequestToken& request_token,
    uint32_t index) {
  SetResultAndSignal(request_token, ResultValue(index));
}

void TextInputClientMac::SetFirstRectAndSignal(
    const RequestToken& request_token,
    const gfx::Rect& first_rect) {
  SetResultAndSignal(request_token, ResultValue(first_rect));
}

void TextInputClientMac::SetResultAndSignal(const RequestToken& request_token,
                                            ResultValue result) {
  {
    base::AutoLock lock(lock_);
    if (!current_request_.has_value() ||
        current_request_.value() != request_token) {
      // Stale request.
      return;
    }
    CHECK(std::holds_alternative<NoResultYetTag>(current_result_));
    current_result_ = result;
  }
  condition_.Signal();
}

void TextInputClientMac::SetAsyncRequestDelegateForTesting(
    std::unique_ptr<AsyncRequestDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  async_request_delegate_ =
      delegate ? std::move(delegate)
               : std::make_unique<DefaultAsyncRequestDelegate>();
}

void TextInputClientMac::SetCharacterIndexWhileLockedForTesting(
    const RequestToken& request_token,
    uint32_t index) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a GetCharacterIndexAtPoint() response that arrives before the
  // `condition_.Wait()` call, so it must run on the same thread (not just
  // sequence) that calls Wait() to preserve ordering.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoUnlock unlock(lock_);
  SetCharacterIndexAndSignal(request_token, index);
}

void TextInputClientMac::SetFirstRectWhileLockedForTesting(
    const RequestToken& request_token,
    const gfx::Rect& first_rect) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a GetFirstRectForRange() response that arrives before the
  // `condition_.Wait()` call, so it must run on the same thread (not just
  // sequence) that calls Wait() to preserve ordering.
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::AutoUnlock unlock(lock_);
  SetFirstRectAndSignal(request_token, first_rect);
}

void TextInputClientMac::AsyncRequestDelegate::SendRequest(
    RenderFrameHost* rfh,
    const RequestToken& request_token,
    const RequestParams& params) {
  std::visit(absl::Overload{
                 [&](const gfx::Point& point) {
                   GetCharacterIndexAtPoint(rfh, request_token, point);
                 },
                 [&](const gfx::Range& range) {
                   GetFirstRectForRange(rfh, request_token, range);
                 },
             },
             params);
}

}  // namespace content
