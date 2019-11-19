// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PICTURE_IN_PICTURE_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PICTURE_IN_PICTURE_CONTROLLER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/platform/supplementable.h"

namespace blink {

class HTMLElement;
class HTMLVideoElement;
class PictureInPictureOptions;
class ScriptPromiseResolver;

// PictureInPictureController allows to know if Picture-in-Picture is allowed
// for a video element in Blink outside of modules/ module. It
// is an interface that the module will implement and add a provider for.
class CORE_EXPORT PictureInPictureController
    : public GarbageCollected<PictureInPictureController>,
      public Supplement<Document> {
  USING_GARBAGE_COLLECTED_MIXIN(PictureInPictureController);

 public:
  static const char kSupplementName[];

  virtual ~PictureInPictureController() = default;

  // Should be called before any other call to make sure a document is attached.
  static PictureInPictureController& From(Document&);

  // Returns whether the given element is currently in Picture-in-Picture. It
  // returns false if PictureInPictureController is not attached to a document.
  static bool IsElementInPictureInPicture(const Element*);

  // List of Picture-in-Picture support statuses. If status is kEnabled,
  // Picture-in-Picture is enabled for a document or element, otherwise it is
  // not supported.
  enum class Status {
    kEnabled,
    kFrameDetached,
    kMetadataNotLoaded,
    kVideoTrackNotAvailable,
    kDisabledBySystem,
    kDisabledByFeaturePolicy,
    kDisabledByAttribute,
    kInvalidWidthOrHeightOption,
  };

  // Enter Picture-in-Picture for an element with options if any and resolve
  // promise if any.
  virtual void EnterPictureInPicture(HTMLElement*,
                                     PictureInPictureOptions*,
                                     ScriptPromiseResolver*) = 0;

  // Exit Picture-in-Picture for a video element and resolve promise if any.
  virtual void ExitPictureInPicture(HTMLVideoElement*,
                                    ScriptPromiseResolver*) = 0;

  // Returns whether a given element in a document associated with the
  // controller is allowed to request Picture-in-Picture.
  virtual Status IsElementAllowed(const HTMLElement&) const = 0;

  // Should be called when an element has exited Picture-in-Picture.
  virtual void OnExitedPictureInPicture(ScriptPromiseResolver*) = 0;

  // Add video element to the list of video elements for the associated document
  // that are eligible to Auto Picture-in-Picture.
  virtual void AddToAutoPictureInPictureElementsList(HTMLVideoElement*) = 0;

  // Remove video element from the list of video elements for the associated
  // document that are eligible to Auto Picture-in-Picture.
  virtual void RemoveFromAutoPictureInPictureElementsList(
      HTMLVideoElement*) = 0;

  // Notifies that one of the states used by Picture-in-Picture has changed.
  virtual void OnPictureInPictureStateChange() = 0;

  void Trace(blink::Visitor*) override;

 protected:
  explicit PictureInPictureController(Document&);

  // Returns whether the given element is currently in Picture-in-Picture.
  // It is protected so that clients use the static method
  // IsElementInPictureInPicture() that avoids creating the controller.
  virtual bool IsPictureInPictureElement(const Element*) const = 0;

  DISALLOW_COPY_AND_ASSIGN(PictureInPictureController);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_FRAME_PICTURE_IN_PICTURE_CONTROLLER_H_
