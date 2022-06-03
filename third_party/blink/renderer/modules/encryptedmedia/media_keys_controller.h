// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_MEDIA_KEYS_CONTROLLER_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_MEDIA_KEYS_CONTROLLER_H_

#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class ExecutionContext;
class WebEncryptedMediaClient;

class MODULES_EXPORT MediaKeysController final
    : public GarbageCollected<MediaKeysController>,
      public Supplement<Page> {
 public:
  static const char kSupplementName[];

  WebEncryptedMediaClient* EncryptedMediaClient(ExecutionContext*);

  static void ProvideMediaKeysTo(Page&);
  static MediaKeysController* From(Page* page) {
    return Supplement<Page>::From<MediaKeysController>(page);
  }

  MediaKeysController();

  void Trace(Visitor* visitor) const override {
    Supplement<Page>::Trace(visitor);
  }
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_ENCRYPTEDMEDIA_MEDIA_KEYS_CONTROLLER_H_
