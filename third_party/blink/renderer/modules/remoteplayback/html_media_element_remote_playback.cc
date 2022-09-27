// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/remoteplayback/html_media_element_remote_playback.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/qualified_name.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/modules/remoteplayback/remote_playback.h"

namespace blink {

// static
bool HTMLMediaElementRemotePlayback::FastHasAttribute(
    const HTMLMediaElement& element,
    const QualifiedName& name) {
  DCHECK(name == html_names::kDisableremoteplaybackAttr);
  return element.FastHasAttribute(name);
}

// static
void HTMLMediaElementRemotePlayback::SetBooleanAttribute(
    HTMLMediaElement& element,
    const QualifiedName& name,
    bool value) {
  DCHECK(name == html_names::kDisableremoteplaybackAttr);
  element.SetBooleanAttribute(name, value);

  RemotePlayback& remote_playback = RemotePlayback::From(element);
  if (value)
    remote_playback.RemotePlaybackDisabled();
}

// static
RemotePlayback* HTMLMediaElementRemotePlayback::remote(
    HTMLMediaElement& element) {
  RemotePlayback& remote_playback = RemotePlayback::From(element);
  Document& document = element.GetDocument();
  if (!document.GetFrame())
    return nullptr;

  return &remote_playback;
}

}  // namespace blink
