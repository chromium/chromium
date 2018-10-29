/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "third_party/blink/public/platform/web_http_load_info.h"

#include "third_party/blink/public/platform/web_http_header_visitor.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_info.h"

namespace blink {

void WebHTTPLoadInfo::Initialize() {
  private_ = base::AdoptRef(new ResourceLoadInfo());
}

void WebHTTPLoadInfo::Reset() {
  private_.Reset();
}

void WebHTTPLoadInfo::Assign(const WebHTTPLoadInfo& r) {
  private_ = r.private_;
}

WebHTTPLoadInfo::WebHTTPLoadInfo(scoped_refptr<ResourceLoadInfo> value)
    : private_(std::move(value)) {}

WebHTTPLoadInfo::operator scoped_refptr<ResourceLoadInfo>() const {
  return private_.Get();
}

int WebHTTPLoadInfo::HttpStatusCode() const {
  DCHECK(!private_.IsNull());
  return private_->http_status_code;
}

void WebHTTPLoadInfo::SetHTTPStatusCode(int status_code) {
  DCHECK(!private_.IsNull());
  private_->http_status_code = status_code;
}

WebString WebHTTPLoadInfo::HttpStatusText() const {
  DCHECK(!private_.IsNull());
  return private_->http_status_text;
}

void WebHTTPLoadInfo::SetHTTPStatusText(const WebString& status_text) {
  DCHECK(!private_.IsNull());
  private_->http_status_text = status_text;
}

static void AddHeader(HTTPHeaderMap* map,
                      const WebString& name,
                      const WebString& value) {
  HTTPHeaderMap::AddResult result = map->Add(name, value);
  // It is important that values are separated by '\n', not comma, otherwise
  // Set-Cookie header is not parseable.
  if (!result.is_new_entry)
    result.stored_value->value =
        result.stored_value->value + "\n" + String(value);
}

void WebHTTPLoadInfo::AddRequestHeader(const WebString& name,
                                       const WebString& value) {
  DCHECK(!private_.IsNull());
  AddHeader(&private_->request_headers, name, value);
}

void WebHTTPLoadInfo::AddResponseHeader(const WebString& name,
                                        const WebString& value) {
  DCHECK(!private_.IsNull());
  AddHeader(&private_->response_headers, name, value);
}

WebString WebHTTPLoadInfo::RequestHeadersText() const {
  DCHECK(!private_.IsNull());
  return private_->request_headers_text;
}

void WebHTTPLoadInfo::SetRequestHeadersText(const WebString& headers_text) {
  DCHECK(!private_.IsNull());
  private_->request_headers_text = headers_text;
}

WebString WebHTTPLoadInfo::ResponseHeadersText() const {
  DCHECK(!private_.IsNull());
  return private_->response_headers_text;
}

void WebHTTPLoadInfo::SetResponseHeadersText(const WebString& headers_text) {
  DCHECK(!private_.IsNull());
  private_->response_headers_text = headers_text;
}

}  // namespace blink
