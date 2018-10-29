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

#include "third_party/blink/renderer/core/workers/worker_content_settings_client.h"

#include <memory>
#include "third_party/blink/public/platform/web_security_origin.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"

namespace blink {

WorkerContentSettingsClient* WorkerContentSettingsClient::Create(
    std::unique_ptr<WebContentSettingsClient> client) {
  return new WorkerContentSettingsClient(std::move(client));
}

WorkerContentSettingsClient::~WorkerContentSettingsClient() = default;

bool WorkerContentSettingsClient::RequestFileSystemAccessSync() {
  if (!client_)
    return true;
  return client_->RequestFileSystemAccessSync();
}

bool WorkerContentSettingsClient::AllowIndexedDB(const WebString& name) {
  if (!client_)
    return true;
  return client_->AllowIndexedDB(name, WebSecurityOrigin());
}

bool WorkerContentSettingsClient::AllowScriptFromSource(
    bool enabled_per_settings,
    const KURL& url) {
  if (client_) {
    return client_->AllowScriptFromSource(enabled_per_settings, url);
  }
  return enabled_per_settings;
}

bool WorkerContentSettingsClient::AllowRunningInsecureContent(
    bool enabled_per_settings,
    const SecurityOrigin* origin,
    const KURL& url) {
  if (client_) {
    return client_->AllowRunningInsecureContent(enabled_per_settings,
                                                WebSecurityOrigin(origin), url);
  }
  return enabled_per_settings;
}

const char WorkerContentSettingsClient::kSupplementName[] =
    "WorkerContentSettingsClient";

WorkerContentSettingsClient* WorkerContentSettingsClient::From(
    ExecutionContext& context) {
  WorkerClients* clients = To<WorkerOrWorkletGlobalScope>(context).Clients();
  DCHECK(clients);
  return Supplement<WorkerClients>::From<WorkerContentSettingsClient>(*clients);
}

WorkerContentSettingsClient::WorkerContentSettingsClient(
    std::unique_ptr<WebContentSettingsClient> client)
    : client_(std::move(client)) {}

void ProvideContentSettingsClientToWorker(
    WorkerClients* clients,
    std::unique_ptr<WebContentSettingsClient> client) {
  DCHECK(clients);
  WorkerContentSettingsClient::ProvideTo(
      *clients, WorkerContentSettingsClient::Create(std::move(client)));
}

}  // namespace blink
