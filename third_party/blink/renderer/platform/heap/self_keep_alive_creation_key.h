// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SELF_KEEP_ALIVE_CREATION_KEY_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SELF_KEEP_ALIVE_CREATION_KEY_H_

namespace blink {

// Creation key needed to instantiate SelfKeepAlive objects.
//
// By adding your class as friend below you acknowledge that you have checked
// alternatives for keeping the object alive, see class comment of
// `SelfKeepAlive`.
class SelfKeepAliveCreationKey final {
 private:
  // NOLINTNEXTLINE: No =default to disallow aggregate initialization.
  SelfKeepAliveCreationKey() {}

  template <typename V8SessionObjectType>
  friend class AIContextObserver;
  friend class AudioContext;
  friend class BlobFileReaderClient;
  friend class BodyStreamBuffer;
  friend class BucketFileSystemBuilder;
  friend class CachedResponseFileReaderLoaderClient;
  friend class ClipboardWriter;
  friend class CSSImageGeneratorValue;
  friend class DetachedClient;
  friend class FileSystemDirectoryHandle;
  friend class InspectorFileReaderLoaderClient;
  friend class LanguageModelPromptBuilder;
  friend class MediaStreamAudioTrackUnderlyingSource;
  friend class MIDIAccessInitializer;
  friend class MojoWatcher;
  friend class PausableScriptExecutor;
  friend class RefCountedAndGarbageCollected;
  friend class Responder;
  friend class RTCDataChannel;
  friend class ScriptState;
  friend class SetMediaKeysHandler;
  friend class ThreadedMessagingProxyBase;
  friend class WebFormElementObserverImpl;
  friend class WebLocalFrameImpl;
  friend class WebRemoteFrameImpl;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_SELF_KEEP_ALIVE_CREATION_KEY_H_
