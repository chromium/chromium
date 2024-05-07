// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_CONTEXT_MENU_INTERCEPTOR_H_
#define CONTENT_PUBLIC_TEST_CONTEXT_MENU_INTERCEPTOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "third_party/blink/public/common/context_menu_data/untrustworthy_context_menu_params.h"
#include "third_party/blink/public/mojom/frame/frame.mojom-test-utils.h"

namespace content {

class RenderFrameHost;

// This class intercepts for ShowContextMenu Mojo method called from a
// renderer process, and allows observing the UntrustworthyContextMenuParams as
// sent by the renderer.
class ContextMenuInterceptor
    : public blink::mojom::LocalFrameHostInterceptorForTesting {
 public:
  // Whether or not the ContextMenu should be prevented from performing
  // its default action, preventing the context menu from showing.
  enum ShowBehavior { kShow, kPreventShow };

  explicit ContextMenuInterceptor(content::RenderFrameHost* render_frame_host,
                                  ShowBehavior behavior = ShowBehavior::kShow);
  ContextMenuInterceptor(const ContextMenuInterceptor&) = delete;
  ContextMenuInterceptor& operator=(const ContextMenuInterceptor&) = delete;
  ~ContextMenuInterceptor() override;

  blink::mojom::LocalFrameHost* GetForwardingInterface() override;

  void ShowContextMenu(
      mojo::PendingAssociatedRemote<blink::mojom::ContextMenuClient>
          context_menu_client,
      const blink::UntrustworthyContextMenuParams& params) override;

  void Wait();
  void Reset();

  blink::UntrustworthyContextMenuParams get_params() { return last_params_; }

 private:
  mojo::test::ScopedSwapImplForTesting<blink::mojom::LocalFrameHost>
      swapped_impl_;
  std::unique_ptr<base::RunLoop> run_loop_;
  base::OnceClosure quit_closure_;
  blink::UntrustworthyContextMenuParams last_params_;
  const ShowBehavior show_behavior_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_CONTEXT_MENU_INTERCEPTOR_H_
