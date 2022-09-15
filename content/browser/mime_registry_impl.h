// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MIME_REGISTRY_IMPL_H_
#define CONTENT_BROWSER_MIME_REGISTRY_IMPL_H_

#include "base/sequence_checker.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/mime/mime_registry.mojom.h"

namespace content {

class MimeRegistryImpl : public blink::mojom::MimeRegistry {
 public:
  MimeRegistryImpl();

  MimeRegistryImpl(const MimeRegistryImpl&) = delete;
  MimeRegistryImpl& operator=(const MimeRegistryImpl&) = delete;

  ~MimeRegistryImpl() override;

  static void Create(
      mojo::PendingReceiver<blink::mojom::MimeRegistry> receiver);

 private:
  void GetMimeTypeFromExtension(
      const std::string& extension,
      GetMimeTypeFromExtensionCallback callback) override;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_MIME_REGISTRY_IMPL_H_
