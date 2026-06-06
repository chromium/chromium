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
#include "base/functional/callback_helpers.h"
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
#include "content/public/browser/browser_thread.h"
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

uint32_t TransformCharacterIndexResult(TextInputClientMac::ResultValue result) {
  // Return index 0 on failure, or a sentinel on timeout.
  return std::visit(
      absl::Overload{
          [](TextInputClientMac::NoResultYetTag) { return UINT32_MAX; },
          [](TextInputClientMac::FailedRequestTag) { return 0u; },
          [](uint32_t index) { return index; },
          [](const gfx::Rect&) -> uint32_t { NOTREACHED(); },
      },
      result);
}

gfx::Rect TransformFirstRectResult(base::WeakPtr<RenderFrameHostImpl> rfhi,
                                   TextInputClientMac::ResultValue result) {
  return std::visit(
      absl::Overload{
          [](TextInputClientMac::NoResultYetTag) { return gfx::Rect(); },
          [](TextInputClientMac::FailedRequestTag) { return gfx::Rect(); },
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

void RecordLockWaitTime(base::LiveTicks start_time) {
  base::UmaHistogramLongTimes("TextInputClient.LockWait2",
                              base::LiveTicks::Now() - start_time);
}

TextInputClientMac::ResultValue RecordResult(
    std::string_view metrics_suffix,
    base::LiveTicks start_time,
    base::LiveTicks end_time,
    TextInputClientMac::ResultValue result) {
  // Only SyncRequest() and AsyncRequest() should be setting FailedRequestTag.
  CHECK(!std::holds_alternative<TextInputClientMac::FailedRequestTag>(result));
  base::UmaHistogramBoolean(
      base::StrCat({"TextInputClient.", metrics_suffix, ".TimedOut"}),
      std::holds_alternative<TextInputClientMac::NoResultYetTag>(result));
  base::UmaHistogramLongTimes(
      base::StrCat({"TextInputClient.", metrics_suffix, "2"}),
      end_time - start_time);
  return result;
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

uint32_t TextInputClientMac::SyncGetCharacterIndexAtPoint(
    RenderWidgetHost* rwh,
    const gfx::Point& point) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return TransformCharacterIndexResult(SyncRequest(
      GetWeakFocusedRenderFrameHostImpl(rwh), point, "CharacterIndex"));
}

void TextInputClientMac::AsyncGetCharacterIndexAtPoint(
    RenderWidgetHost* rwh,
    const gfx::Point& point,
    base::OnceCallback<void(uint32_t)> result_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  AsyncRequest(GetWeakFocusedRenderFrameHostImpl(rwh), point, "CharacterIndex",
               base::BindOnce(&TransformCharacterIndexResult)
                   .Then(std::move(result_callback)));
}

gfx::Rect TextInputClientMac::SyncGetFirstRectForRange(
    RenderWidgetHost* rwh,
    const gfx::Range& range) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::WeakPtr<RenderFrameHostImpl> rfhi =
      GetWeakFocusedRenderFrameHostImpl(rwh);
  return TransformFirstRectResult(rfhi, SyncRequest(rfhi, range, "FirstRect"));
}

void TextInputClientMac::AsyncGetFirstRectForRange(
    RenderWidgetHost* rwh,
    const gfx::Range& range,
    base::OnceCallback<void(gfx::Rect)> result_callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  base::WeakPtr<RenderFrameHostImpl> rfhi =
      GetWeakFocusedRenderFrameHostImpl(rwh);
  AsyncRequest(rfhi, range, "FirstRect",
               base::BindOnce(&TransformFirstRectResult, rfhi)
                   .Then(std::move(result_callback)));
}

TextInputClientMac::ResultValue TextInputClientMac::SyncRequest(
    base::WeakPtr<RenderFrameHostImpl> rfhi,
    const RequestParams& params,
    std::string_view metrics_suffix) {
  if (!rfhi) {
    // No focused frame.
    return FailedRequestTag{};
  }

  CHECK(!in_sync_request_);
  base::AutoReset in_sync_request(&in_sync_request_, true);

  ResultValue result;
  const base::LiveTicks start = base::LiveTicks::Now();
  {
    base::AutoLock lock(lock_);
    RecordLockWaitTime(start);

    CHECK(!current_sync_request_.has_value());
    CHECK(std::holds_alternative<NoResultYetTag>(current_sync_result_));
    base::AutoReset current_request(&current_sync_request_, RequestToken{});

    async_request_delegate_->SendRequest(rfhi.get(),
                                         current_sync_request_.value(), params);

    base::TimeDelta remaining_timeout = wait_timeout_;
    while (std::holds_alternative<NoResultYetTag>(current_sync_result_) &&
           remaining_timeout.is_positive()) {
      base::ScopedAllowBaseSyncPrimitivesOutsideBlockingScope allow_wait;
      condition_.TimedWait(remaining_timeout);
      remaining_timeout = start + wait_timeout_ - base::LiveTicks::Now();
    }

    // Take the result before releasing the lock.
    std::swap(result, current_sync_result_);
  }

  return RecordResult(metrics_suffix, start, base::LiveTicks::Now(), result);
}

void TextInputClientMac::AsyncRequest(
    base::WeakPtr<RenderFrameHostImpl> rfhi,
    const RequestParams& params,
    std::string_view metrics_suffix,
    base::OnceCallback<void(ResultValue)> result_callback) {
  if (!rfhi) {
    // No focused frame.
    std::move(result_callback).Run(FailedRequestTag{});
    return;
  }

  CHECK(!in_sync_request_);

  const base::LiveTicks start = base::LiveTicks::Now();
  base::AutoLock lock(lock_);
  RecordLockWaitTime(start);

  // Call `result_callback` either when a result is received or on timeout.
  auto [success_callback, timeout_callback] = base::SplitOnceCallback(
      base::BindOnce(&RecordResult, metrics_suffix, start)
          .Then(std::move(result_callback)));

  const RequestToken request_token;
  auto [it, inserted] = async_requests_.emplace(
      request_token, AsyncRequestData(std::move(success_callback)));
  CHECK(inserted);

  async_request_delegate_->SendRequest(rfhi.get(), request_token, params);

  it->second.timer->Start(
      FROM_HERE, wait_timeout_,
      base::BindOnce(&TextInputClientMac::OnAsyncRequestTimedOut,
                     weak_factory_.GetWeakPtr(), request_token,
                     std::move(timeout_callback)));
}

void TextInputClientMac::OnAsyncRequestTimedOut(
    const RequestToken& request_token,
    ResultAndTimeCallback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  {
    base::AutoLock lock(lock_);
    size_t erased = async_requests_.erase(request_token);
    if (!erased) {
      // `request_token` was already removed from the map on the IO thread
      // when a result arrived. The result is being posted to the UI thread so
      // ignore the timeout.
      return;
    }
  }
  std::move(callback).Run(base::LiveTicks::Now(), NoResultYetTag{});
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
  base::AutoLock lock(lock_);
  if (current_sync_request_ && *current_sync_request_ == request_token) {
    CHECK(std::holds_alternative<NoResultYetTag>(current_sync_result_));
    current_sync_result_ = result;
    condition_.Signal();
    return;
  }

  const auto it = async_requests_.find(request_token);
  if (it == async_requests_.end()) {
    // Stale request.
    return;
  }

  // Post the result back to the main thread.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(it->second.callback),
                                base::LiveTicks::Now(), result));
  async_requests_.erase(it);
}

