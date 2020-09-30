// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FAKE_LOCAL_FRAME_H_
#define CONTENT_PUBLIC_TEST_FAKE_LOCAL_FRAME_H_

#include "base/optional.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/associated_receiver_set.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/frame/frame.mojom.h"
#include "third_party/blink/public/mojom/frame/frame_owner_properties.mojom.h"
#include "third_party/blink/public/mojom/input/focus_type.mojom-forward.h"

namespace gfx {
class Point;
class Rect;
}

namespace content {

// This class implements a LocalFrame that can be attached to the
// AssociatedInterfaceProvider so that it will be called when the browser
// normally sends a request to the renderer process. But for a unittest
// setup it can be intercepted by this class.
class FakeLocalFrame : public blink::mojom::LocalFrame {
 public:
  FakeLocalFrame();
  ~FakeLocalFrame() override;

  void Init(blink::AssociatedInterfaceProvider* provider);

  // blink::mojom::LocalFrame:
  void GetTextSurroundingSelection(
      uint32_t max_length,
      GetTextSurroundingSelectionCallback callback) override;
  void SendInterventionReport(const std::string& id,
                              const std::string& message) override;
  void SetFrameOwnerProperties(
      blink::mojom::FrameOwnerPropertiesPtr properties) override;
  void NotifyUserActivation(
      blink::mojom::UserActivationNotificationType notification_type) override;
  void NotifyVirtualKeyboardOverlayRect(const gfx::Rect&) override;
  void AddMessageToConsole(blink::mojom::ConsoleMessageLevel level,
                           const std::string& message,
                           bool discard_duplicates) override;
  void AddInspectorIssue(blink::mojom::InspectorIssueInfoPtr info) override;
  void CheckCompleted() override;
  void StopLoading() override;
  void Collapse(bool collapsed) override;
  void EnableViewSourceMode() override;
  void Focus() override;
  void ClearFocusedElement() override;
  void GetResourceSnapshotForWebBundle(
      mojo::PendingReceiver<data_decoder::mojom::ResourceSnapshotForWebBundle>
          receiver) override;
  void CopyImageAt(const gfx::Point& window_point) override;
  void SaveImageAt(const gfx::Point& window_point) override;
  void ReportBlinkFeatureUsage(
      const std::vector<blink::mojom::WebFeature>&) override;
  void RenderFallbackContent() override;
  void BeforeUnload(bool is_reload, BeforeUnloadCallback callback) override;
  void MediaPlayerActionAt(const gfx::Point& location,
                           blink::mojom::MediaPlayerActionPtr action) override;
  void AdvanceFocusInFrame(blink::mojom::FocusType focus_type,
                           const base::Optional<base::UnguessableToken>&
                               source_frame_token) override;
  void AdvanceFocusInForm(blink::mojom::FocusType focus_type) override;
  void ReportContentSecurityPolicyViolation(
      network::mojom::CSPViolationPtr violation) override;
  void DidUpdateFramePolicy(const blink::FramePolicy& frame_policy) override;
  void OnScreensChange() override;
  void PostMessageEvent(
      const base::Optional<base::UnguessableToken>& source_frame_token,
      const base::string16& source_origin,
      const base::string16& target_origin,
      blink::TransferableMessage message) override;
  void GetSavableResourceLinks(
      GetSavableResourceLinksCallback callback) override;
#if defined(OS_MAC)
  void GetCharacterIndexAtPoint(const gfx::Point& point) override;
  void GetFirstRectForRange(const gfx::Range& range) override;
  void GetStringForRange(const gfx::Range& range,
                         GetStringForRangeCallback callback) override;
#endif
  void BindReportingObserver(
      mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) override;
  void UpdateOpener(const base::Optional<base::UnguessableToken>&
                        opener_frame_token) override;

 private:
  void BindFrameHostReceiver(mojo::ScopedInterfaceEndpointHandle handle);

  mojo::AssociatedReceiver<blink::mojom::LocalFrame> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FAKE_LOCAL_FRAME_H_
