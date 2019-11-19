/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_SOURCE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_MEDIA_HTML_MEDIA_SOURCE_H_

#include <memory>
#include "third_party/blink/public/platform/web_time_range.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/fileapi/url_registry.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class WebMediaSource;
class HTMLMediaElement;
class TimeRanges;
class TrackBase;

class CORE_EXPORT HTMLMediaSource : public URLRegistrable,
                                    public GarbageCollectedMixin {
 public:
  static void SetRegistry(URLRegistry*);
  static HTMLMediaSource* Lookup(const String& url) {
    return registry_ ? static_cast<HTMLMediaSource*>(registry_->Lookup(url))
                     : nullptr;
  }

  // Called when an HTMLMediaElement is attempting to attach to this object,
  // and helps enforce attachment to at most one element at a time.
  // If already attached, returns false. Otherwise, must be in
  // 'closed' state, and returns true to indicate attachment success.
  // Reattachment allowed by first calling close() (even if already in
  // 'closed').
  // Once attached, the source uses the element to synchronously service some
  // API operations like duration change that may need to initiate seek.
  virtual bool AttachToElement(HTMLMediaElement*) = 0;
  virtual void SetWebMediaSourceAndOpen(std::unique_ptr<WebMediaSource>) = 0;
  virtual void Close() = 0;
  virtual bool IsClosed() const = 0;
  virtual double duration() const = 0;

  // 'Internal' in these methods doesn't mean private, it means that they are
  // internal to chromium and are not exposed to JavaScript.

  // The JavaScript exposed version of this is Buffered.
  virtual WebTimeRanges BufferedInternal() const = 0;

  virtual WebTimeRanges SeekableInternal() const = 0;
  virtual TimeRanges* Buffered() const = 0;
  virtual void OnTrackChanged(TrackBase*) = 0;

  // URLRegistrable
  URLRegistry& Registry() const override { return *registry_; }

 private:
  static URLRegistry* registry_;
};

}  // namespace blink

#endif
