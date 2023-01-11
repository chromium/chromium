// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_SERIALIZATION_HANDLER_H_
#define CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_SERIALIZATION_HANDLER_H_

#include <string>

#include "base/functional/callback.h"
#include "content/common/frame.mojom.h"

namespace content {

// Encapsulates pointers to the relevant callbacks that will be invoked
// throughout the serialization process, in response to messages coming from the
// renderer: |did_serialize_data_callback| will report each chunk of data that's
// being serialized, while |done_callback| will simply notify when the
// serialization process is finished.
class SavePackageSerializationHandler
    : public mojom::FrameHTMLSerializerHandler {
 public:
  using DidReceiveDataCallback =
      base::RepeatingCallback<void(const std::string&)>;
  using DoneCallback = base::OnceCallback<void()>;

  SavePackageSerializationHandler(
      const DidReceiveDataCallback& did_serialize_data_callback,
      DoneCallback done_callback);

  SavePackageSerializationHandler(const SavePackageSerializationHandler&) =
      delete;
  SavePackageSerializationHandler& operator=(
      const SavePackageSerializationHandler&) = delete;

  ~SavePackageSerializationHandler() override;

  // mojom::FrameHTMLSerializerHandler implementation:
  void DidReceiveData(const std::string& data_buffer) override;
  void Done() override;

 private:
  const DidReceiveDataCallback did_serialize_data_callback_;
  DoneCallback done_callback_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_SAVE_PACKAGE_SERIALIZATION_HANDLER_H_
