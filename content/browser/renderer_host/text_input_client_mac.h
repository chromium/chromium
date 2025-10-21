// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_
#define CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_

#include <limits>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/no_destructor.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "ui/base/mojom/attributed_string.mojom-forward.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Range;
}

namespace content {
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
  static TextInputClientMac* GetInstance();

  TextInputClientMac(const TextInputClientMac&) = delete;
  TextInputClientMac& operator=(const TextInputClientMac&) = delete;

  // ---- Sync IME implementation methods ----

  // These two methods have an associated pair of methods to get data from the
  // renderer. The Get*() methods block the calling thread (always the UI
  // thread) with a short timeout after the async message has been sent to the
  // renderer to lookup the information needed to respond to the system. The
  // Set*AndSignal() methods store the looked up information in this service and
  // signal the condition to allow the Get*() methods to unlock and return that
  // stored value.

  // Gets the index of the character at the specified point (in Blink
  // coordinates, in physical pixels). Returns UINT32_MAX if the request times
  // out or is not completed.
  uint32_t GetCharacterIndexAtPoint(RenderWidgetHost* rwh,
                                    const gfx::Point& point);
  // Gets the first rect of characters in the specified range. Returns
  // NSZeroRect if the request times out or is not completed. The result is in
  // Blink coordinates, in physical pixels.
  gfx::Rect GetFirstRectForRange(RenderWidgetHost* rwh,
                                 const gfx::Range& range);

  // When the renderer sends a reply, it will be received by TextInputHostImpl
  // (which implements the mojo interface), which will call the corresponding
  // method on the IO thread to unlock the condition and allow the Get*()
  // methods to continue/return.
  void SetCharacterIndexAndSignal(uint32_t index);
  void SetFirstRectAndSignal(const gfx::Rect& first_rect);

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

 private:
  friend base::NoDestructor<TextInputClientMac>;
  FRIEND_TEST_ALL_PREFIXES(TextInputClientMacTest, TimeoutRectForRange);
  TextInputClientMac();
  ~TextInputClientMac();

  // The critical sections that the Condition guards are in Get*() methods.
  // These methods lock the internal condition for use before the asynchronous
  // message is sent to the renderer to lookup the required information. These
  // are only used on the UI thread.
  void BeforeRequest() EXCLUSIVE_LOCK_FUNCTION(lock_);

  // Called at the end of a critical section. This will release the lock and
  // condition.
  void AfterRequest() UNLOCK_FUNCTION(lock_);

  uint32_t character_index_ = std::numeric_limits<uint32_t>::max();
  gfx::Rect first_rect_;

  base::Lock lock_;
  base::ConditionVariable condition_;

  // The amount of time that the browser process will wait for a response from
  // the renderer.
  base::TimeDelta wait_timeout_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_TEXT_INPUT_CLIENT_MAC_H_
