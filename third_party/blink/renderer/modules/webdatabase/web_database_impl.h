// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_WEB_DATABASE_IMPL_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_WEB_DATABASE_IMPL_H_

#include <stdint.h>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "third_party/blink/public/mojom/webdatabase/web_database.mojom-blink.h"

namespace blink {

// Receives database messages from the browser process and processes them on the
// IO thread.
class WebDatabaseImpl : public mojom::blink::WebDatabase {
 public:
  WebDatabaseImpl();
  ~WebDatabaseImpl() override;

  static void Create(mojo::PendingReceiver<mojom::blink::WebDatabase>);

 private:
  // blink::mojom::blink::Database
  void UpdateSize(const scoped_refptr<const SecurityOrigin>&,
                  const String& name,
                  int64_t size) override;
  void CloseImmediately(const scoped_refptr<const SecurityOrigin>&,
                        const String& name) override;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_WEB_DATABASE_IMPL_H_
