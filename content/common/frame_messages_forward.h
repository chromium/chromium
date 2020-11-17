// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_FRAME_MESSAGES_FORWARD_H_
#define CONTENT_COMMON_FRAME_MESSAGES_FORWARD_H_

// Forward-declaration for FrameHostMsg_DidCommitProvisionalLoad_Params, which
// is typemapped to a Mojo [Native] type used by content::mojom::FrameHost. This
// means that the generated header for mojom::FrameHost requires (at least) a
// forward declaration of the legacy IPC struct type.
//
// Including content/common/frame_messages.h in the generated header for
// mojom::FrameHost is, however, not possible because legacy IPC struct type
// definition headers must be included exactly once in each translation unit
// using them, so any .cc files directly/indirectly including frame_messages.h
// could no longer include the generated header for mojom::FrameHost. Hence the
// generated header for mojom::FrameHost does not include frame_messages.h, but
// instead includes the forward-declaration below.
//
// TODO(https://crbug.com/729021): Eventually convert this legacy IPC struct to
// a proper Mojo type.
struct FrameHostMsg_DidCommitProvisionalLoad_Params;

#endif  // CONTENT_COMMON_FRAME_MESSAGES_FORWARD_H_
