// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_

#include "base/memory/scoped_refptr.h"
#include "base/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ContentDescription;
class ScriptPromiseResolver;
class ScriptState;
class ServiceWorkerRegistration;

class ContentIndex final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ContentIndex(ServiceWorkerRegistration* registration,
               scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ContentIndex() override;

  // Web-exposed function defined in the IDL file.
  ScriptPromise add(ScriptState* script_state,
                    const ContentDescription* description);
  ScriptPromise deleteDescription(ScriptState* script_state, const String& id);
  ScriptPromise getDescriptions(ScriptState* script_state);

  void Trace(blink::Visitor* visitor) override;

 private:
  mojom::blink::ContentIndexService* GetService();

  // Callbacks.
  void DidGetIconSizes(ScriptPromiseResolver* resolver,
                       mojom::blink::ContentDescriptionPtr description,
                       const Vector<WebSize>& icon_sizes);
  void DidGetIcons(ScriptPromiseResolver* resolver,
                   mojom::blink::ContentDescriptionPtr description,
                   Vector<SkBitmap> icons);
  void DidAdd(ScriptPromiseResolver* resolver,
              mojom::blink::ContentIndexError error);
  void DidDeleteDescription(ScriptPromiseResolver* resolver,
                            mojom::blink::ContentIndexError error);
  void DidGetDescriptions(
      ScriptPromiseResolver* resolver,
      mojom::blink::ContentIndexError error,
      Vector<mojom::blink::ContentDescriptionPtr> descriptions);

  Member<ServiceWorkerRegistration> registration_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  mojo::Remote<mojom::blink::ContentIndexService> content_index_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_