void TextInputClientMac::SetAsyncRequestDelegateForTesting(
    std::unique_ptr<AsyncRequestDelegate> delegate) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  async_request_delegate_ =
      delegate ? std::move(delegate)
               : std::make_unique<DefaultAsyncRequestDelegate>();
}

void TextInputClientMac::SetTimeoutForTesting(base::TimeDelta timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  wait_timeout_ = timeout;
}

base::TimeDelta TextInputClientMac::GetTimeoutForTesting() const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  return wait_timeout_;
}

void TextInputClientMac::SetCharacterIndexWhileLockedForTesting(
    const RequestToken& request_token,
    uint32_t index) {
  // Drop the lock to signal the condition variable. Tests use this to simulate
  // a SyncGetCharacterIndexAtPoint() response that arrives before the
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
  // a SyncGetFirstRectForRange() response that arrives before the
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

TextInputClientMac::AsyncRequestData::AsyncRequestData(
    ResultAndTimeCallback callback)
    : callback(std::move(callback)),
      timer(new base::OneShotTimer(),
            // `AsyncRequestData` can be deleted on the IO thread, but `timer`
            // must be destroyed on the same thread that created it.
            base::OnTaskRunnerDeleter(
                base::SequencedTaskRunner::GetCurrentDefault())) {}

TextInputClientMac::AsyncRequestData::~AsyncRequestData() = default;

TextInputClientMac::AsyncRequestData::AsyncRequestData(AsyncRequestData&&) =
    default;

TextInputClientMac::AsyncRequestData&
TextInputClientMac::AsyncRequestData::operator=(AsyncRequestData&&) = default;

}  // namespace content
