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

#include "third_party/blink/public/platform/web_url_load_timing.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"

namespace blink {

void WebURLLoadTiming::Initialize() {
  private_ = ResourceLoadTiming::Create();
}

void WebURLLoadTiming::Reset() {
  private_.Reset();
}

void WebURLLoadTiming::Assign(const WebURLLoadTiming& other) {
  private_ = other.private_;
}

base::TimeTicks WebURLLoadTiming::RequestTime() const {
  return private_->RequestTime();
}

void WebURLLoadTiming::SetRequestTime(base::TimeTicks time) {
  private_->SetRequestTime(time);
}

base::TimeTicks WebURLLoadTiming::ProxyStart() const {
  return private_->ProxyStart();
}

void WebURLLoadTiming::SetProxyStart(base::TimeTicks start) {
  private_->SetProxyStart(start);
}

base::TimeTicks WebURLLoadTiming::ProxyEnd() const {
  return private_->ProxyEnd();
}

void WebURLLoadTiming::SetProxyEnd(base::TimeTicks end) {
  private_->SetProxyEnd(end);
}

base::TimeTicks WebURLLoadTiming::DnsStart() const {
  return private_->DnsStart();
}

void WebURLLoadTiming::SetDNSStart(base::TimeTicks start) {
  private_->SetDnsStart(start);
}

base::TimeTicks WebURLLoadTiming::DnsEnd() const {
  return private_->DnsEnd();
}

void WebURLLoadTiming::SetDNSEnd(base::TimeTicks end) {
  private_->SetDnsEnd(end);
}

base::TimeTicks WebURLLoadTiming::ConnectStart() const {
  return private_->ConnectStart();
}

void WebURLLoadTiming::SetConnectStart(base::TimeTicks start) {
  private_->SetConnectStart(start);
}

base::TimeTicks WebURLLoadTiming::ConnectEnd() const {
  return private_->ConnectEnd();
}

void WebURLLoadTiming::SetConnectEnd(base::TimeTicks end) {
  private_->SetConnectEnd(end);
}

base::TimeTicks WebURLLoadTiming::WorkerStart() const {
  return private_->WorkerStart();
}

void WebURLLoadTiming::SetWorkerStart(base::TimeTicks start) {
  private_->SetWorkerStart(start);
}

base::TimeTicks WebURLLoadTiming::WorkerReady() const {
  return private_->WorkerReady();
}

void WebURLLoadTiming::SetWorkerReady(base::TimeTicks ready) {
  private_->SetWorkerReady(ready);
}

base::TimeTicks WebURLLoadTiming::SendStart() const {
  return private_->SendStart();
}

void WebURLLoadTiming::SetSendStart(base::TimeTicks start) {
  private_->SetSendStart(start);
}

base::TimeTicks WebURLLoadTiming::SendEnd() const {
  return private_->SendEnd();
}

void WebURLLoadTiming::SetSendEnd(base::TimeTicks end) {
  private_->SetSendEnd(end);
}

base::TimeTicks WebURLLoadTiming::ReceiveHeadersStart() const {
  return private_->ReceiveHeadersStart();
}

void WebURLLoadTiming::SetReceiveHeadersStart(base::TimeTicks start) {
  private_->SetReceiveHeadersStart(start);
}

base::TimeTicks WebURLLoadTiming::ReceiveHeadersEnd() const {
  return private_->ReceiveHeadersEnd();
}

void WebURLLoadTiming::SetReceiveHeadersEnd(base::TimeTicks end) {
  private_->SetReceiveHeadersEnd(end);
}

base::TimeTicks WebURLLoadTiming::SslStart() const {
  return private_->SslStart();
}

void WebURLLoadTiming::SetSSLStart(base::TimeTicks start) {
  private_->SetSslStart(start);
}

base::TimeTicks WebURLLoadTiming::SslEnd() const {
  return private_->SslEnd();
}

void WebURLLoadTiming::SetSSLEnd(base::TimeTicks end) {
  private_->SetSslEnd(end);
}

base::TimeTicks WebURLLoadTiming::PushStart() const {
  return private_->PushStart();
}

void WebURLLoadTiming::SetPushStart(base::TimeTicks start) {
  private_->SetPushStart(start);
}

base::TimeTicks WebURLLoadTiming::PushEnd() const {
  return private_->PushEnd();
}

void WebURLLoadTiming::SetPushEnd(base::TimeTicks end) {
  private_->SetPushEnd(end);
}

WebURLLoadTiming::WebURLLoadTiming(scoped_refptr<ResourceLoadTiming> value)
    : private_(std::move(value)) {}

WebURLLoadTiming& WebURLLoadTiming::operator=(
    scoped_refptr<ResourceLoadTiming> value) {
  private_ = std::move(value);
  return *this;
}

WebURLLoadTiming::operator scoped_refptr<ResourceLoadTiming>() const {
  return private_.Get();
}

WebURLLoadTiming WebURLLoadTiming::DeepCopy() const {
  return private_->DeepCopy();
}

bool WebURLLoadTiming::operator==(const WebURLLoadTiming& other) const {
  return *private_ == *other.private_;
}

}  // namespace blink

namespace WTF {

CrossThreadCopier<blink::WebURLLoadTiming>::Type CrossThreadCopier<
    blink::WebURLLoadTiming>::Copy(const blink::WebURLLoadTiming& timing) {
  return timing.DeepCopy();
}

}  // namespace WTF
