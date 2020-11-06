// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DIGITAL_GOODS_SERVICE_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DIGITAL_GOODS_SERVICE_H_

#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/digital_goods/digital_goods.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"

namespace blink {

class ScriptState;
class Visitor;

class DigitalGoodsService final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  explicit DigitalGoodsService(
      mojo::PendingRemote<payments::mojom::blink::DigitalGoods> pending_remote);
  ~DigitalGoodsService() override;

  // IDL Interface:
  ScriptPromise getDetails(ScriptState*, const Vector<String>& item_ids);
  ScriptPromise acknowledge(ScriptState*,
                            const String& purchase_token,
                            const String& purchase_type);
  ScriptPromise listPurchases(ScriptState*);

  void Trace(Visitor* visitor) const override;

 private:
  mojo::Remote<payments::mojom::blink::DigitalGoods> mojo_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_PAYMENTS_GOODS_DIGITAL_GOODS_SERVICE_H_
