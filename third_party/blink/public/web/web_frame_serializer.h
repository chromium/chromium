/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_SERIALIZER_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_SERIALIZER_H_

#include "base/functional/callback.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_thread_safe_data.h"
#include "third_party/blink/public/platform/web_url.h"

namespace blink {

class WebLocalFrame;

// Serialization of frame contents into html or mhtml.
class BLINK_EXPORT WebFrameSerializer {
 public:
  // Delegate for controling the behavior of generateMHTMLParts method.
  class MHTMLPartsGenerationDelegate {
   public:
    // Tells whether to skip serialization of a subresource with a given URI.
    // Used to deduplicate resources across multiple frames.
    virtual bool ShouldSkipResource(const WebURL&) = 0;

    virtual bool UseBinaryEncoding() = 0;

    virtual bool RemovePopupOverlay() = 0;
  };

  // Generates and returns an MHTML header.
  //
  // Contents of the header (i.e. title and mime type) will be based
  // on the frame passed as an argument (which typically should be
  // the main, top-level frame).
  //
  // Same |boundary| needs to used for all generateMHTMLHeader and
  // generateMHTMLParts and generateMHTMLFooter calls that belong to the same
  // MHTML document (see also rfc1341, section 7.2.1, "boundary" description).
  static WebThreadSafeData GenerateMHTMLHeader(const WebString& boundary,
                                               WebLocalFrame*,
                                               MHTMLPartsGenerationDelegate*);

  // Generates and MHTML parts for the given frame and the savable resources
  // underneath. Calls `callback` with the result which should be appended to
  // the MHTML file.
  //
  // The same `boundary` needs to be used for all generateMHTMLHeader and
  // generateMHTMLParts and generateMHTMLFooter calls that belong to the same
  // MHTML document (see also rfc1341, section 7.2.1, "boundary" description).
  static void GenerateMHTMLParts(
      const WebString& boundary,
      WebLocalFrame*,
      MHTMLPartsGenerationDelegate*,
      base::OnceCallback<void(WebThreadSafeData)> callback);

  // FIXME: The following are here for unit testing purposes. Consider
  // changing the unit tests instead.

  // Generate the META for charset declaration.
  static WebString GenerateMetaCharsetDeclaration(const WebString& charset);
  // Generate the MOTW declaration.
  static WebString GenerateMarkOfTheWebDeclaration(const WebURL&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_FRAME_SERIALIZER_H_
