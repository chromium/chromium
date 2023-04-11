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

#include "third_party/blink/renderer/modules/mediasource/media_source_registry_impl.h"

#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// static
void MediaSourceRegistryImpl::Init() {
  DCHECK(IsMainThread());
  DEFINE_STATIC_LOCAL(MediaSourceRegistryImpl, instance, ());
  DVLOG(1) << __func__ << " instance=" << &instance;
}

void MediaSourceRegistryImpl::RegisterURL(const KURL& url,
                                          URLRegistrable* registrable) {
  DCHECK(IsMainThread());
  DCHECK_EQ(&registrable->Registry(), this);

  DCHECK(!url.IsEmpty());  // Caller of interface should already enforce this.

  DVLOG(1) << __func__ << " url=" << url << ", IsMainThread=" << IsMainThread();

  scoped_refptr<MediaSourceAttachment> attachment =
      base::AdoptRef(static_cast<MediaSourceAttachment*>(registrable));

  media_sources_.Set(url.GetString(), std::move(attachment));
}

void MediaSourceRegistryImpl::UnregisterURL(const KURL& url) {
  DCHECK(IsMainThread());
  DVLOG(1) << __func__ << " url=" << url << ", IsMainThread=" << IsMainThread();
  DCHECK(!url.IsEmpty());  // Caller of interface should already enforce this.

  auto iter = media_sources_.find(url.GetString());
  if (iter == media_sources_.end())
    return;

  scoped_refptr<MediaSourceAttachment> attachment = iter->value;
  attachment->Unregister();
  media_sources_.erase(iter);
}

scoped_refptr<MediaSourceAttachment> MediaSourceRegistryImpl::LookupMediaSource(
    const String& url) {
  DCHECK(IsMainThread());
  DCHECK(!url.empty());
  auto iter = media_sources_.find(url);
  if (iter == media_sources_.end())
    return nullptr;
  return iter->value;
}

MediaSourceRegistryImpl::MediaSourceRegistryImpl() {
  DCHECK(IsMainThread());
  MediaSourceAttachment::SetRegistry(this);
}

}  // namespace blink
