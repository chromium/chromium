// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_EXTENSION_SCRIPT_STREAMER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_EXTENSION_SCRIPT_STREAMER_H_

#include "base/time/time.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"

namespace blink {

class BackgroundInlineScriptStreamer;
class InlineScriptStreamer;
class WebLocalFrame;
class WebString;

class BLINK_EXPORT ExtensionScriptStreamer {
 public:
#if INSIDE_BLINK
  explicit ExtensionScriptStreamer(scoped_refptr<BackgroundInlineScriptStreamer>
                                       background_inline_script_streamer);
  InlineScriptStreamer* GetInlineScriptStreamer() const;
#endif  // INSIDE_BLINK
  ExtensionScriptStreamer() = default;
  ExtensionScriptStreamer(const ExtensionScriptStreamer&);
  ~ExtensionScriptStreamer();

  bool CancelStreamingIfNotStarted() const;

  static ExtensionScriptStreamer PostStreamingTaskToBackgroundThread(
      WebLocalFrame* web_frame,
      const WebString& content,
      const std::string_view& url,
      uint64_t script_id,
      base::TimeDelta wait_timeout);

 private:
  WebPrivatePtrForRefCounted<BackgroundInlineScriptStreamer>
      background_inline_script_streamer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_EXTENSION_SCRIPT_STREAMER_H_
