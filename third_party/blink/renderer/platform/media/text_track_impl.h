// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_TEXT_TRACK_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_TEXT_TRACK_IMPL_H_

#include <memory>
#include <string>

#include "media/base/text_track.h"
#include "third_party/blink/renderer/platform/platform_export.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {
class WebInbandTextTrackImpl;
class WebMediaPlayerClient;

class PLATFORM_EXPORT TextTrackImpl : public media::TextTrack {
 public:
  // Constructor assumes ownership of the |text_track| object.
  TextTrackImpl(const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
                WebMediaPlayerClient* client,
                std::unique_ptr<WebInbandTextTrackImpl> text_track);

  TextTrackImpl(const TextTrackImpl&) = delete;
  TextTrackImpl& operator=(const TextTrackImpl&) = delete;
  ~TextTrackImpl() override;

  void addWebVTTCue(base::TimeDelta start,
                    base::TimeDelta end,
                    const std::string& id,
                    const std::string& content,
                    const std::string& settings) override;

 private:
  static void OnAddCue(WebInbandTextTrackImpl* text_track,
                       base::TimeDelta start,
                       base::TimeDelta end,
                       const std::string& id,
                       const std::string& content,
                       const std::string& settings);

  static void OnRemoveTrack(WebMediaPlayerClient* client,
                            std::unique_ptr<WebInbandTextTrackImpl> text_track);

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  WebMediaPlayerClient* client_;
  std::unique_ptr<WebInbandTextTrackImpl> text_track_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_MEDIA_TEXT_TRACK_IMPL_H_
