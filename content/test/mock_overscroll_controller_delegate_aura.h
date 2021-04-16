// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_MOCK_OVERSCROLL_CONTROLLER_DELEGATE_AURA_H_
#define CONTENT_TEST_MOCK_OVERSCROLL_CONTROLLER_DELEGATE_AURA_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/browser/renderer_host/overscroll_controller_delegate.h"
#include "content/test/mock_overscroll_observer.h"

namespace content {

class MessageLoopRunner;
class RenderWidgetHostViewAura;

// Receives overscroll gesture updates from the aura overscroll controller.
class MockOverscrollControllerDelegateAura
    : public OverscrollControllerDelegate,
      public MockOverscrollObserver {
 public:
  MockOverscrollControllerDelegateAura(RenderWidgetHostViewAura* rwhva);
  ~MockOverscrollControllerDelegateAura() override;

  // OverscrollControllerDelegate:
  gfx::Size GetDisplaySize() const override;
  base::Optional<float> GetMaxOverscrollDelta() const override;
  bool OnOverscrollUpdate(float, float) override;
  void OnOverscrollComplete(OverscrollMode) override;
  void OnOverscrollModeChange(OverscrollMode old_mode,
                              OverscrollMode new_mode,
                              OverscrollSource source,
                              cc::OverscrollBehavior behavior) override;

  // MockOverscrollObserver:
  void WaitForUpdate() override;
  void WaitForEnd() override;
  void Reset() override;

 private:
  void OnOverscrollEnd();

  RenderWidgetHostViewAura* rwhva_;
  scoped_refptr<MessageLoopRunner> update_message_loop_runner_;
  scoped_refptr<MessageLoopRunner> end_message_loop_runner_;
  bool seen_update_;
  bool overscroll_ended_;
  DISALLOW_COPY_AND_ASSIGN(MockOverscrollControllerDelegateAura);
};

}  // namespace content

#endif  // CONTENT_TEST_MOCK_OVERSCROLL_CONTROLLER_DELEGATE_AURA_H_
