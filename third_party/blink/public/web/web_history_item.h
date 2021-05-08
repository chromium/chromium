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

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_HISTORY_ITEM_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_HISTORY_ITEM_H_

#include "services/network/public/mojom/referrer_policy.mojom-shared.h"
#include "third_party/blink/public/mojom/page_state/page_state.mojom-shared.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/public/platform/web_private_ptr.h"
#include "third_party/blink/public/platform/web_scroll_anchor_data.h"
#include "third_party/blink/public/platform/web_string.h"

namespace gfx {
class PointF;
}  // namespace gfx

namespace blink {

class HistoryItem;
class WebHTTPBody;
class WebString;
class WebSerializedScriptValue;
template <typename T>
class WebVector;

// Represents a frame-level navigation entry in session history.  A
// WebHistoryItem is a node in a tree.
//
// Copying a WebHistoryItem is cheap.
//
class WebHistoryItem {
 public:
  ~WebHistoryItem() { Reset(); }

  WebHistoryItem() = default;
  WebHistoryItem(const WebHistoryItem& h) { Assign(h); }
  WebHistoryItem& operator=(const WebHistoryItem& h) {
    Assign(h);
    return *this;
  }

  BLINK_EXPORT void Initialize();
  BLINK_EXPORT void Reset();
  BLINK_EXPORT void Assign(const WebHistoryItem&);

  bool IsNull() const { return private_.IsNull(); }

  BLINK_EXPORT WebString UrlString() const;
  BLINK_EXPORT void SetURLString(const WebString&);

  BLINK_EXPORT WebString GetReferrer() const;
  BLINK_EXPORT network::mojom::ReferrerPolicy GetReferrerPolicy() const;
  BLINK_EXPORT void SetReferrer(const WebString&,
                                network::mojom::ReferrerPolicy);

  BLINK_EXPORT const WebString& Target() const;
  BLINK_EXPORT void SetTarget(const WebString&);

  BLINK_EXPORT gfx::PointF VisualViewportScrollOffset() const;
  BLINK_EXPORT void SetVisualViewportScrollOffset(const gfx::PointF&);

  BLINK_EXPORT gfx::Point GetScrollOffset() const;
  BLINK_EXPORT void SetScrollOffset(const gfx::Point&);

  BLINK_EXPORT float PageScaleFactor() const;
  BLINK_EXPORT void SetPageScaleFactor(float);

  BLINK_EXPORT WebVector<WebString> GetDocumentState() const;
  BLINK_EXPORT void SetDocumentState(const WebVector<WebString>&);

  BLINK_EXPORT int64_t ItemSequenceNumber() const;
  BLINK_EXPORT void SetItemSequenceNumber(int64_t);

  BLINK_EXPORT int64_t DocumentSequenceNumber() const;
  BLINK_EXPORT void SetDocumentSequenceNumber(int64_t);

  BLINK_EXPORT mojom::ScrollRestorationType ScrollRestorationType() const;
  BLINK_EXPORT void SetScrollRestorationType(mojom::ScrollRestorationType);

  BLINK_EXPORT WebSerializedScriptValue StateObject() const;
  BLINK_EXPORT void SetStateObject(const WebSerializedScriptValue&);

  BLINK_EXPORT WebString HttpContentType() const;
  BLINK_EXPORT void SetHTTPContentType(const WebString&);

  BLINK_EXPORT WebHTTPBody HttpBody() const;
  BLINK_EXPORT void SetHttpBody(const WebHTTPBody&);

  BLINK_EXPORT WebVector<WebString> GetReferencedFilePaths() const;

  BLINK_EXPORT bool DidSaveScrollOrScaleState() const;

  BLINK_EXPORT ScrollAnchorData GetScrollAnchorData() const;
  BLINK_EXPORT void SetScrollAnchorData(const ScrollAnchorData&);

  BLINK_EXPORT WebString GetAppHistoryKey() const;
  BLINK_EXPORT void SetAppHistoryKey(const WebString&);

  BLINK_EXPORT WebString GetAppHistoryId() const;
  BLINK_EXPORT void SetAppHistoryId(const WebString&);

  BLINK_EXPORT WebSerializedScriptValue GetAppHistoryState() const;
  BLINK_EXPORT void SetAppHistoryState(const WebSerializedScriptValue&);

#if INSIDE_BLINK
  BLINK_EXPORT WebHistoryItem(HistoryItem*);
  BLINK_EXPORT WebHistoryItem& operator=(HistoryItem*);
  BLINK_EXPORT operator HistoryItem*() const;
#endif

 private:
  WebPrivatePtr<HistoryItem> private_;
  // TODO(dcheng): Remove this, since unique name is no longer a Blink concept.
  WebString target_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_HISTORY_ITEM_H_
