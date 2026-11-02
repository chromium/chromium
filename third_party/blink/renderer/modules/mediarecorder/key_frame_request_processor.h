// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_KEY_FRAME_REQUEST_PROCESSOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_KEY_FRAME_REQUEST_PROCESSOR_H_

#include <variant>

#include "base/functional/callback.h"
#include "base/time/time.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Class handling the logic of periodically requesting keyframes. This class is
// thread-compatible. Key frame requests are observable by the page, so
// timestamps are coarsened like performance.now() before use.
class MODULES_EXPORT KeyFrameRequestProcessor {
 public:
  // TODO(crbug.com/402412612): Replace NotConfiguredTag with std::monostate.
  struct NotConfiguredTag {};
  using Configuration = std::variant<NotConfiguredTag,  // Not configured
                                     uint64_t,          // Count
                                     base::TimeDelta    // Duration
                                     >;
  using TimeNowCallback = base::OnceCallback<base::TimeTicks()>;

  KeyFrameRequestProcessor() = default;
  KeyFrameRequestProcessor(Configuration config,
                           bool cross_origin_isolated_capability);

  // Call to update the configuration.
  void UpdateConfig(Configuration config,
                    bool cross_origin_isolated_capability) {
    config_ = config;
    cross_origin_isolated_capability_ = cross_origin_isolated_capability;
  }
  // Notify the processor that a keyframe has or will soon be processed.
  void OnKeyFrame(base::TimeTicks now);
  // Returns if a key frame should be requested, in which case true will not
  // be returned again until OnKeyFrame() has ben called.
  bool OnFrameAndShouldRequestKeyFrame(base::TimeTicks now);

 private:
  base::TimeTicks CoarsenTime(base::TimeTicks time) const;

  Configuration config_;
  bool cross_origin_isolated_capability_ = false;
  struct {
    size_t frame_counter;
    base::TimeTicks timestamp;
  } last_key_frame_received_ = {0, base::TimeTicks()};
  bool consider_key_frame_request_ = false;
  size_t frame_counter_ = 0;
};
}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MEDIARECORDER_KEY_FRAME_REQUEST_PROCESSOR_H_
