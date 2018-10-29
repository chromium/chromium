/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/modules/mediasource/media_source_registry.h"

#include "third_party/blink/renderer/modules/mediasource/media_source.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

MediaSourceRegistry& MediaSourceRegistry::Registry() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(MediaSourceRegistry, instance, ());
  return instance;
}

void MediaSourceRegistry::RegisterURL(SecurityOrigin*,
                                      const KURL& url,
                                      URLRegistrable* registrable) {
  DCHECK_EQ(&registrable->Registry(), this);
  DCHECK(IsMainThread());

  MediaSource* source = static_cast<MediaSource*>(registrable);
  source->AddedToRegistry();
  media_sources_->Set(url.GetString(), source);
}

void MediaSourceRegistry::UnregisterURL(const KURL& url) {
  DCHECK(IsMainThread());
  HeapHashMap<String, Member<MediaSource>>::iterator iter =
      media_sources_->find(url.GetString());
  if (iter == media_sources_->end())
    return;

  MediaSource* source = iter->value;
  media_sources_->erase(iter);
  source->RemovedFromRegistry();
}

URLRegistrable* MediaSourceRegistry::Lookup(const String& url) {
  DCHECK(IsMainThread());
  return url.IsNull() ? nullptr : media_sources_->at(url);
}

MediaSourceRegistry::MediaSourceRegistry()
    : media_sources_(new HeapHashMap<String, Member<MediaSource>>) {
  HTMLMediaSource::SetRegistry(this);
}

}  // namespace blink
