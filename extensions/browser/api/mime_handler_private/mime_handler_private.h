// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MIME_HANDLER_PRIVATE_MIME_HANDLER_PRIVATE_H_
#define EXTENSIONS_BROWSER_API_MIME_HANDLER_PRIVATE_MIME_HANDLER_PRIVATE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/api/mime_handler.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace extensions {
class StreamContainer;
class MimeHandlerServiceImplTest;

class MimeHandlerServiceImpl : public mime_handler::MimeHandlerService {
 public:
  explicit MimeHandlerServiceImpl(
      base::WeakPtr<StreamContainer> stream_container);
  ~MimeHandlerServiceImpl() override;

  static void Create(
      base::WeakPtr<StreamContainer> stream_container,
      mojo::PendingReceiver<mime_handler::MimeHandlerService> receiver);

 private:
  friend class MimeHandlerServiceImplTest;

  // mime_handler::MimeHandlerService overrides.
  void GetStreamInfo(GetStreamInfoCallback callback) override;

  // A handle to the stream being handled by the MimeHandlerViewGuest.
  base::WeakPtr<StreamContainer> stream_;

  base::WeakPtrFactory<MimeHandlerServiceImpl> weak_factory_{this};
};

}  // namespace extensions

namespace mojo {

template <>
struct TypeConverter<extensions::mime_handler::StreamInfoPtr,
                     extensions::StreamContainer> {
  static extensions::mime_handler::StreamInfoPtr Convert(
      const extensions::StreamContainer& stream);
};

}  // namespace mojo

#endif  // EXTENSIONS_BROWSER_API_MIME_HANDLER_PRIVATE_MIME_HANDLER_PRIVATE_H_
