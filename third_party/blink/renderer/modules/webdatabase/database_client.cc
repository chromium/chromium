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

#include "third_party/blink/renderer/modules/webdatabase/database_client.h"

#include "third_party/blink/public/platform/web_content_settings_client.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/webdatabase/database.h"
#include "third_party/blink/renderer/modules/webdatabase/inspector_database_agent.h"

namespace blink {

DatabaseClient::DatabaseClient(Page& page) : Supplement(page) {}

void DatabaseClient::Trace(Visitor* visitor) const {
  visitor->Trace(inspector_agent_);
  Supplement<Page>::Trace(visitor);
}

DatabaseClient* DatabaseClient::FromPage(Page* page) {
  return Supplement<Page>::From<DatabaseClient>(page);
}

DatabaseClient* DatabaseClient::From(ExecutionContext* context) {
  return DatabaseClient::FromPage(
      To<LocalDOMWindow>(context)->GetFrame()->GetPage());
}

const char DatabaseClient::kSupplementName[] = "DatabaseClient";

bool DatabaseClient::AllowDatabase(ExecutionContext* context) {
  DCHECK(context->IsContextThread());
  LocalDOMWindow* window = To<LocalDOMWindow>(context);
  return window->GetFrame()->AllowStorageAccessSyncAndNotify(
      WebContentSettingsClient::StorageType::kDatabase);
}

void DatabaseClient::DidOpenDatabase(blink::Database* database,
                                     const String& domain,
                                     const String& name,
                                     const String& version) {
  if (inspector_agent_)
    inspector_agent_->DidOpenDatabase(database, domain, name, version);
}

void DatabaseClient::SetInspectorAgent(InspectorDatabaseAgent* agent) {
  // TODO(dgozman): we should not set agent twice, but it's happening in OOPIF
  // case.
  inspector_agent_ = agent;
}

}  // namespace blink
