// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_IMAGE_REPLACEMENT_IMAGE_REPLACEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_IMAGE_REPLACEMENT_IMAGE_REPLACEMENT_H_

#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "third_party/blink/public/mojom/image_replacement/image_replacement.mojom-blink.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_receiver.h"
#include "third_party/blink/renderer/platform/mojo/heap_mojo_remote.h"

namespace blink {
class Document;
class HTMLImageElement;
class ShadowRoot;

// Manages the replacement of primary content in an HTMLImageElement with
// remote content hosted in an iframe.
class CORE_EXPORT ImageReplacement : public GarbageCollected<ImageReplacement>,
                                     public mojom::blink::ImageReplacement {
 public:
  // Creates a handle for image replacement. If a handle has previously been
  // created (and hasn't been discarded/disconnected), this method will fail.
  // Note: This doesn't actually start replacement; the caller can connect to
  // returned remote and call StartReplacement() to do so.
  static base::expected<mojo::PendingRemote<mojom::blink::ImageReplacement>,
                        String>
  CreateAndBindReceiver(HTMLImageElement& image_element);
  explicit ImageReplacement(base::PassKey<ImageReplacement>,
                            HTMLImageElement& image_element);

  // Looks up the image replacement for `image_element` in `document`,
  // unregisters it, and resets the mojo connections.
  static void ResetImageReplacement(base::PassKey<HTMLImageElement>,
                                    HTMLImageElement& image_element,
                                    Document& document);
  // Creates the shadow tree in `html_image_element` for the image replacement.
  static void CreateImageReplacementShadowTree(base::PassKey<HTMLImageElement>,
                                               HTMLImageElement& image_element);

  void Trace(Visitor*) const;

 private:
  // mojom::blink::ImageReplacement:
  void StartReplacement(mojo::PendingRemote<mojom::blink::ImageReplacementHost>
                            host_remote) override;

  mojo::PendingRemote<mojom::blink::ImageReplacement> BindReceiver();
  void Reset(Document& document);

 private:
  void OnDisconnect();

  Member<HTMLImageElement> image_element_;
  HeapMojoReceiver<mojom::blink::ImageReplacement, ImageReplacement> receiver_;
  HeapMojoRemote<mojom::blink::ImageReplacementHost> host_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_IMAGE_REPLACEMENT_IMAGE_REPLACEMENT_H_
