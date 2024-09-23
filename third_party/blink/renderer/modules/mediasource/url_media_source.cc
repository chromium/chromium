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

#include "third_party/blink/renderer/modules/mediasource/url_media_source.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/media/media_source_attachment.h"
#include "third_party/blink/renderer/core/url/dom_url.h"
#include "third_party/blink/renderer/modules/mediasource/attachment_creation_pass_key_provider.h"
#include "third_party/blink/renderer/modules/mediasource/cross_thread_media_source_attachment.h"
#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/modules/mediasource/media_source_registry_impl.h"
#include "third_party/blink/renderer/modules/mediasource/same_thread_media_source_attachment.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

// static
String URLMediaSource::createObjectURL(ScriptState* script_state,
                                       MediaSource* source) {
  // Since WebWorkers previously could not obtain MediaSource objects, we should
  // be on the main thread unless MediaSourceInWorkers is enabled and we're in a
  // dedicated worker execution context. Even in that case, we must prevent real
  // object URL creation+registration here if MediaSourceInWorkersUsingHandle is
  // enabled, since in that case, MediaSourceHandle is the exclusive attachment
  // mechanism for a worker-owned MediaSource.
  ExecutionContext* execution_context = ExecutionContext::From(script_state);
  DCHECK(execution_context);
  DCHECK(source);

  UseCounter::Count(execution_context, WebFeature::kCreateObjectURLMediaSource);

  MediaSourceAttachment* attachment;
  if (execution_context->IsDedicatedWorkerGlobalScope()) {
    DCHECK(!IsMainThread());

    UseCounter::Count(execution_context,
                      WebFeature::kCreateObjectURLMediaSourceFromWorker);

    // Return empty string, which if attempted to be used as media element src
    // by the app will cause the required failure. Note that the partial
    // interface for this method in WebIDL must have same exposure as the
    // extended createObjectURL interface, so we cannot simply remove this.
    return String();
  }

  // Other contexts outside of main window thread or conditionally a dedicated
  // worker thread are not supported (like Shared Worker and Service Worker).
  DCHECK(IsMainThread() && execution_context->IsWindow());

  // PassKey provider usage here ensures that we are allowed to call the
  // attachment constructor.
  attachment = new SameThreadMediaSourceAttachment(
      source, AttachmentCreationPassKeyProvider::GetPassKey());

  // The creation of a ThreadSafeRefCounted attachment object, above, should
  // have a refcount of 1 immediately. It will be adopted into a scoped_refptr
  // in MediaSourceRegistryImpl::RegisterURL. See also MediaSourceAttachment
  // (and usage in HTMLMediaElement, MediaSourceRegistry{Impl}, and MediaSource)
  // for further detail.
  DCHECK(attachment->HasOneRef());

  String url = DOMURL::CreatePublicURL(execution_context, attachment);

  // If attachment's registration failed, release its start-at-one reference to
  // let it be destructed.
  if (url.empty())
    attachment->Release();

  return url;
}

}  // namespace blink
