/*
 * Copyright (C) 2008, 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_SOURCE_CODE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_SOURCE_CODE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_streamer.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class ScriptResource;
class SingleCachedMetadataHandler;

class CORE_EXPORT ScriptSourceCode final {
  DISALLOW_NEW();

 public:
  // For inline scripts.
  ScriptSourceCode(
      const String& source,
      ScriptSourceLocationType = ScriptSourceLocationType::kUnknown,
      SingleCachedMetadataHandler* = nullptr,
      const KURL& = KURL(),
      const TextPosition& = TextPosition::MinimumPosition());

  // For external scripts.
  //
  // We lose the encoding information from ScriptResource.
  // Not sure if that matters.
  ScriptSourceCode(ScriptStreamer*,
                   ScriptResource*,
                   ScriptStreamer::NotStreamingReason);

  // For (external) worker scripts. Leaves url fragment intact.
  //
  // If we move worker top-level script fetch to the worker thread, this could
  // probably be merged in to the main external script constructor.
  //
  // NOTE: It is import to keep the url fragment in this case so that errors in
  // worker scripts can include the fragment when reporting the location of the
  // failure. This is enforced by several tests in
  // external/wpt/workers/interfaces/WorkerGlobalScope/onerror/.
  ScriptSourceCode(const String& source,
                   SingleCachedMetadataHandler*,
                   const KURL&);

  ~ScriptSourceCode();
  void Trace(blink::Visitor*);

  const ParkableString& Source() const { return source_; }
  SingleCachedMetadataHandler* CacheHandler() const { return cache_handler_; }
  const KURL& Url() const { return url_; }
  const TextPosition& StartPosition() const { return start_position_; }
  ScriptSourceLocationType SourceLocationType() const {
    return source_location_type_;
  }
  const String& SourceMapUrl() const { return source_map_url_; }

  ScriptStreamer* Streamer() const { return streamer_; }
  ScriptStreamer::NotStreamingReason NotStreamingReason() const {
    return not_streaming_reason_;
  }

 private:
  ScriptSourceCode(
      const ParkableString& source,
      ScriptSourceLocationType = ScriptSourceLocationType::kUnknown,
      SingleCachedMetadataHandler* cache_handler = nullptr,
      const KURL& = KURL(),
      const TextPosition& start_position = TextPosition::MinimumPosition());

  const ParkableString source_;
  Member<SingleCachedMetadataHandler> cache_handler_;
  Member<ScriptStreamer> streamer_;
  ScriptStreamer::NotStreamingReason not_streaming_reason_;

  // The URL of the source code, which is primarily intended for DevTools
  // javascript debugger.
  //
  // Note that this can be different from the resulting script's base URL
  // (#concept-script-base-url) for inline classic scripts.
  const KURL url_;

  const String source_map_url_;
  const TextPosition start_position_;
  const ScriptSourceLocationType source_location_type_;
};

}  // namespace blink

WTF_ALLOW_INIT_WITH_MEM_FUNCTIONS(blink::ScriptSourceCode);

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_SOURCE_CODE_H_
