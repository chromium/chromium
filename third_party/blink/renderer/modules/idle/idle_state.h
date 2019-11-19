// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_STATE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_STATE_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/blink/public/mojom/idle/idle_manager.mojom-blink.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class IdleState final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit IdleState(mojom::blink::IdleStatePtr);

  ~IdleState() override;

  const mojom::blink::IdleState state() const;

  // IdleStatus IDL interface.
  String user() const;
  String screen() const;

 private:
  const mojom::blink::IdleStatePtr state_;

  DISALLOW_COPY_AND_ASSIGN(IdleState);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_IDLE_IDLE_STATE_H_
