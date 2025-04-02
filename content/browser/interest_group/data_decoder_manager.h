// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_DATA_DECODER_MANAGER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_DATA_DECODER_MANAGER_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <optional>
#include <tuple>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "url/origin.h"

namespace data_decoder {
class DataDecoder;
}

namespace content {

// DataDecoderManager keeps a global cache of DataDecoders for use by Protected
// Audience, partitioned by main frame origin and owner origin. Consumers
// provide the two origins when requesting a Handle, then make their DataDecoder
// invocations using the Handle, and destroy the Handle only when the calls have
// completed.
//
// The DataDecoderManager returns Handles with the same underlying DataDecoder
// when passed the same two origins, and times out DataDecoders with no live
// Handles after an idle timeout period.
class CONTENT_EXPORT DataDecoderManager {
 public:
  // The duration after which DataDecoders without any active Handles. Five
  // seconds matches the DataDecoder's internal default time. The task to clean
  // up idle decoders is run at most once every `kIdleTimeout`, so it's possible
  // for idle decoders to be around longer than the timeout.
  static constexpr base::TimeDelta kIdleTimeout{base::Seconds(5)};

  // PerOriginDecoder and DecoderMap are public only for internal use in Handle.

  // Per top-frame origin and owner-origin decoder. It tracks the number of live
  // handles, and is kept alive for `kIdleTimeout` after the last associated
  // Handle has been destroyed.
  struct PerOriginDecoder {
    // Since DataDecoderManager uses its own timeout, use a longer timeout than
    // the default, to prefer DataDecoderManager's timeout logic over
    // DataDecoder's logic. Some consumers may prewarm an idle decoder,
    // resulting in the underlying decoder actually being idle for more than
    // `kIdleTimeout`, so a longer timeout can help in that case.
    data_decoder::DataDecoder decoder{/*idle_timeout=*/base::Minutes(1)};

    // The time when the process the decoder wraps is expected to shutdown. Set
    // to be `kIdleTimeout` in the future when the final Handle is destroyed,
    // which is expected to roughly coincide with the last pending callback
    // being invoked. This should be std::nullopt whenever `num_handles` is
    // non-zero.
    std::optional<base::TimeTicks> expected_service_teardown_time;

    // Tracks the number of handles associated with the decoder.
    size_t num_handles = 0;
  };
  using DecoderMap =
      std::map<std::pair<url::Origin, url::Origin>, PerOriginDecoder>;

  // Handle returned when requesting a DataDecoder. Caller must maintain
  // ownership of a Handle while waiting on the callback.
  class CONTENT_EXPORT Handle {
   public:
    Handle(base::PassKey<DataDecoderManager>,
           DataDecoderManager* manager,
           DecoderMap::iterator decoder_it);
    ~Handle();

    // Caller should not keep a pointer to the DataDecoder. Destroying the
    // Handle is not guaranteed to cancel pending tasks (though it may cancel
    // them), so all callbacks must be bound to WeakPtrs, and weak pointers
    // destroyed if the caller no longer wishes to receive the results of any
    // pending callbacks.
    data_decoder::DataDecoder& data_decoder();

   private:
    const raw_ptr<DataDecoderManager> manager_;
    DecoderMap::iterator decoder_it_;
  };

  DataDecoderManager();
  DataDecoderManager(const DataDecoderManager&) = delete;
  ~DataDecoderManager();

  DataDecoderManager& operator=(const DataDecoderManager&) = delete;

  // Returns a Handle owning a shared DataDecoder for use in the specified
  // context. The Handle must not outlive the DataDecoderManager.
  std::unique_ptr<Handle> GetHandle(const url::Origin& main_frame_origin,
                                    const url::Origin& owner_origin);

  // Returns the total number of DataDecoders that DataDecoderManager currently
  // has, including idle ones.
  size_t NumDecodersForTesting() const;

  // Returns the number of live Handles for the specified context. Returns 0 if
  // there are no Handles, but the specified decoder still exists. Returns
  // std::nullopt if no such DataDecoder currently exists.
  std::optional<size_t> GetHandleCountForTesting(
      const url::Origin& main_frame_origin,
      const url::Origin& owner_origin) const;

 private:
  void OnHandleDestroyed(DecoderMap::iterator decoder_it);

  void CleanUpIdleDecoders();

  DecoderMap decoder_map_;

  base::OneShotTimer timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_DATA_DECODER_MANAGER_H_
