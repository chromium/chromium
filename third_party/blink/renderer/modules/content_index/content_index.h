// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/public/mojom/content_index/content_index.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_wrapper_mode.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"

namespace blink {

class ContentDescription;
class ExceptionState;
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
                    const ContentDescription* description,
                    ExceptionState& exception_state);
  ScriptPromise deleteDescription(ScriptState* script_state,
                                  const String& id,
                                  ExceptionState& exception_state);
  ScriptPromise getDescriptions(ScriptState* script_state,
                                ExceptionState& exception_state);

  void Trace(Visitor* visitor) const override;

 private:
  mojom::blink::ContentIndexService* GetService();

  // Callbacks.
  void DidGetIconSizes(mojom::blink::ContentDescriptionPtr description,
                       ScriptPromiseResolver* resolver,
                       const Vector<gfx::Size>& icon_sizes);
  void DidGetIcons(ScriptPromiseResolver* resolver,
                   mojom::blink::ContentDescriptionPtr description,
                   Vector<SkBitmap> icons);
  void DidCheckOfflineCapability(
      KURL launch_url,
      mojom::blink::ContentDescriptionPtr description,
      Vector<SkBitmap> icons,
      ScriptPromiseResolver* resolver,
      bool is_offline_capable);
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
  HeapMojoRemote<mojom::blink::ContentIndexService> content_index_service_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_CONTENT_INDEX_CONTENT_INDEX_H_
