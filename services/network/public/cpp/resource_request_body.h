// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_
#define SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/cpp/data_element.h"
#include "services/network/public/mojom/url_loader.mojom-shared.h"
#include "url/gurl.h"

namespace blink {
namespace mojom {
class FetchAPIRequestBodyDataView;
}  // namespace mojom
}  // namespace blink

namespace network {

// ResourceRequestBody represents body (i.e. upload data) of a HTTP request.
class COMPONENT_EXPORT(NETWORK_CPP_BASE) ResourceRequestBody
    : public base::RefCountedThreadSafe<ResourceRequestBody> {
 public:
  ResourceRequestBody();

  // Creates ResourceRequestBody that holds a copy of |bytes|.
  static scoped_refptr<ResourceRequestBody> CreateFromBytes(const char* bytes,
                                                            size_t length);

  void AppendBytes(std::vector<uint8_t> bytes);
  void AppendBytes(const char* bytes, int bytes_len);
  void AppendFileRange(const base::FilePath& file_path,
                       uint64_t offset,
                       uint64_t length,
                       const base::Time& expected_modification_time);
  // Appends the specified part of |file|. If |length| extends beyond the end of
  // the file, it will be set to the end of the file.
  void AppendRawFileRange(base::File file,
                          const base::FilePath& file_path,
                          uint64_t offset,
                          uint64_t length,
                          const base::Time& expected_modification_time);

  // Appends a blob. If the 2-parameter version is used, the resulting body can
  // be read by Blink, which is needed when the body is sent to Blink, e.g., for
  // service worker interception. The length must be size of the entire blob,
  // not a subrange of it. If the length is unknown, use the 1-parameter
  // version, but this means the body/blob won't be readable by Blink (that's OK
  // if this ResourceRequestBody will only be sent to the browser process and
  // won't be sent to Blink).
  //
  // TODO(crbug.com/846167): Remove these functions when NetworkService is
  // enabled, as blobs are passed via AppendDataPipe in that case.
  void AppendBlob(const std::string& uuid);
  void AppendBlob(const std::string& uuid, uint64_t length);

  void AppendDataPipe(
      mojo::PendingRemote<mojom::DataPipeGetter> data_pipe_getter);

  // |chunked_data_pipe_getter| will provide the upload body for a chunked
  // upload. Unlike the other methods, which support concatenating data of
  // various types, when this is called, |chunked_data_pipe_getter| will provide
  // the entire response body, and other types of data may not added when
  // sending chunked data.
  //
  // It's unclear how widespread support for chunked uploads is, since there are
  // no web APIs that send uploads with unknown request body sizes, so this
  // method should only be used when talking to servers that are are known to
  // support chunked uploads.
  void SetToChunkedDataPipe(mojo::PendingRemote<mojom::ChunkedDataPipeGetter>
                                chunked_data_pipe_getter);

  const std::vector<DataElement>* elements() const { return &elements_; }
  std::vector<DataElement>* elements_mutable() { return &elements_; }
  void swap_elements(std::vector<DataElement>* elements) {
    elements_.swap(*elements);
  }

  // Identifies a particular upload instance, which is used by the cache to
  // formulate a cache key.  This value should be unique across browser
  // sessions.  A value of 0 is used to indicate an unspecified identifier.
  void set_identifier(int64_t id) { identifier_ = id; }
  int64_t identifier() const { return identifier_; }

  // Returns paths referred to by |elements| of type
  // network::mojom::DataElementType::kFile.
  std::vector<base::FilePath> GetReferencedFiles() const;

  // Sets the flag which indicates whether the post data contains sensitive
  // information like passwords.
  void set_contains_sensitive_info(bool contains_sensitive_info) {
    contains_sensitive_info_ = contains_sensitive_info;
  }
  bool contains_sensitive_info() const { return contains_sensitive_info_; }

 private:
  friend class base::RefCountedThreadSafe<ResourceRequestBody>;
  friend struct mojo::StructTraits<blink::mojom::FetchAPIRequestBodyDataView,
                                   scoped_refptr<network::ResourceRequestBody>>;
  friend struct mojo::StructTraits<network::mojom::URLRequestBodyDataView,
                                   scoped_refptr<network::ResourceRequestBody>>;
  ~ResourceRequestBody();

  std::vector<DataElement> elements_;
  int64_t identifier_;

  bool contains_sensitive_info_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestBody);
};

}  // namespace network

#endif  // SERVICES_NETWORK_PUBLIC_CPP_RESOURCE_REQUEST_BODY_H_
