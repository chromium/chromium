// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_CONTROLLER_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_CONTROLLER_IMPL_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/picture_in_picture/picture_in_picture.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document_shutdown_observer.h"
#include "third_party/blink/renderer/core/frame/picture_in_picture_controller.h"
#include "third_party/blink/renderer/core/page/page_visibility_observer.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class HTMLVideoElement;
class PictureInPictureOptions;
class PictureInPictureWindow;
class TreeScope;
struct WebSize;

// The PictureInPictureControllerImpl is keeping the state and implementing the
// logic around the Picture-in-Picture feature. It is meant to be used as well
// by the Picture-in-Picture Web API and internally (eg. media controls). All
// consumers inside Blink modules/ should use this class to access
// Picture-in-Picture. In core/, they should use PictureInPictureController.
// PictureInPictureControllerImpl instance is associated to a Document. It is
// supplement and therefore can be lazy-initiated. Callers should consider
// whether they want to instantiate an object when they make a call.
class MODULES_EXPORT PictureInPictureControllerImpl
    : public PictureInPictureController,
      public PageVisibilityObserver,
      public DocumentShutdownObserver,
      public blink::mojom::blink::PictureInPictureSessionObserver {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureControllerImpl);

 public:
  explicit PictureInPictureControllerImpl(Document&);
  ~PictureInPictureControllerImpl() override = default;

  // Meant to be called internally by PictureInPictureController::From()
  // through ModulesInitializer.
  static PictureInPictureControllerImpl* Create(Document&);

  // Gets, or creates, PictureInPictureControllerImpl supplement on Document.
  // Should be called before any other call to make sure a document is attached.
  static PictureInPictureControllerImpl& From(Document&);

  // Returns whether system allows Picture-in-Picture feature or not for
  // the associated document.
  bool PictureInPictureEnabled() const;

  // Returns whether the document associated with the controller is allowed to
  // request Picture-in-Picture.
  Status IsDocumentAllowed() const;

  // Returns whether the combination of element and options can be in
  // Picture-in-Picture.
  Status VerifyElementAndOptions(const HTMLElement&,
                                 const PictureInPictureOptions*) const;

  // Returns element currently in Picture-in-Picture if any. Null otherwise.
  Element* PictureInPictureElement() const;
  Element* PictureInPictureElement(TreeScope&) const;

  // Returns video element whose autoPictureInPicture attribute was set most
  // recently.
  HTMLVideoElement* AutoPictureInPictureElement() const;

  // Returns whether entering Auto Picture-in-Picture is allowed.
  bool IsEnterAutoPictureInPictureAllowed() const;

  // Returns whether exiting Auto Picture-in-Picture is allowed.
  bool IsExitAutoPictureInPictureAllowed() const;

  // Implementation of PictureInPictureController.
  void EnterPictureInPicture(HTMLElement*,
                             PictureInPictureOptions*,
                             ScriptPromiseResolver*) override;
  void ExitPictureInPicture(HTMLVideoElement*, ScriptPromiseResolver*) override;
  void AddToAutoPictureInPictureElementsList(HTMLVideoElement*) override;
  void RemoveFromAutoPictureInPictureElementsList(HTMLVideoElement*) override;
  Status IsElementAllowed(const HTMLElement&) const override;
  bool IsPictureInPictureElement(const Element*) const override;
  void OnPictureInPictureStateChange() override;

  // Implementation of PictureInPictureSessionObserver.
  void OnWindowSizeChanged(const blink::WebSize&) override;
  void OnStopped() override;

  // Implementation of PageVisibilityObserver.
  void PageVisibilityChanged() override;

  // Implementation of DocumentShutdownObserver.
  void ContextDestroyed(Document*) override;

  void Trace(blink::Visitor*) override;

  bool IsSessionObserverReceiverBoundForTesting() {
    return session_observer_receiver_.is_bound();
  }

 private:
  void OnEnteredPictureInPicture(
      HTMLVideoElement*,
      ScriptPromiseResolver*,
      mojo::PendingRemote<mojom::blink::PictureInPictureSession>,
      const WebSize&);
  void OnExitedPictureInPicture(ScriptPromiseResolver*) override;

  // Makes sure the `picture_in_picture_service_` is set. Returns whether it was
  // initialized successfully.
  bool EnsureService();

  // The Picture-in-Picture element for the associated document.
  Member<HTMLVideoElement> picture_in_picture_element_;

  // The list of video elements for the associated document that are eligible
  // to Auto Picture-in-Picture.
  HeapDeque<Member<HTMLVideoElement>> auto_picture_in_picture_elements_;

  // The Picture-in-Picture window for the associated document.
  Member<PictureInPictureWindow> picture_in_picture_window_;

  // Mojo bindings for the session observer interface implemented by |this|.
  mojo::Receiver<mojom::blink::PictureInPictureSessionObserver>
      session_observer_receiver_;

  // Picture-in-Picture service living in the browser process.
  mojo::Remote<mojom::blink::PictureInPictureService>
      picture_in_picture_service_;

  // Instance of the Picture-in-Picture session sent back by the service.
  mojo::Remote<mojom::blink::PictureInPictureSession>
      picture_in_picture_session_;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureControllerImpl);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PICTURE_IN_PICTURE_PICTURE_IN_PICTURE_CONTROLLER_IMPL_H_
