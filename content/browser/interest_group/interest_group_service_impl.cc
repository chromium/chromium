// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_service_impl.h"

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"

namespace content {

namespace {

template <typename T>
T* DcheckNotNullAndReturn(T* ptr) {
  DCHECK_NE(nullptr, ptr);
  return ptr;
}

}  // namespace

InterestGroupServiceImpl::InterestGroupServiceImpl(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore> receiver)
    : FrameServiceBase(render_frame_host, std::move(receiver)) {}

// static
void InterestGroupServiceImpl::CreateMojoService(
    RenderFrameHost* render_frame_host,
    mojo::PendingReceiver<blink::mojom::RestrictedInterestGroupStore>
        receiver) {
  DCHECK(render_frame_host);

  // The object is bound to the lifetime of |render_frame_host| and the mojo
  // connection. See FrameServiceBase for details.
  new InterestGroupServiceImpl(render_frame_host, std::move(receiver));
}

void InterestGroupServiceImpl::JoinInterestGroup(
    blink::mojom::InterestGroupPtr group) {
  // TODO(crbug.com/1186444): Pass |group| to interest group store service.
}

void InterestGroupServiceImpl::LeaveInterestGroup(const url::Origin& owner,
                                                  const std::string& name) {
  // TODO(crbug.com/1186444): Pass |group| to interest group store service.
}

InterestGroupServiceImpl::~InterestGroupServiceImpl() = default;

}  // namespace content
