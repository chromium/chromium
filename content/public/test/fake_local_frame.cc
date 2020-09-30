// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/fake_local_frame.h"

#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "third_party/blink/public/mojom/frame/media_player_action.mojom.h"

namespace content {

FakeLocalFrame::FakeLocalFrame() {}

FakeLocalFrame::~FakeLocalFrame() {}

void FakeLocalFrame::Init(blink::AssociatedInterfaceProvider* provider) {
  provider->OverrideBinderForTesting(
      blink::mojom::LocalFrame::Name_,
      base::BindRepeating(&FakeLocalFrame::BindFrameHostReceiver,
                          base::Unretained(this)));
}

void FakeLocalFrame::GetTextSurroundingSelection(
    uint32_t max_length,
    GetTextSurroundingSelectionCallback callback) {
  std::move(callback).Run(base::string16(), 0, 0);
}

void FakeLocalFrame::SendInterventionReport(const std::string& id,
                                            const std::string& message) {}

void FakeLocalFrame::SetFrameOwnerProperties(
    blink::mojom::FrameOwnerPropertiesPtr properties) {}

void FakeLocalFrame::NotifyUserActivation(
    blink::mojom::UserActivationNotificationType notification_type) {}

void FakeLocalFrame::NotifyVirtualKeyboardOverlayRect(const gfx::Rect&) {}

void FakeLocalFrame::AddMessageToConsole(
    blink::mojom::ConsoleMessageLevel level,
    const std::string& message,
    bool discard_duplicates) {}

void FakeLocalFrame::AddInspectorIssue(
    blink::mojom::InspectorIssueInfoPtr info) {}

void FakeLocalFrame::CheckCompleted() {}

void FakeLocalFrame::StopLoading() {}

void FakeLocalFrame::Collapse(bool collapsed) {}

void FakeLocalFrame::EnableViewSourceMode() {}

void FakeLocalFrame::Focus() {}

void FakeLocalFrame::ClearFocusedElement() {}

void FakeLocalFrame::GetResourceSnapshotForWebBundle(
    mojo::PendingReceiver<data_decoder::mojom::ResourceSnapshotForWebBundle>
        receiver) {}

void FakeLocalFrame::CopyImageAt(const gfx::Point& window_point) {}

void FakeLocalFrame::SaveImageAt(const gfx::Point& window_point) {}

void FakeLocalFrame::ReportBlinkFeatureUsage(
    const std::vector<blink::mojom::WebFeature>&) {}

void FakeLocalFrame::RenderFallbackContent() {}

void FakeLocalFrame::BeforeUnload(bool is_reload,
                                  BeforeUnloadCallback callback) {
  base::TimeTicks now = base::TimeTicks::Now();
  std::move(callback).Run(true /*leave the page*/, now, now);
}

void FakeLocalFrame::MediaPlayerActionAt(
    const gfx::Point& location,
    blink::mojom::MediaPlayerActionPtr action) {}

void FakeLocalFrame::AdvanceFocusInFrame(
    blink::mojom::FocusType focus_type,
    const base::Optional<base::UnguessableToken>& source_frame_token) {}

void FakeLocalFrame::AdvanceFocusInForm(blink::mojom::FocusType focus_type) {}

void FakeLocalFrame::ReportContentSecurityPolicyViolation(
    network::mojom::CSPViolationPtr violation) {}

void FakeLocalFrame::DidUpdateFramePolicy(
    const blink::FramePolicy& frame_policy) {}

void FakeLocalFrame::OnScreensChange() {}

void FakeLocalFrame::PostMessageEvent(
    const base::Optional<base::UnguessableToken>& source_frame_token,
    const base::string16& source_origin,
    const base::string16& target_origin,
    blink::TransferableMessage message) {}

void FakeLocalFrame::GetSavableResourceLinks(
    GetSavableResourceLinksCallback callback) {}

#if defined(OS_MAC)
void FakeLocalFrame::GetCharacterIndexAtPoint(const gfx::Point& point) {}
void FakeLocalFrame::GetFirstRectForRange(const gfx::Range& range) {}
void FakeLocalFrame::GetStringForRange(const gfx::Range& range,
                                       GetStringForRangeCallback callback) {
  std::move(callback).Run(nullptr, gfx::Point());
}
#endif

void FakeLocalFrame::BindReportingObserver(
    mojo::PendingReceiver<blink::mojom::ReportingObserver> receiver) {}

void FakeLocalFrame::UpdateOpener(
    const base::Optional<base::UnguessableToken>& opener_frame_token) {}

void FakeLocalFrame::BindFrameHostReceiver(
    mojo::ScopedInterfaceEndpointHandle handle) {
  receiver_.Bind(mojo::PendingAssociatedReceiver<blink::mojom::LocalFrame>(
      std::move(handle)));
}

}  // namespace content
