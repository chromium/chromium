// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_

#include <memory>
#include <optional>
#include <string_view>
#include <variant>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/no_destructor.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/token_type.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"
#include "ui/base/mojom/attributed_string.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/range/range.h"

namespace gfx {
class Range;
}

namespace content {
class RenderFrameHost;
class RenderFrameHostImpl;
class RenderWidgetHost;

// This class does synchronous IPC calls to the renderer process in order to
// help implement the synchronous NSTextInputClient API.
//
// The implementation involves two one-way IPC calls: a call is made to the
// renderer process and the renderer replies with a separate IPC call. If the
// reply is not received before a timeout fires, then the sync IPC is considered
// to have failed.
//
// This is done because Mojo sync calls do not have timeouts, and therefore they
// are unsuitable for use with an untrusted process.
//
// This class also handles async calls for the dictionary popup
// (`GetStringAtPoint` and `GetStringFromRange`). While these implementation
// details of dictionary lookup were originally added here as sync calls
// (they're now async), they remain here as they are also used in testing and
// thus it is convenient to have them on this class.
class CONTENT_EXPORT TextInputClientMac {
 public:
  using RequestToken = base::TokenType<struct RequestTokenTag>;

  // Generic wrappers for params and result types of either
  // *GetCharacterIndexAtPoint or *GetFirstRectForRange.

  using RequestParams = std::variant<
      // CharacterIndexAtPoint param.
      gfx::Point,
      // FirstRectForRange param.
      gfx::Range>;

  // Tag class indicating no result was received yet. If this is returned from
  // SyncRequest(), the request timed out.
  struct NoResultYetTag {};

  // Tag class used to indicate a request will never get a result (eg. if
  // there's no focused frame to query).
  struct FailedRequestTag {};

  using ResultValue = std::variant<
      // States with no result.
      NoResultYetTag,
      FailedRequestTag,
      // CharacterIndexAtPoint result.
      uint32_t,
      // FirstRectForRange result.
      gfx::Rect>;

  // Used by both AsyncGet*() methods and the blocking SyncGet*() wrappers to
  // start async requests. Can be overridden for testing.
  class AsyncRequestDelegate {
   public:
    virtual ~AsyncRequestDelegate() = default;

    virtual void GetCharacterIndexAtPoint(RenderFrameHost* rfh,
                                          const RequestToken& request_token,
                                          const gfx::Point& point) = 0;
    virtual void GetFirstRectForRange(RenderFrameHost* rfh,
                                      const RequestToken& request_token,
                                      const gfx::Range& range) = 0;

    // Helper to call the correct getter function for `params`.
    void SendRequest(RenderFrameHost* rfh,
                     const RequestToken& request_token,
                     const RequestParams& params);
  };

  static TextInputClientMac* GetInstance();

  TextInputClientMac(const TextInputClientMac&) = delete;
  TextInputClientMac& operator=(const TextInputClientMac&) = delete;

  // ---- Sync/Async IME implementation methods ----

  // These two sets of methods have an associated pair of methods to get data
  // from the renderer.
  //
  // The SyncGet*() methods block the calling thread (always the UI thread) with
  // a short timeout after the async message has been sent to the renderer to
  // lookup the information needed to respond to the system. The Set*AndSignal()
  // methods store the looked up information in this service and signal the
  // condition to allow the SyncGet*() methods to unlock and return that stored
  // value.
  //
  // The AsyncGet*() methods use the same mechanism but don't block the calling
  // thread. Instead they store a `result_callback`, and Set*AndSignal() passes
  // the looked up information to the callback.

  // Gets the index of the character at the specified point (in Blink
  // coordinates, in physical pixels). Returns UINT32_MAX if the request times
  // out or is not completed.
  uint32_t SyncGetCharacterIndexAtPoint(RenderWidgetHost* rwh,
                                        const gfx::Point& point);
  void AsyncGetCharacterIndexAtPoint(
      RenderWidgetHost* rwh,
      const gfx::Point& point,
      base::OnceCallback<void(uint32_t)> result_callback);

  // Gets the first rect of characters in the specified range. Returns
  // NSZeroRect if the request times out or is not completed. The result is in
  // Blink coordinates, in physical pixels.
  gfx::Rect SyncGetFirstRectForRange(RenderWidgetHost* rwh,
                                     const gfx::Range& range);
  void AsyncGetFirstRectForRange(
      RenderWidgetHost* rwh,
      const gfx::Range& range,
      base::OnceCallback<void(gfx::Rect)> result_callback);

  // When the renderer sends a reply, it will be received by TextInputHostImpl
  // (which implements the mojo interface), which will call the corresponding
  // method on the IO thread. These methods either unlock the condition and
  // allow the SyncGet*() methods to continue/return, or post the result to the
  // AsyncGet*() method's callback on the UI thread.
  void SetCharacterIndexAndSignal(const RequestToken& request_token,
                                  uint32_t index);
  void SetFirstRectAndSignal(const RequestToken& request_token,
                             const gfx::Rect& first_rect);

  // ---- Dictionary lookup implementation methods ----

