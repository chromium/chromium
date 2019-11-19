// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_

#include <memory>

#include "third_party/blink/renderer/core/dom/events/event_target.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_typed_array.h"
#include "third_party/blink/renderer/modules/xr/xr_native_origin_information.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/transforms/transformation_matrix.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class TransformationMatrix;
class XRInputSource;
class XRPose;
class XRSession;

class XRSpace : public EventTargetWithInlineData {
  DEFINE_WRAPPERTYPEINFO();

 protected:
  explicit XRSpace(XRSession* session);

 public:
  ~XRSpace() override;

  // Gets a default viewer pose appropriate for this space. This is an identity
  // for viewer space, null for everything else.
  virtual std::unique_ptr<TransformationMatrix> DefaultViewerPose();

  // Gets the pose of this space's origin in mojo space. This is a transform
  // that maps from this space to mojo space (aka device space). Unless noted
  // otherwise, all data returned over vr_service.mojom interfaces is expressed
  // in mojo space coordinates. Returns nullptr if computing a transform is not
  // possible.
  virtual std::unique_ptr<TransformationMatrix> MojoFromSpace();

  // Gets the pose of the mojo origin in this reference space, corresponding
  // to a transform from mojo coordinates to reference space coordinates.
  virtual std::unique_ptr<TransformationMatrix> SpaceFromMojo(
      const TransformationMatrix& mojo_from_viewer);

  // Gets the viewer pose in this space, corresponding to a transform from
  // viewer coordinates to this space's coordinates. (The position elements of
  // the transformation matrix are the viewer's location in this space's
  // coordinates.)
  virtual std::unique_ptr<TransformationMatrix> SpaceFromViewer(
      const TransformationMatrix& mojo_from_viewer);

  // Gets an input pose in this space. This requires the viewer pose as
  // an additional input since a "viewer" space needs to transform the
  // input pose to headset-relative coordinates.
  virtual std::unique_ptr<TransformationMatrix> SpaceFromInputForViewer(
      const TransformationMatrix& mojo_from_input,
      const TransformationMatrix& mojo_from_viewer);

  virtual XRPose* getPose(XRSpace* other_space,
                          const TransformationMatrix* mojo_from_viewer);

  // Gets the viewer pose in this space, including using an appropriate
  // default pose (i.e. if tracking is lost), and applying originOffset
  // as applicable. TODO(https://crbug.com/1008466): consider moving
  // the originOffset handling to a separate class?
  std::unique_ptr<TransformationMatrix> SpaceFromViewerWithDefaultAndOffset(
      const TransformationMatrix* mojo_from_viewer);

  XRSession* session() const { return session_; }

  // EventTarget overrides.
  ExecutionContext* GetExecutionContext() const override;
  const AtomicString& InterfaceName() const override;

  // Return origin offset matrix, aka native_origin_from_offset_space.
  virtual TransformationMatrix OriginOffsetMatrix();
  virtual TransformationMatrix InverseOriginOffsetMatrix();

  virtual base::Optional<XRNativeOriginInformation> NativeOrigin() const = 0;

  void Trace(blink::Visitor* visitor) override;

 private:
  const Member<XRSession> session_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_XR_XR_SPACE_H_
