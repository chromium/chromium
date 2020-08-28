// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_
#define CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/browser/global_routing_id.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/mojom/font_access/font_access.mojom.h"
#include "url/origin.h"

namespace content {

class CONTENT_EXPORT FontAccessManagerImpl
    : public blink::mojom::FontAccessManager {
 public:
  FontAccessManagerImpl();
  ~FontAccessManagerImpl() override;

  struct BindingContext {
    BindingContext(const url::Origin& origin, GlobalFrameRoutingId frame_id)
        : origin(origin), frame_id(frame_id) {}

    url::Origin origin;
    GlobalFrameRoutingId frame_id;
  };

  void BindReceiver(
      const BindingContext& context,
      mojo::PendingReceiver<blink::mojom::FontAccessManager> receiver);

  // blink.mojom.FontAccessManager:
#if defined(OS_MAC)
  // TODO(crbug.com/1119575): Remove this IPC method. It is there due to
  // the Mac enumeration implementation being done renderer-side and only
  // the permission request being needed browser-side.
  void RequestPermission(RequestPermissionCallback callback) override;
#endif
  void EnumerateLocalFonts(EnumerateLocalFontsCallback callback) override;

 private:
  void DidRequestPermission(EnumerateLocalFontsCallback callback,
                            blink::mojom::PermissionStatus status);
  // Registered clients.
  mojo::ReceiverSet<blink::mojom::FontAccessManager, BindingContext> receivers_;
  scoped_refptr<base::SequencedTaskRunner> ipc_task_runner_;
  scoped_refptr<base::TaskRunner> results_task_runner_;

  SEQUENCE_CHECKER(sequence_checker_);
  DISALLOW_COPY_AND_ASSIGN(FontAccessManagerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FONT_ACCESS_FONT_ACCESS_MANAGER_IMPL_H_