  // A shared callback for the following two methods. The values returned are:
  //   - The attributed string, either of the word that contains the given point
  //     (for GetStringAtPoint) or of the string specified by the given range
  //     (for GetStringFromRange) and
  //   - The lower-left baseline coordinate (in Blink coordinates, in physical
  //     pixels) of the returned string as displayed on the screen.
  using GetStringCallback =
      base::OnceCallback<void(ui::mojom::AttributedStringPtr,
                              const gfx::Point&)>;

  // Given a point (in Blink coordinates, in physical pixels), looks up a word
  // in the given RenderWidgetHost.
  //
  // This method is useful for implementing -quickLookWithEvent:, which AppKit
  // calls when the user taps on a view using 3 fingers.
  void GetStringAtPoint(RenderWidgetHost* rwh,
                        const gfx::Point& point,
                        GetStringCallback callback);

  // Given a range, looks up a string in the given RenderWidgetHost.
  //
  // This method is useful for implementing "Look Up <<selection>>" in the
  // context menu.
  void GetStringFromRange(RenderWidgetHost* rwh,
                          const gfx::Range& range,
                          GetStringCallback callback);

  // Overrides the default AsyncRequestDelegate with a test fake. If `delegate`
  // is nullptr, restores the default.
  void SetAsyncRequestDelegateForTesting(
      std::unique_ptr<AsyncRequestDelegate> delegate);

  void SetTimeoutForTesting(base::TimeDelta timeout);
  base::TimeDelta GetTimeoutForTesting() const;

  // Allows tests to call setters while already holding the lock, to prevent
  // deadlocks when calling them from the main test thread.
  void SetCharacterIndexWhileLockedForTesting(const RequestToken& request_token,
                                              uint32_t index);
  void SetFirstRectWhileLockedForTesting(const RequestToken& request_token,
                                         const gfx::Rect& first_rect);

 private:
  friend base::NoDestructor<TextInputClientMac>;

  // A callback taking the result value and the time it was received.
  using ResultAndTimeCallback =
      base::OnceCallback<void(base::LiveTicks, ResultValue)>;

  struct AsyncRequestData {
    explicit AsyncRequestData(ResultAndTimeCallback callback);
    ~AsyncRequestData();

    // Move-only
    AsyncRequestData(AsyncRequestData&&);
    AsyncRequestData& operator=(AsyncRequestData&&);
    AsyncRequestData(const AsyncRequestData&) = delete;
    AsyncRequestData& operator=(const AsyncRequestData&) = delete;

    ResultAndTimeCallback callback;

    // OneShotTimer is not copyable or movable, but unique_ptr<OneShotTimer> is.
    std::unique_ptr<base::OneShotTimer, base::OnTaskRunnerDeleter> timer;
  };

  TextInputClientMac();
  ~TextInputClientMac();

  // Shared implementations of the public SyncGet* and AsyncGet* methods. `rfhi`
  // is the currently focused frame, which is a WeakPtr in case the
  // RenderFrameHost is deleted while waiting for the response.
  ResultValue SyncRequest(base::WeakPtr<RenderFrameHostImpl> rfhi,
                          const RequestParams& params,
                          std::string_view metrics_suffix)
      VALID_CONTEXT_REQUIRED(thread_checker_) LOCKS_EXCLUDED(lock_);
  void AsyncRequest(base::WeakPtr<RenderFrameHostImpl> rfhi,
                    const RequestParams& params,
                    std::string_view metrics_suffix,
                    base::OnceCallback<void(ResultValue)> result_callback)
      VALID_CONTEXT_REQUIRED(thread_checker_) LOCKS_EXCLUDED(lock_);

  // Invoked when a request sent by AsyncRequest() times out.
  void OnAsyncRequestTimedOut(const RequestToken& request_token,
                              ResultAndTimeCallback callback)
      LOCKS_EXCLUDED(lock_);

  // Shared implementation of the Set*AndSignal() methods.
  void SetResultAndSignal(const RequestToken& request_token, ResultValue result)
      LOCKS_EXCLUDED(lock_);

  THREAD_CHECKER(thread_checker_);

  std::optional<RequestToken> current_sync_request_ GUARDED_BY(lock_);
  ResultValue current_sync_result_ GUARDED_BY(lock_);

  absl::flat_hash_map<RequestToken, AsyncRequestData> async_requests_
      GUARDED_BY(lock_);

  base::Lock lock_;
  base::ConditionVariable condition_;

  std::unique_ptr<AsyncRequestDelegate> async_request_delegate_
      GUARDED_BY_CONTEXT(thread_checker_);

  // True iff `current_sync_request_` has a value. This is a separate variable
  // that's accessed only on the main thread so that it can be tested without
  // taking the lock, which would deadlock if the main thread already holds it.
  bool in_sync_request_ GUARDED_BY_CONTEXT(thread_checker_) = false;

  base::TimeDelta wait_timeout_ GUARDED_BY_CONTEXT(thread_checker_) =
      base::Milliseconds(1500);

  base::WeakPtrFactory<TextInputClientMac> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_
