// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_MODULES_INITIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_MODULES_INITIALIZER_H_

#include "third_party/blink/renderer/core/core_initializer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class MODULES_EXPORT ModulesInitializer : public CoreInitializer {
 public:
  void Initialize() override;
  void RegisterInterfaces(mojo::BinderMap&) override;

 protected:
  void InitLocalFrame(LocalFrame&) const override;
  void OnClearWindowObjectInMainWorld(Document&,
                                      const Settings&) const override;

 private:
  void InstallSupplements(LocalFrame&) const override;
  void ProvideLocalFileSystemToWorker(WorkerGlobalScope&) const override;
  MediaControls* CreateMediaControls(HTMLMediaElement&,
                                     ShadowRoot&) const override;
  PictureInPictureController* CreatePictureInPictureController(
      Document&) const override;
  void InitInspectorAgentSession(DevToolsSession*,
                                 bool,
                                 InspectorDOMAgent*,
                                 InspectedFrames*,
                                 Page*) const override;
  std::unique_ptr<WebMediaPlayer> CreateWebMediaPlayer(
      WebLocalFrameClient*,
      HTMLMediaElement&,
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*) const override;
  WebRemotePlaybackClient* CreateWebRemotePlaybackClient(
      HTMLMediaElement&) const override;

  void ProvideModulesToPage(Page&, WebViewClient*) const override;
  void ForceNextWebGLContextCreationToFail() const override;

  void CollectAllGarbageForAnimationAndPaintWorkletForTesting() const override;

  void CloneSessionStorage(
      Page* clone_from_page,
      const SessionStorageNamespaceId& clone_to_namespace) override;

  void DidCommitLoad(LocalFrame&) override;
  void DidChangeManifest(LocalFrame&) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_MODULES_INITIALIZER_H_
